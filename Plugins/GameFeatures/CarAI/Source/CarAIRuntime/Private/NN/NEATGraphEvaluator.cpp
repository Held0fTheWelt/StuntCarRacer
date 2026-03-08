#include "NN/NEATGraphEvaluator.h"
#include "NN/NEATGenomeTypes.h"

#include "Misc/AssertionMacros.h"

// ============================================================================
// Activation functions (no clamping; agent clamps outputs)
// ============================================================================

static float Sigmoid(float X)
{
	return 1.f / (1.f + FMath::Exp(-FMath::Clamp(X, -20.f, 20.f)));
}

static float Tanh(float X)
{
	return FMath::Tanh(X);
}

static float Relu(float X)
{
	return FMath::Max(0.f, X);
}

float UNEATGraphEvaluator::ApplyActivation(ENEATActivation Act, float X)
{
	switch (Act)
	{
		case ENEATActivation::Sigmoid: return Sigmoid(X);
		case ENEATActivation::Tanh:    return Tanh(X);
		case ENEATActivation::ReLU:    return Relu(X);
		default:                       return 0.f;
	}
}

// ============================================================================
// BuildFromGenome: topological sort + compile
// ============================================================================

bool UNEATGraphEvaluator::BuildFromGenome(const FNEATGenome& Genome)
{
	bCompiled = false;
	TopoOrder.Empty();
	CompiledNodes.Empty();
	NodeIdToTopoIndex.Empty();
	ExpectedInputCount = Genome.NumInputs;
	ExpectedOutputCount = Genome.NumOutputs;

	if (!Genome.bIsValid || Genome.Nodes.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] BuildFromGenome FAIL: genome invalid or empty (genome_id=%d)"), Genome.GenomeID);
		return false;
	}

	if (Genome.NumOutputs != 3)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] BuildFromGenome FAIL: expected 3 outputs (steer, throttle, brake), got %d"), Genome.NumOutputs);
		return false;
	}

	// Reject any node with unsupported (Invalid) activation
	for (const FNEATNode& N : Genome.Nodes)
	{
		if (N.Activation == ENEATActivation::Invalid)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] BuildFromGenome FAIL: node %d has Invalid activation (genome_id=%d)"), N.NodeId, Genome.GenomeID);
			return false;
		}
	}

	// Build node set and in-degree (enabled connections only)
	TMap<int32, int32> InDegree;
	for (const FNEATNode& N : Genome.Nodes)
	{
		InDegree.Add(N.NodeId, 0);
	}
	for (const FNEATConnection& C : Genome.Connections)
	{
		if (!C.bEnabled) continue;
		if (InDegree.Contains(C.OutNode))
		{
			InDegree[C.OutNode]++;
		}
	}

	// Kahn's algorithm: nodes with in-degree 0 go first
	TArray<int32> Queue;
	for (const auto& Pair : InDegree)
	{
		if (Pair.Value == 0) Queue.Add(Pair.Key);
	}

	TArray<int32> Order;
	while (Queue.Num() > 0)
	{
		int32 NodeId = Queue.Pop();
		Order.Add(NodeId);

		for (const FNEATConnection& C : Genome.Connections)
		{
			if (!C.bEnabled) continue;
			if (C.InNode != NodeId) continue;
			int32* OutDegree = InDegree.Find(C.OutNode);
			if (OutDegree)
			{
				(*OutDegree)--;
				if (*OutDegree == 0) Queue.Add(C.OutNode);
			}
		}
	}

	if (Order.Num() != Genome.Nodes.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] BuildFromGenome FAIL: cycle detected (ordered %d of %d nodes); only acyclic feed-forward supported"),
			Order.Num(), Genome.Nodes.Num());
		return false;
	}

	TopoOrder = Order;
	for (int32 i = 0; i < TopoOrder.Num(); ++i)
	{
		NodeIdToTopoIndex.Add(TopoOrder[i], i);
	}

	// Build compiled nodes (for non-input nodes: bias, response, activation, inputs)
	const int32 NumInputs = Genome.NumInputs;
	for (int32 NodeId : TopoOrder)
	{
		const FNEATNode* Node = Genome.FindNode(NodeId);
		if (!Node)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] BuildFromGenome: node %d not found"), NodeId);
			return false;
		}

		FNEATCompiledNode Comp;
		Comp.NodeId = NodeId;
		Comp.Bias = Node->Bias;
		Comp.Response = Node->Response;
		Comp.Activation = Node->Activation;

		for (const FNEATConnection& C : Genome.Connections)
		{
			if (!C.bEnabled) continue;
			if (C.OutNode != NodeId) continue;
			FNEATCompiledInput Inp;
			Inp.InNodeId = C.InNode;
			Inp.Weight = C.Weight;
			Comp.Inputs.Add(Inp);
		}

		CompiledNodes.Add(Comp);
	}

	bCompiled = true;
	UE_LOG(LogTemp, Log, TEXT("[NEATGraphEvaluator] Graph compilation summary: genome_id=%d | inputs=%d outputs=%d (steer, throttle, brake) | nodes=%d topo_order=%d | feed-forward acyclic"),
		Genome.GenomeID, ExpectedInputCount, ExpectedOutputCount, Genome.Nodes.Num(), TopoOrder.Num());
	return true;
}

