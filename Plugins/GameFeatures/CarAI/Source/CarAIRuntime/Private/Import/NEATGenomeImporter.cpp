#include "Import/NEATGenomeImporter.h"
#include "NN/NEATGenomeTypes.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================
// LoadFromFile
// ============================================================================

bool UNEATGenomeImporter::LoadFromFile(const FString& FilePath, FNEATGenome& OutGenome)
{
	OutGenome = FNEATGenome();
	OutGenome.bIsValid = false;

	FString AbsolutePath = FPaths::ConvertRelativePathToFull(FilePath);
	if (!FPaths::FileExists(AbsolutePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: File not found: %s"), *AbsolutePath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *AbsolutePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Could not read file: %s"), *AbsolutePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Invalid JSON in: %s"), *AbsolutePath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATGenomeImporter] Parsing genome from: %s"), *AbsolutePath);

	// Required scalar fields
	if (!Root->HasField(TEXT("genome_id")) || !Root->HasField(TEXT("generation")) || !Root->HasField(TEXT("num_inputs")) || !Root->HasField(TEXT("num_outputs")))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Missing required field (genome_id, generation, num_inputs, num_outputs) in: %s"), *AbsolutePath);
		return false;
	}

	OutGenome.GenomeID = Root->GetIntegerField(TEXT("genome_id"));
	OutGenome.Generation = Root->GetIntegerField(TEXT("generation"));
	{
		const TSharedPtr<FJsonValue> FitnessField = Root->TryGetField(TEXT("fitness"));
		OutGenome.Fitness = (FitnessField && FitnessField->Type != EJson::Null) ? (float)FitnessField->AsNumber() : 0.f;
	}
	OutGenome.NumInputs = Root->GetIntegerField(TEXT("num_inputs"));
	OutGenome.NumOutputs = Root->GetIntegerField(TEXT("num_outputs"));

	if (OutGenome.NumInputs < 0 || OutGenome.NumOutputs < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: num_inputs (%d) or num_outputs (%d) negative in: %s"),
			OutGenome.NumInputs, OutGenome.NumOutputs, *AbsolutePath);
		return false;
	}

	// Nodes array
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Missing 'nodes' array in: %s"), *AbsolutePath);
		return false;
	}

	TSet<int32> SeenNodeIds;
	for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
	{
		const TSharedPtr<FJsonObject>* NodeObj = nullptr;
		if (!NodeVal->TryGetObject(NodeObj) || !NodeObj->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Invalid node entry in 'nodes' array in: %s"), *AbsolutePath);
			return false;
		}

		FNEATNode Node;
		Node.NodeId = (*NodeObj)->GetIntegerField(TEXT("id"));
		FString ActivationStr = (*NodeObj)->GetStringField(TEXT("activation"));
		Node.Activation = NEATActivationFromString(ActivationStr);
		if (Node.Activation == ENEATActivation::Invalid)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Unsupported activation '%s' for node %d in: %s. Supported: sigmoid, tanh, relu."),
				*ActivationStr, Node.NodeId, *AbsolutePath);
			return false;
		}
		Node.Bias = (*NodeObj)->HasField(TEXT("bias")) ? (*NodeObj)->GetNumberField(TEXT("bias")) : 0.f;
		Node.Response = (*NodeObj)->HasField(TEXT("response")) ? (*NodeObj)->GetNumberField(TEXT("response")) : 1.f;

		if (SeenNodeIds.Contains(Node.NodeId))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Duplicate node ID %d in: %s"), Node.NodeId, *AbsolutePath);
			return false;
		}
		SeenNodeIds.Add(Node.NodeId);
		OutGenome.Nodes.Add(Node);
	}

	// Connections array
	const TArray<TSharedPtr<FJsonValue>>* ConnArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("connections"), ConnArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Missing 'connections' array in: %s"), *AbsolutePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ConnVal : *ConnArray)
	{
		const TSharedPtr<FJsonObject>* ConnObj = nullptr;
		if (!ConnVal->TryGetObject(ConnObj) || !ConnObj->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Invalid connection entry in: %s"), *AbsolutePath);
			return false;
		}

		FNEATConnection Conn;
		Conn.InNode = (*ConnObj)->GetIntegerField(TEXT("in_node"));
		Conn.OutNode = (*ConnObj)->GetIntegerField(TEXT("out_node"));
		Conn.Weight = (*ConnObj)->GetNumberField(TEXT("weight"));
		Conn.bEnabled = (*ConnObj)->HasField(TEXT("enabled")) ? (*ConnObj)->GetBoolField(TEXT("enabled")) : true;

		if (!SeenNodeIds.Contains(Conn.InNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Connection references missing in_node %d in: %s"), Conn.InNode, *AbsolutePath);
			return false;
		}
		if (!SeenNodeIds.Contains(Conn.OutNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] FAIL: Connection references missing out_node %d in: %s"), Conn.OutNode, *AbsolutePath);
			return false;
		}

		OutGenome.Connections.Add(Conn);
	}

	UE_LOG(LogTemp, Log, TEXT("[NEATGenomeImporter] Parsed genome_id=%d nodes=%d connections=%d; validating structure."),
		OutGenome.GenomeID, OutGenome.Nodes.Num(), OutGenome.Connections.Num());
	return ValidateGenome(OutGenome);
}

