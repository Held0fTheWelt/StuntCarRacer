#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NEATPolicyBackend.generated.h"

/**
 * Abstract policy backend: given an observation vector, produces action outputs.
 * Implementations: USimpleNeuralNetwork (fixed MLP) or UNEATGraphEvaluator (NEAT genome).
 * RacingAgentComponent uses one backend; outputs are clamped at the agent layer.
 */
UCLASS(Abstract)
class CARAIRUNTIME_API UPolicyBackend : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Run policy inference. OutActions must receive exactly 3 values (steer, throttle, brake).
	 * Outputs are raw; the agent clamps steer to [-1,1], throttle/brake to [0,1].
	 * @return true if inference succeeded; false on validation or runtime failure (logs reason)
	 */
	UFUNCTION(BlueprintCallable, Category = "Policy")
	virtual bool Evaluate(const TArray<float>& Observation, TArray<float>& OutActions) { return false; }

	/** Whether this backend is ready to evaluate (compiled graph, valid network, etc.). */
	UFUNCTION(BlueprintCallable, Category = "Policy")
	virtual bool IsValid() const { return false; }
};
