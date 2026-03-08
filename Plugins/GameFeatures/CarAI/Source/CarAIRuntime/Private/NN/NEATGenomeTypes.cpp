#include "NN/NEATGenomeTypes.h"
#include "Types/RacingAgentTypes.h"

#include "Misc/AssertionMacros.h"

// ============================================================================
// Activation parsing (fail-fast on unsupported)
// ============================================================================

ENEATActivation NEATActivationFromString(const FString& Name)
{
	const FString Lower = Name.ToLower().TrimStartAndEnd();
	if (Lower == TEXT("sigmoid")) return ENEATActivation::Sigmoid;
	if (Lower == TEXT("tanh"))   return ENEATActivation::Tanh;
	if (Lower == TEXT("relu"))   return ENEATActivation::ReLU;
	return ENEATActivation::Invalid;
}

// ============================================================================
// FNEATGenome
// ============================================================================

const FNEATNode* FNEATGenome::FindNode(int32 NodeId) const
{
	for (const FNEATNode& N : Nodes)
	{
		if (N.NodeId == NodeId) return &N;
	}
	return nullptr;
}

// ============================================================================
// Conversion to legacy FNEATGenomeData
// ============================================================================

static FString ActivationToString(ENEATActivation A)
{
	switch (A)
	{
		case ENEATActivation::Sigmoid: return TEXT("sigmoid");
		case ENEATActivation::Tanh:    return TEXT("tanh");
		case ENEATActivation::ReLU:    return TEXT("relu");
		default:                       return TEXT("invalid");
	}
}

void NEATGenomeToGenomeData(const FNEATGenome& Genome, FNEATGenomeData& OutData)
{
	OutData.GenomeID = Genome.GenomeID;
	OutData.Generation = Genome.Generation;
	OutData.Fitness = Genome.Fitness;
	OutData.NodeIDs.Empty();
	OutData.Connections.Empty();
	OutData.Activations.Empty();

	// Preserve order by node id for parallel arrays
	TArray<FNEATNode> SortedNodes = Genome.Nodes;
	SortedNodes.Sort([](const FNEATNode& A, const FNEATNode& B) { return A.NodeId < B.NodeId; });

	for (const FNEATNode& N : SortedNodes)
	{
		OutData.NodeIDs.Add(N.NodeId);
		OutData.Activations.Add(ActivationToString(N.Activation));
	}

	for (const FNEATConnection& C : Genome.Connections)
	{
		OutData.Connections.Add(FString::Printf(TEXT("%d,%d,%.4f,%d"),
			C.InNode, C.OutNode, C.Weight, C.bEnabled ? 1 : 0));
	}
}
