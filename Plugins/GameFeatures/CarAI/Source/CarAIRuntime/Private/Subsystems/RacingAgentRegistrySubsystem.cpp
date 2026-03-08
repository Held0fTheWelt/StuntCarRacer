#include "Subsystems/RacingAgentRegistrySubsystem.h"
#include "Components/RacingAgentComponent.h"
#include "CarAIRuntimeLogging.h"
#include "Engine/World.h"

bool URacingAgentRegistrySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool URacingAgentRegistrySubsystem::RegisterAgent(URacingAgentComponent* Agent)
{
	if (!Agent)
	{
		UE_LOG(LogCarAIAgent, Warning, TEXT("[RacingAgentRegistry] RegisterAgent: null agent ignored."));
		return false;
	}
	if (IsRegistered(Agent))
	{
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[RacingAgentRegistry] Agent already registered; skipping duplicate (%s)."), Agent->GetOwner() ? *Agent->GetOwner()->GetName() : TEXT("?"));
		return false;
	}

	RegisteredAgents.Add(Agent);
	UE_LOG(LogCarAIAgent, Display, TEXT("[RacingAgentRegistry] Agent registered. Total=%d World=%s Owner=%s"),
		RegisteredAgents.Num(), GetWorld() ? *GetWorld()->GetName() : TEXT("?"), Agent->GetOwner() ? *Agent->GetOwner()->GetName() : TEXT("?"));

	OnAgentRegistered.Broadcast(Agent);
	return true;
}

void URacingAgentRegistrySubsystem::UnregisterAgent(URacingAgentComponent* Agent)
{
	if (!Agent)
	{
		return;
	}

	const int32 PrevNum = RegisteredAgents.Num();
	RegisteredAgents.RemoveAll([Agent](const TWeakObjectPtr<URacingAgentComponent>& Weak)
	{
		return Weak.Get() == Agent;
	});

	if (RegisteredAgents.Num() < PrevNum)
	{
		UE_LOG(LogCarAIAgent, Display, TEXT("[RacingAgentRegistry] Agent unregistered. Total=%d World=%s Owner=%s"),
			RegisteredAgents.Num(), GetWorld() ? *GetWorld()->GetName() : TEXT("?"), Agent->GetOwner() ? *Agent->GetOwner()->GetName() : TEXT("?"));
		OnAgentUnregistered.Broadcast(Agent);
	}
}

TArray<URacingAgentComponent*> URacingAgentRegistrySubsystem::GetRegisteredAgents() const
{
	TArray<URacingAgentComponent*> Out;
	for (const TWeakObjectPtr<URacingAgentComponent>& Weak : RegisteredAgents)
	{
		if (URacingAgentComponent* Ptr = Weak.Get())
		{
			Out.Add(Ptr);
		}
	}
	return Out;
}

bool URacingAgentRegistrySubsystem::IsRegistered(URacingAgentComponent* Agent) const
{
	if (!Agent) return false;
	for (const TWeakObjectPtr<URacingAgentComponent>& Weak : RegisteredAgents)
	{
		if (Weak.Get() == Agent) return true;
	}
	return false;
}
