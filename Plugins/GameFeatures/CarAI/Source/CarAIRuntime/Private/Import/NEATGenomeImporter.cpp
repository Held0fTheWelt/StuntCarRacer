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
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] File not found: %s"), *AbsolutePath);
		return false;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *AbsolutePath))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Failed to read file: %s"), *AbsolutePath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Invalid JSON in: %s"), *AbsolutePath);
		return false;
	}

	// Required scalar fields
	if (!Root->HasField(TEXT("genome_id")) || !Root->HasField(TEXT("generation")) || !Root->HasField(TEXT("num_inputs")) || !Root->HasField(TEXT("num_outputs")))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Missing required field (genome_id, generation, num_inputs, num_outputs) in: %s"), *AbsolutePath);
		return false;
	}

	OutGenome.GenomeID = Root->GetIntegerField(TEXT("genome_id"));
	OutGenome.Generation = Root->GetIntegerField(TEXT("generation"));
	OutGenome.Fitness = Root->HasField(TEXT("fitness")) ? Root->GetNumberField(TEXT("fitness")) : 0.f;
	OutGenome.NumInputs = Root->GetIntegerField(TEXT("num_inputs"));
	OutGenome.NumOutputs = Root->GetIntegerField(TEXT("num_outputs"));

	if (OutGenome.NumInputs < 0 || OutGenome.NumOutputs < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] num_inputs (%d) or num_outputs (%d) negative in: %s"),
			OutGenome.NumInputs, OutGenome.NumOutputs, *AbsolutePath);
		return false;
	}

	// Nodes
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("nodes"), NodesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Missing 'nodes' array in: %s"), *AbsolutePath);
		return false;
	}

	TSet<int32> SeenNodeIds;
	for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
	{
		const TSharedPtr<FJsonObject>* NodeObj = nullptr;
		if (!NodeVal->TryGetObject(NodeObj) || !NodeObj->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Invalid node entry in 'nodes' array in: %s"), *AbsolutePath);
			return false;
		}

		FNEATNode Node;
		Node.NodeId = (*NodeObj)->GetIntegerField(TEXT("id"));
		FString ActivationStr = (*NodeObj)->GetStringField(TEXT("activation"));
		Node.Activation = NEATActivationFromString(ActivationStr);
		if (Node.Activation == ENEATActivation::Invalid)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Unsupported activation '%s' for node %d in: %s. Supported: sigmoid, tanh, relu."),
				*ActivationStr, Node.NodeId, *AbsolutePath);
			return false;
		}
		Node.Bias = (*NodeObj)->HasField(TEXT("bias")) ? (*NodeObj)->GetNumberField(TEXT("bias")) : 0.f;
		Node.Response = (*NodeObj)->HasField(TEXT("response")) ? (*NodeObj)->GetNumberField(TEXT("response")) : 1.f;

		if (SeenNodeIds.Contains(Node.NodeId))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Duplicate node ID %d in: %s"), Node.NodeId, *AbsolutePath);
			return false;
		}
		SeenNodeIds.Add(Node.NodeId);
		OutGenome.Nodes.Add(Node);
	}

	// Connections
	const TArray<TSharedPtr<FJsonValue>>* ConnArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("connections"), ConnArray))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Missing 'connections' array in: %s"), *AbsolutePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ConnVal : *ConnArray)
	{
		const TSharedPtr<FJsonObject>* ConnObj = nullptr;
		if (!ConnVal->TryGetObject(ConnObj) || !ConnObj->IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Invalid connection entry in: %s"), *AbsolutePath);
			return false;
		}

		FNEATConnection Conn;
		Conn.InNode = (*ConnObj)->GetIntegerField(TEXT("in_node"));
		Conn.OutNode = (*ConnObj)->GetIntegerField(TEXT("out_node"));
		Conn.Weight = (*ConnObj)->GetNumberField(TEXT("weight"));
		Conn.bEnabled = (*ConnObj)->HasField(TEXT("enabled")) ? (*ConnObj)->GetBoolField(TEXT("enabled")) : true;

		if (!SeenNodeIds.Contains(Conn.InNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Connection references missing in_node %d in: %s"), Conn.InNode, *AbsolutePath);
			return false;
		}
		if (!SeenNodeIds.Contains(Conn.OutNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Connection references missing out_node %d in: %s"), Conn.OutNode, *AbsolutePath);
			return false;
		}

		OutGenome.Connections.Add(Conn);
	}

	// Validation (input/output count consistency, etc.)
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
		UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Genome %d: num_inputs=%d num_outputs=%d (must be positive)"),
			InOutGenome.GenomeID, ExpectedInputs, ExpectedOutputs);
		return false;
	}

	// NEAT convention: input nodes 0..NumInputs-1, output nodes NumInputs..NumInputs+NumOutputs-1
	TSet<int32> NodeIds;
	for (const FNEATNode& N : InOutGenome.Nodes)
	{
		NodeIds.Add(N.NodeId);
	}

	for (int32 i = 0; i < ExpectedInputs; ++i)
	{
		if (!NodeIds.Contains(i))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Genome %d: missing input node %d (expected 0..%d)"),
				InOutGenome.GenomeID, i, ExpectedInputs - 1);
			return false;
		}
	}
	for (int32 i = 0; i < ExpectedOutputs; ++i)
	{
		const int32 OutId = ExpectedInputs + i;
		if (!NodeIds.Contains(OutId))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Genome %d: missing output node %d (expected %d..%d)"),
				InOutGenome.GenomeID, OutId, ExpectedInputs, ExpectedInputs + ExpectedOutputs - 1);
			return false;
		}
	}

	// Connections: in_node and out_node must exist (already checked in LoadFromFile)
	for (const FNEATConnection& C : InOutGenome.Connections)
	{
		if (!NodeIds.Contains(C.InNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Genome %d: connection in_node %d not in nodes"), InOutGenome.GenomeID, C.InNode);
			return false;
		}
		if (!NodeIds.Contains(C.OutNode))
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGenomeImporter] Genome %d: connection out_node %d not in nodes"), InOutGenome.GenomeID, C.OutNode);
			return false;
		}
	}

	int32 DisabledCount = 0;
	for (const FNEATConnection& C : InOutGenome.Connections)
	{
		if (!C.bEnabled) DisabledCount++;
	}

	InOutGenome.bIsValid = true;
	UE_LOG(LogTemp, Log, TEXT("[NEATGenomeImporter] Genome %d validated: %d nodes, %d connections (%d disabled), inputs=%d outputs=%d"),
		InOutGenome.GenomeID, InOutGenome.Nodes.Num(), InOutGenome.Connections.Num(), DisabledCount,
		InOutGenome.NumInputs, InOutGenome.NumOutputs);
	return true;
}