// ============================================================================
// Evaluate
// ============================================================================

bool UNEATGraphEvaluator::Evaluate(const TArray<float>& Observation, TArray<float>& OutActions)
{
	OutActions.Empty();

	if (!bCompiled)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] Inference failed: graph not compiled"));
		return false;
	}

	if (Observation.Num() != ExpectedInputCount)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] Inference failed: observation size %d does not match expected input count %d"),
			Observation.Num(), ExpectedInputCount);
		return false;
	}

	ValueMap.Empty();

	// Set input node values (NEAT convention: input nodes 0..NumInputs-1)
	for (int32 i = 0; i < ExpectedInputCount; ++i)
	{
		ValueMap.Add(i, Observation[i]);
	}

	// Feed-forward: evaluate non-input nodes in topological order
	for (const FNEATCompiledNode& Comp : CompiledNodes)
	{
		if (Comp.NodeId < ExpectedInputCount) continue;
		float Sum = 0.f;
		for (const FNEATCompiledInput& Inp : Comp.Inputs)
		{
			const float* Val = ValueMap.Find(Inp.InNodeId);
			if (Val) Sum += Inp.Weight * (*Val);
		}
		float X = Comp.Bias + Comp.Response * Sum;
		float Y = ApplyActivation(Comp.Activation, X);
		ValueMap.Add(Comp.NodeId, Y);
	}

	// Read 3 outputs (node ids NumInputs, NumInputs+1, NumInputs+2); fail if any output node missing (disconnected)
	const int32 OutBase = ExpectedInputCount;
	OutActions.SetNum(3);
	for (int32 k = 0; k < 3; ++k)
	{
		const float* V = ValueMap.Find(OutBase + k);
		if (!V)
		{
			UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] Inference failed: output node %d (index %d) not computed; graph may have disconnected output"),
				OutBase + k, k);
			return false;
		}
		OutActions[k] = *V;
	}

	return true;
}

// ============================================================================
// CreateFromGenome
// ============================================================================

UNEATGraphEvaluator* UNEATGraphEvaluator::CreateFromGenome(UObject* WorldContextObject, const FNEATGenome& Genome)
{
	UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
	UNEATGraphEvaluator* Eval = NewObject<UNEATGraphEvaluator>(Outer);
	if (!Eval)
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] CreateFromGenome failed: could not create evaluator object"));
		return nullptr;
	}
	if (!Eval->BuildFromGenome(Genome))
	{
		UE_LOG(LogTemp, Error, TEXT("[NEATGraphEvaluator] CreateFromGenome failed for genome_id=%d (see BuildFromGenome logs above)"), Genome.GenomeID);
		return nullptr;
	}
	UE_LOG(LogTemp, Log, TEXT("[NEATGraphEvaluator] NEAT runtime backend ready: genome_id=%d input_count=%d output_count=3"),
		Genome.GenomeID, Eval->GetExpectedInputCount());
	return Eval;
}
