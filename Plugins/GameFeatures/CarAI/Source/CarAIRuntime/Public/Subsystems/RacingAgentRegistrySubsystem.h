#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RacingAgentRegistrySubsystem.generated.h"

class URacingAgentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRacingAgentRegistered, URacingAgentComponent*, Agent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRacingAgentUnregistered, URacingAgentComponent*, Agent);

/**
 * Runtime-owned registry of URacingAgentComponent instances in the world.
 * This is the canonical source for "which agents exist in PIE"; the editor consumes this
 * instead of world-scanning heuristics. Agents self-register in BeginPlay and unregister in EndPlay.
 */
UCLASS()
class CARAIRUNTIME_API URacingAgentRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Register a runtime agent. Prevents duplicates; logs at Verbose if already registered. Returns true if newly added. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent Registry")
	bool RegisterAgent(URacingAgentComponent* Agent);

	/** Unregister a runtime agent. Safe to call if not registered. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent Registry")
	void UnregisterAgent(URacingAgentComponent* Agent);

	/** Get all currently registered agents (valid pointers only). */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent Registry")
	TArray<URacingAgentComponent*> GetRegisteredAgents() const;

	/** Number of agents currently in the registry (including stale weak refs until pruned). */
	int32 GetRegisteredCount() const { return RegisteredAgents.Num(); }

	/** True if the given component is in the registry. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent Registry")
	bool IsRegistered(URacingAgentComponent* Agent) const;

	/** Fired when an agent is successfully registered. */
	UPROPERTY(BlueprintAssignable, Category = "Racing Agent Registry")
	FOnRacingAgentRegistered OnAgentRegistered;

	/** Fired when an agent is unregistered. */
	UPROPERTY(BlueprintAssignable, Category = "Racing Agent Registry")
	FOnRacingAgentUnregistered OnAgentUnregistered;

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TArray<TWeakObjectPtr<URacingAgentComponent>> RegisteredAgents;
};
