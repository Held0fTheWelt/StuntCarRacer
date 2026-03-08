#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NN/NEATGenomeTypes.h"
#include "NN/NEATPolicyBackend.h"
#include "NEATGraphEvaluator.generated.h"

/** Single input to a node (in_node_id, weight) for feed-forward evaluation. */
USTRUCT()
struct FNEATCompiledInput
{
	GENERATED_BODY()
	UPROPERTY() int32 InNodeId = -1;
	UPROPERTY() float Weight = 0.f;
};

/** Compiled node for evaluation: bias, response, activation, and enabled inputs. */
USTRUCT()
struct FNEATCompiledNode
{
	GENERATED_BODY()
	UPROPERTY() int32 NodeId = -1;
	UPROPERTY() float Bias = 0.f;
	UPROPERTY() float Response = 1.f;
	UPROPERTY() ENEATActivation Activation = ENEATActivation::Invalid;
	UPROPERTY() TArray<FNEATCompiledInput> Inputs;
};

/**
 * Dedicated NEAT runtime inference backend. Compiles a validated FNEATGenome into an
 * acyclic feed-forward execution graph; runs deterministic inference:
 *   value = activation(bias + response * sum(weight * input_value)) per node in topological order.
 * Produces exactly 3 raw outputs (steer, throttle, brake); the agent layer clamps them
 * (not inside this evaluator). Observation size is validated before inference; invalid
 * size or unsupported graph (e.g. cycle, disconnected output) fails with a clear log.
 */
UCLASS()
class CARAIRUNTIME_API UNEATGraphEvaluator : public UPolicyBackend
{
	GENERATED_BODY()

public:
	/**
	 * Build evaluable graph from a validated genome. Detects cycles and fails clearly.
	 * @return true if graph is acyclic and compiled; false otherwise (logs reason)
	 */
	UFUNCTION(BlueprintCallable, Category = "NEAT")
	bool BuildFromGenome(const FNEATGenome& Genome);

	virtual bool Evaluate(const TArray<float>& Observation, TArray<float>& OutActions) override;
	virtual bool IsValid() const override { return bCompiled; }

	int32 GetExpectedInputCount() const { return ExpectedInputCount; }
	int32 GetExpectedOutputCount() const { return ExpectedOutputCount; }

	/** Create and compile an evaluator from a genome. Returns nullptr on failure. */
	UFUNCTION(BlueprintCallable, Category = "NEAT", meta = (WorldContext = "WorldContextObject"))
	static UNEATGraphEvaluator* CreateFromGenome(UObject* WorldContextObject, const FNEATGenome& Genome);

private:
	/** Apply activation function; no clamping. */
	static float ApplyActivation(ENEATActivation Act, float X);

	UPROPERTY()
	bool bCompiled = false;

	UPROPERTY()
	int32 ExpectedInputCount = 0;

	UPROPERTY()
	int32 ExpectedOutputCount = 0;

	/** Node ids in topological (evaluation) order. */
	UPROPERTY()
	TArray<int32> TopoOrder;

	/** Per-node compiled data (index matches TopoOrder for non-input nodes; input nodes are not in this list for computation). */
	UPROPERTY()
	TArray<FNEATCompiledNode> CompiledNodes;

	/** NodeId -> index into TopoOrder (or we use a TMap for value lookup during eval). */
	TMap<int32, int32> NodeIdToTopoIndex;

	/** Value buffer during Evaluate: NodeId -> current value. */
	TMap<int32, float> ValueMap;
};
