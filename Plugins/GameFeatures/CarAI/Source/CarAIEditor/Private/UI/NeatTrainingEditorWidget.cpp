#include "UI/NeatTrainingEditorWidget.h"
#include "CarAIEditor.h"
#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

// ============================================================================
// Workflow State
// ============================================================================

static const TCHAR* WorkflowStateToShortString(ENeatTrainingWorkflowState S)
{
	switch (S)
	{
		case ENeatTrainingWorkflowState::Idle:                    return TEXT("Idle");
		case ENeatTrainingWorkflowState::ManagerInitialized:     return TEXT("ManagerInitialized");
		case ENeatTrainingWorkflowState::ArmedForPIE:            return TEXT("ArmedForPIE");
		case ENeatTrainingWorkflowState::WaitingForPIEWorld:     return TEXT("WaitingForPIEWorld");
		case ENeatTrainingWorkflowState::WaitingForRuntimeAgents: return TEXT("WaitingForRuntimeAgents");
		case ENeatTrainingWorkflowState::AgentsRegistered:      return TEXT("AgentsRegistered");
		case ENeatTrainingWorkflowState::TrainingStarting:       return TEXT("TrainingStarting");
		case ENeatTrainingWorkflowState::TrainingRunning:        return TEXT("TrainingRunning");
		case ENeatTrainingWorkflowState::TrainingFailed:         return TEXT("TrainingFailed");
		case ENeatTrainingWorkflowState::TrainingCompleted:     return TEXT("TrainingCompleted");
		case ENeatTrainingWorkflowState::PIEEnded:               return TEXT("PIEEnded");
		default:                                                return TEXT("Unknown");
	}
}

void UNeatTrainingEditorWidget::SetWorkflowState(ENeatTrainingWorkflowState NewState)
{
	const ENeatTrainingWorkflowState OldState = WorkflowState;
	if (OldState == NewState)
	{
		return;
	}
	WorkflowState = NewState;
	LastStatusMessage = GetWorkflowStateDescription();
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Workflow state: %s -> %s | %s"),
		WorkflowStateToShortString(OldState), WorkflowStateToShortString(NewState), *LastStatusMessage);
}

FString UNeatTrainingEditorWidget::GetWorkflowStateDescription() const
{
	switch (WorkflowState)
	{
		case ENeatTrainingWorkflowState::Idle:
			return TEXT("Idle. Initialize Manager to begin.");
		case ENeatTrainingWorkflowState::ManagerInitialized:
			return TEXT("Manager ready. Arm training (then start PIE) or register agents and start manually.");
		case ENeatTrainingWorkflowState::ArmedForPIE:
			return TEXT("Armed for training. Start PIE to continue automatically.");
		case ENeatTrainingWorkflowState::WaitingForPIEWorld:
			return TEXT("Waiting for PIE world...");
		case ENeatTrainingWorkflowState::WaitingForRuntimeAgents:
			return TEXT("Waiting for runtime agents (GameFeature)...");
		case ENeatTrainingWorkflowState::AgentsRegistered:
			return FString::Printf(TEXT("Agents registered (%d). Starting training..."), RegisteredAgentCount);
		case ENeatTrainingWorkflowState::TrainingStarting:
			return TEXT("Starting training...");
		case ENeatTrainingWorkflowState::TrainingRunning:
			return FString::Printf(TEXT("Training running (generation %d)."), CurrentGeneration);
		case ENeatTrainingWorkflowState::TrainingFailed:
			return TEXT("Training failed. Check Output Log.");
		case ENeatTrainingWorkflowState::TrainingCompleted:
			return TEXT("Training completed.");
		case ENeatTrainingWorkflowState::PIEEnded:
			return TEXT("PIE ended. Re-arm or re-register to run again.");
		default:
			return TEXT("Unknown state");
	}
}

// ============================================================================
// Manager Lifetime
// ============================================================================

void UNeatTrainingEditorWidget::InitializeManager()
{
	if (TrainingManager && IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Manager already initialized; reusing existing instance"));
		bManagerReady = true;
		SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
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
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] UNEATTrainingManager created successfully"));
	SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
	RefreshStatus();
}

void UNeatTrainingEditorWidget::ArmTraining()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NeatTrainingEditorWidget] ArmTraining: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("Initialize manager first, then Arm Training");
		return;
	}
	bArmedForPIE = true;
	SetWorkflowState(ENeatTrainingWorkflowState::ArmedForPIE);
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Training armed for PIE. Start PIE to continue automatically."));
}

void UNeatTrainingEditorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	FCarAIEditor::Get().RegisterNeatTrainingWidget(this);
}

void UNeatTrainingEditorWidget::NativeDestruct()
{
	FCarAIEditor::Get().UnregisterNeatTrainingWidget(this);
	CachedPIEWorld.Reset();
	Super::NativeDestruct();
}

void UNeatTrainingEditorWidget::OnPIEWorldStarted(UWorld* PIEWorld)
{
	if (!bArmedForPIE)
	{
		return;
	}
	SetWorkflowState(ENeatTrainingWorkflowState::WaitingForPIEWorld);
	CachedPIEWorld = PIEWorld;
	if (PIEWorld)
	{
		SetWorkflowState(ENeatTrainingWorkflowState::WaitingForRuntimeAgents);
		UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] PIE world captured; deferred flow waiting for runtime agents."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[NeatTrainingEditorWidget] PIE started but world not resolved; staying in WaitingForPIEWorld."));
	}
}

void UNeatTrainingEditorWidget::OnPIEEnded()
{
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] PIE ended; deferred training request cancelled, clearing stale references."));
	CachedPIEWorld.Reset();
	if (TrainingManager && IsValid(TrainingManager))
	{
		TrainingManager->UnregisterAllAgents();
	}
	RegisteredAgentCount = 0;
	bArmedForPIE = false;
	SetWorkflowState(ENeatTrainingWorkflowState::PIEEnded);
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
		SetWorkflowState(ENeatTrainingWorkflowState::AgentsRegistered);
		UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Agent registration: found and registered %d agent(s) in world '%s'"),
			Count, *World->GetName());
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

	SetWorkflowState(ENeatTrainingWorkflowState::TrainingStarting);
	UE_LOG(LogTemp, Log, TEXT("[NeatTrainingEditorWidget] Training start requested: FreshStart=%s, Agents=%d, Generations=%d, PopulationSize=%d, MaxEpisodeDuration=%.1fs, Python='%s'"),
		bFreshStart ? TEXT("true") : TEXT("false"),
		RegisteredAgentCount, NumGenerations, PopulationSize, MaxEpisodeDuration, *PythonExecutable);

	TrainingManager->StartTraining();

	bTrainingInProgress = true;
	SetWorkflowState(ENeatTrainingWorkflowState::TrainingRunning);
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
	SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
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
		if (WorkflowState != ENeatTrainingWorkflowState::Idle)
		{
			SetWorkflowState(ENeatTrainingWorkflowState::Idle);
		}
		else if (LastStatusMessage.IsEmpty())
		{
			LastStatusMessage = TEXT("Not initialized");
		}
		return;
	}

	bTrainingInProgress = TrainingManager->IsTraining();
	CurrentGeneration = TrainingManager->GetCurrentGeneration();
	// RegisteredAgentCount is authoritative from RegisterAgents(); do not reset here.

	// Keep status text in sync with workflow state (e.g. TrainingRunning shows current generation)
	if (WorkflowState == ENeatTrainingWorkflowState::TrainingRunning)
	{
		LastStatusMessage = GetWorkflowStateDescription();
	}
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
