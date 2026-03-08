#include "UI/NeatTrainingEditorWidget.h"
#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

// ============================================================================
// Manager Lifetime
// ============================================================================

void UNeatTrainingEditorWidget::InitializeManager()
{
	if (TrainingManager && IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Manager already initialized; reusing existing instance"));
		LastStatusMessage = TEXT("Manager ready (reused)");
		bManagerReady = true;
		RefreshStatus();
		return;
	}

	TrainingManager = NewObject<UNEATTrainingManager>(this);
	if (!TrainingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] Failed to create UNEATTrainingManager"));
		LastStatusMessage = TEXT("ERROR: Failed to create manager");
		bManagerReady = false;
		return;
	}

	bManagerReady = true;
	LastStatusMessage = TEXT("Manager initialized");
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] UNEATTrainingManager created successfully"));
	RefreshStatus();
}

// ============================================================================
// Agent Registration
// ============================================================================

void UNeatTrainingEditorWidget::RegisterAgents()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] RegisterAgents: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("ERROR: Initialize manager before registering agents");
		return;
	}

	UWorld* World = GetEditorOrPIEWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] RegisterAgents: no valid world found; enter PIE or open a level"));
		LastStatusMessage = TEXT("ERROR: No valid world; enter PIE or open a level");
		return;
	}

	TrainingManager->UnregisterAllAgents();

	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<URacingAgentComponent*> AgentComps;
		(*It)->GetComponents<URacingAgentComponent>(AgentComps);
		for (URacingAgentComponent* Comp : AgentComps)
		{
			if (Comp && IsValid(Comp))
			{
				TrainingManager->RegisterAgent(Comp);
				Count++;
			}
		}
	}

	RegisteredAgentCount = Count;

	if (Count == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NeatTrainingEditorWidget] RegisterAgents: no URacingAgentComponent found in world (%s). Ensure agents are in the level and PIE is active."),
			*World->GetName());
		LastStatusMessage = TEXT("WARNING: No agents found; check level and PIE state");
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Agent registration: found and registered %d agent(s) in world '%s'"),
			Count, *World->GetName());
		LastStatusMessage = FString::Printf(TEXT("Registered %d agent(s)"), Count);
	}
}

// ============================================================================
// Training Control
// ============================================================================

void UNeatTrainingEditorWidget::StartTraining()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("ERROR: Initialize manager before starting training");
		return;
	}

	if (RegisteredAgentCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: no agents registered; call Register Agents first"));
		LastStatusMessage = TEXT("ERROR: No agents registered; call Register Agents first");
		return;
	}

	if (PythonExecutable.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: PythonExecutable is empty; set a valid Python path"));
		LastStatusMessage = TEXT("ERROR: Python executable is not set");
		return;
	}

	ApplyConfigToManager();

	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Training start requested: FreshStart=%s, Agents=%d, Generations=%d, PopulationSize=%d, MaxEpisodeDuration=%.1fs, Python='%s'"),
		bFreshStart ? TEXT("true") : TEXT("false"),
		RegisteredAgentCount, NumGenerations, PopulationSize, MaxEpisodeDuration, *PythonExecutable);

	TrainingManager->StartTraining();

	bTrainingInProgress = true;
	LastStatusMessage = FString::Printf(TEXT("Training started (%d agents, %d generations)"), RegisteredAgentCount, NumGenerations);
}

void UNeatTrainingEditorWidget::StopTraining()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NeatTrainingEditorWidget] StopTraining: no manager exists; nothing to stop"));
		LastStatusMessage = TEXT("No manager to stop");
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Training stop requested"));
	TrainingManager->StopTraining();

	bTrainingInProgress = false;
	LastStatusMessage = TEXT("Training stopped");
	RefreshStatus();
}

// ============================================================================
// Status
// ============================================================================

void UNeatTrainingEditorWidget::RefreshStatus()
{
	bManagerReady = (TrainingManager != nullptr && IsValid(TrainingManager));

	if (!bManagerReady)
	{
		bTrainingInProgress = false;
		RegisteredAgentCount = 0;
		CurrentGeneration = 0;
		if (LastStatusMessage.IsEmpty())
		{
			LastStatusMessage = TEXT("Not initialized");
		}
		return;
	}

	bTrainingInProgress = TrainingManager->IsTraining();
	CurrentGeneration = TrainingManager->GetCurrentGeneration();
	// RegisteredAgentCount is authoritative from RegisterAgents(); do not reset here.
}

// ============================================================================
// Internal Helpers
// ============================================================================

void UNeatTrainingEditorWidget::ApplyConfigToManager()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		return;
	}
	TrainingManager->bFreshStart = bFreshStart;
	TrainingManager->PythonExecutable = PythonExecutable;
	TrainingManager->PopulationSize = PopulationSize;
	TrainingManager->NumGenerations = NumGenerations;
	TrainingManager->MaxEpisodeDuration = MaxEpisodeDuration;

	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Config applied to manager: FreshStart=%s PopSize=%d NumGen=%d EpDur=%.1f Python='%s'"),
		bFreshStart ? TEXT("true") : TEXT("false"),
		PopulationSize, NumGenerations, MaxEpisodeDuration, *PythonExecutable);
}

UWorld* UNeatTrainingEditorWidget::GetEditorOrPIEWorld() const
{
	if (!GEngine)
	{
		return nullptr;
	}

	// Prefer the PIE world when active — agents live there during training.
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			return Context.World();
		}
	}

	// Fall back to editor world.
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}

	return nullptr;
}