// ============================================================================
// ValidateGenome
// ============================================================================

bool UNEATGenomeImporter::ValidateGenome(FNEATGenome& InOutGenome)
{
	InOutGenome.bIsValid = false;

	const int32 ExpectedInputs = InOutGenome.NumInputs;
	const int32 ExpectedOutputs = InOutGenome.NumOutputs;

	if (ExpectedInputs <= 0 || ExpectedOutputs <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] VALIDATION FAIL: Genome %d: num_inputs=%d num_outputs=%d (must be positive)"),
			InOutGenome.GenomeID, ExpectedInputs, ExpectedOutputs);
		return false;
	}

	TSet<int32> NodeIds;
	for (const FNEATNode& N : InOutGenome.Nodes)
	{
		NodeIds.Add(N.NodeId);
	}

	// NEAT convention: input node IDs -1..-NumInputs (observation[i] -> node -(i+1))
	for (int32 i = 0; i < ExpectedInputs; ++i)
	{
		const int32 InputNodeId = -(i + 1);
		if (!NodeIds.Contains(InputNodeId))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] VALIDATION FAIL: Genome %d: missing input node %d (expected -1..-%d)"),
				InOutGenome.GenomeID, InputNodeId, ExpectedInputs);
			return false;
		}
	}

	// NEAT convention: output node IDs 0..NumOutputs-1
	for (int32 i = 0; i < ExpectedOutputs; ++i)
	{
		if (!NodeIds.Contains(i))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] VALIDATION FAIL: Genome %d: missing output node %d (expected 0..%d)"),
				InOutGenome.GenomeID, i, ExpectedOutputs - 1);
			return false;
		}
	}

	// All connection endpoints must reference existing nodes
	for (const FNEATConnection& C : InOutGenome.Connections)
	{
		if (!NodeIds.Contains(C.InNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] VALIDATION FAIL: Genome %d: connection in_node %d not in nodes"), InOutGenome.GenomeID, C.InNode);
			return false;
		}
		if (!NodeIds.Contains(C.OutNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] VALIDATION FAIL: Genome %d: connection out_node %d not in nodes"), InOutGenome.GenomeID, C.OutNode);
			return false;
		}
	}

	int32 DisabledCount = 0;
	for (const FNEATConnection& C : InOutGenome.Connections)
	{
		if (!C.bEnabled) DisabledCount++;
	}
	if (DisabledCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[NEATGenomeImporter] Genome %d: %d disabled connections (excluded from evaluation)"), InOutGenome.GenomeID, DisabledCount);
	}

	InOutGenome.bIsValid = true;
	UE_LOG(LogTemp, Log, TEXT("[NEATGenomeImporter] Genome %d validated: nodes=%d connections=%d (enabled=%d) inputs=%d outputs=%d"),
		InOutGenome.GenomeID, InOutGenome.Nodes.Num(), InOutGenome.Connections.Num(), InOutGenome.Connections.Num() - DisabledCount,
		InOutGenome.NumInputs, InOutGenome.NumOutputs);
	return true;
}
