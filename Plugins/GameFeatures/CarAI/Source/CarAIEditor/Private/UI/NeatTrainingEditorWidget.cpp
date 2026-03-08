#include "UI/NeatTrainingEditorWidget.h"
#include "CarAIEditor.h"
#include "Logging.h"
#include "Manager/NEATTrainingManager.h"
#include "Components/RacingAgentComponent.h"
#include "Subsystems/RacingAgentRegistrySubsystem.h"
#include "CarAIRuntimeLogging.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

// ============================================================================
// Workflow State
// ============================================================================

static const TCHAR* WorkflowStateToShortString(ENeatTrainingWorkflowState S)
{
	switch (S)
	{
		case ENeatTrainingWorkflowState::Idle:                     return TEXT("Idle");
		case ENeatTrainingWorkflowState::ManagerInitialized:      return TEXT("ManagerInitialized");
		case ENeatTrainingWorkflowState::ArmedForPIE:            return TEXT("ArmedForPIE");
		case ENeatTrainingWorkflowState::WaitingForPIEWorld:      return TEXT("WaitingForPIEWorld");
		case ENeatTrainingWorkflowState::WaitingForRuntimeAgents: return TEXT("WaitingForRuntimeAgents");
		case ENeatTrainingWorkflowState::RegisteringAgents:       return TEXT("RegisteringAgents");
		case ENeatTrainingWorkflowState::ReadyToStart:           return TEXT("ReadyToStart");
		case ENeatTrainingWorkflowState::AgentsRegistered:        return TEXT("AgentsRegistered");
		case ENeatTrainingWorkflowState::TrainingStarting:       return TEXT("TrainingStarting");
		case ENeatTrainingWorkflowState::TrainingRunning:        return TEXT("TrainingRunning");
		case ENeatTrainingWorkflowState::TrainingFailed:         return TEXT("TrainingFailed");
		case ENeatTrainingWorkflowState::TrainingCompleted:      return TEXT("TrainingCompleted");
		case ENeatTrainingWorkflowState::PIEEnded:               return TEXT("PIEEnded");
		default:                                                 return TEXT("Unknown");
	}
}

void UNeatTrainingEditorWidget::SetWorkflowState(ENeatTrainingWorkflowState NewState, const FString& Reason)
{
	const ENeatTrainingWorkflowState OldState = WorkflowState;
	if (OldState == NewState)
	{
		return;
	}
	WorkflowState = NewState;
	LastStatusMessage = GetWorkflowStateDescription();
	if (WorkflowLogVerbosity >= 1)
	{
		if (Reason.IsEmpty())
		{
			UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Workflow state: %s -> %s | %s"),
				WorkflowStateToShortString(OldState), WorkflowStateToShortString(NewState), *LastStatusMessage);
		}
		else
		{
			UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Workflow state: %s -> %s | %s | Reason: %s"),
				WorkflowStateToShortString(OldState), WorkflowStateToShortString(NewState), *LastStatusMessage, *Reason);
		}
	}
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
		case ENeatTrainingWorkflowState::RegisteringAgents:
			return TEXT("Registering discovered agents...");
		case ENeatTrainingWorkflowState::ReadyToStart:
			return TEXT("Agents registered; evaluating readiness to start training.");
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
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Manager already initialized; reusing existing instance"));
		bManagerReady = true;
		SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
		RefreshStatus();
		return;
	}

	TrainingManager = NewObject<UNEATTrainingManager>(this);
	if (!TrainingManager)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] Failed to create UNEATTrainingManager"));
		LastStatusMessage = TEXT("ERROR: Failed to create manager");
		bManagerReady = false;
		return;
	}

	bManagerReady = true;
	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] UNEATTrainingManager created successfully"));
	SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
	RefreshStatus();
}

void UNeatTrainingEditorWidget::ArmTraining()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] ArmTraining: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("Initialize manager first, then Arm Training");
		return;
	}
	bArmedForPIE = true;
	SetWorkflowState(ENeatTrainingWorkflowState::ArmedForPIE);
	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Training armed for PIE. Start PIE to continue automatically."));

	// Secondary path: PIE already running — start discovery immediately.
	UWorld* ExistingPIEWorld = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World())
			{
				ExistingPIEWorld = Context.World();
				break;
			}
		}
	}
	if (ExistingPIEWorld)
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] PIE already active; starting agent discovery now."));
		OnPIEWorldStarted(ExistingPIEWorld);
	}
}

void UNeatTrainingEditorWidget::CancelArmedTraining()
{
	if (!bArmedForPIE && WorkflowState != ENeatTrainingWorkflowState::ArmedForPIE &&
		WorkflowState != ENeatTrainingWorkflowState::WaitingForPIEWorld &&
		WorkflowState != ENeatTrainingWorkflowState::WaitingForRuntimeAgents &&
		WorkflowState != ENeatTrainingWorkflowState::RegisteringAgents &&
		WorkflowState != ENeatTrainingWorkflowState::ReadyToStart)
	{
		return;
	}
	// Clear discovery timer if we were waiting for agents.
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		GEditor->GetEditorWorldContext().World()->GetTimerManager().ClearTimer(AgentDiscoveryTimerHandle);
	}
	AgentDiscoveryTimerHandle.Invalidate();
	CachedPIEWorld.Reset();
	bArmedForPIE = false;
	if (TrainingManager && IsValid(TrainingManager))
	{
		SetWorkflowState(ENeatTrainingWorkflowState::ManagerInitialized);
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Armed training cancelled. Status: Manager ready."));
	}
	else
	{
		SetWorkflowState(ENeatTrainingWorkflowState::Idle);
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Armed training cancelled."));
	}
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
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] PIE hook entered; PIE world captured. Will poll runtime registry for self-registered agents (interval=%.1fs, max=%.0fs)."),
			AgentDiscoveryIntervalSeconds, AgentDiscoveryMaxDurationSeconds);

		// Use editor world timer so polling runs regardless of PIE world tick.
		UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (EditorWorld)
		{
			AgentDiscoveryPollIndex = 0;
			AgentDiscoveryStartTime = FPlatformTime::Seconds();
			EditorWorld->GetTimerManager().SetTimer(
				AgentDiscoveryTimerHandle,
				this,
				&UNeatTrainingEditorWidget::PollForRuntimeAgents,
				AgentDiscoveryIntervalSeconds,
				true
			);
		}
		else
		{
			UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] No editor world for discovery timer; cannot poll for agents."));
			SetWorkflowState(ENeatTrainingWorkflowState::TrainingFailed);
			LastStatusMessage = TEXT("ERROR: No editor world for agent discovery timer.");
		}
	}
	else
	{
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] PIE started but world not resolved; staying in WaitingForPIEWorld."));
	}
}

void UNeatTrainingEditorWidget::PollForRuntimeAgents()
{
	UWorld* World = CachedPIEWorld.Get();
	if (!World || !IsValid(World))
	{
		if (WorkflowLogVerbosity >= 2)
		{
			UE_LOG(LogCarAIEditor, Log, TEXT("[NeatTrainingEditorWidget] Poll #%d: skipped — PIE world invalid or null."), AgentDiscoveryPollIndex);
		}
		return;
	}
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		if (WorkflowLogVerbosity >= 2)
		{
			UE_LOG(LogCarAIEditor, Log, TEXT("[NeatTrainingEditorWidget] Poll #%d: skipped — manager null or invalid."), AgentDiscoveryPollIndex);
		}
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - AgentDiscoveryStartTime;
	AgentDiscoveryPollIndex++;

	// ——— Registry-driven discovery: canonical source is runtime registry (agents self-register in BeginPlay). ———
	URacingAgentRegistrySubsystem* Registry = World->GetSubsystem<URacingAgentRegistrySubsystem>();
	if (!Registry)
	{
		if (WorkflowLogVerbosity >= 2)
		{
			UE_LOG(LogCarAIEditor, Log, TEXT("[NeatTrainingEditorWidget] Poll #%d | elapsed=%.1fs | state=%s | world=%s | registry=null (PIE world may not have subsystem yet)."),
				AgentDiscoveryPollIndex, Elapsed, WorkflowStateToShortString(WorkflowState), *World->GetName());
		}
		return;
	}

	TArray<URacingAgentComponent*> FoundInWorld = Registry->GetRegisteredAgents();
	const int32 DiscoveredCount = FoundInWorld.Num();

	// Per-poll summary: registry count is the source of truth.
	if (WorkflowLogVerbosity >= 2)
	{
		UE_LOG(LogCarAIEditor, Log, TEXT("[NeatTrainingEditorWidget] Poll #%d | elapsed=%.1fs | state=%s | world=%s | registry_count=%d (registry-driven discovery)."),
			AgentDiscoveryPollIndex, Elapsed, WorkflowStateToShortString(WorkflowState), *World->GetName(), DiscoveredCount);
	}
	if (AgentDiscoveryPollIndex == 1 && WorkflowLogVerbosity >= 1)
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] PIE world captured; runtime registry resolved. Polling registry for agent count (interval=%.1fs, max=%.0fs)."),
			AgentDiscoveryIntervalSeconds, AgentDiscoveryMaxDurationSeconds);
	}

	// Timeout: fail with full diagnostics
	if (Elapsed >= AgentDiscoveryMaxDurationSeconds)
	{
		UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
		if (EditorWorld)
		{
			EditorWorld->GetTimerManager().ClearTimer(AgentDiscoveryTimerHandle);
		}
		AgentDiscoveryTimerHandle.Invalidate();
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] Runtime agent discovery TIMEOUT (registry-driven). total_polls=%d elapsed=%.1fs last_registry_count=%d last_manager_registered=%d state=%s world=%s. Ensure GameFeature adds RacingAgentComponent so they self-register in BeginPlay."),
			AgentDiscoveryPollIndex, Elapsed, DiscoveredCount, TrainingManager->GetRegisteredAgentCount(), WorkflowStateToShortString(WorkflowState), *World->GetName());
		SetWorkflowState(ENeatTrainingWorkflowState::TrainingFailed, TEXT("Discovery timeout; zero agents in runtime registry."));
		LastStatusMessage = TEXT("Timeout: no agents in runtime registry. Ensure GameFeature adds RacingAgentComponent (they self-register in BeginPlay).");
		return;
	}

	// Hard log if registry count stays zero (agents self-register in BeginPlay; if they exist, registry should see them).
	if (DiscoveredCount == 0 && AgentDiscoveryPollIndex >= 10 && AgentDiscoveryPollIndex % 10 == 0)
	{
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] Still zero agents in registry after %d polls (%.1fs). RacingAgentComponent must call registry RegisterAgent in BeginPlay."), AgentDiscoveryPollIndex, Elapsed);
	}

	if (DiscoveredCount == 0)
	{
		return;
	}

	// Found agents: stop polling, register, then auto-start
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (EditorWorld)
	{
		EditorWorld->GetTimerManager().ClearTimer(AgentDiscoveryTimerHandle);
	}
	AgentDiscoveryTimerHandle.Invalidate();

	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Readiness passed: registry_count=%d. Auto-registration with manager triggered."), DiscoveredCount);
	SetWorkflowState(ENeatTrainingWorkflowState::RegisteringAgents, TEXT("Discovered agents via registry; registering with manager."));

	int32 NewlyRegistered = 0;
	int32 AlreadyRegistered = 0;
	int32 Rejected = 0;
	for (URacingAgentComponent* Comp : FoundInWorld)
	{
		if (!Comp || !IsValid(Comp)) { Rejected++; continue; }
		if (TrainingManager->RegisterAgent(Comp))
		{
			NewlyRegistered++;
		}
		else
		{
			AlreadyRegistered++;
		}
	}
	RegisteredAgentCount = TrainingManager->GetRegisteredAgentCount();

	if (WorkflowLogVerbosity >= 1)
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Registration summary: discovered=%d eligible=%d newly_registered=%d already_registered=%d rejected=%d total_registered=%d world=%s"),
			DiscoveredCount, DiscoveredCount, NewlyRegistered, AlreadyRegistered, Rejected, RegisteredAgentCount, *World->GetName());
	}

	SetWorkflowState(ENeatTrainingWorkflowState::AgentsRegistered, TEXT("Registration complete."));
	SetWorkflowState(ENeatTrainingWorkflowState::ReadyToStart, TEXT("Evaluating readiness for auto-start."));

	if (TryAutoStartTraining())
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Training auto-start triggered after agent registration (agents=%d)."), RegisteredAgentCount);
	}
	else
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Readiness check did not pass; training not started. Register/start manually if needed."));
	}
}

bool UNeatTrainingEditorWidget::TryAutoStartTraining()
{
	// Explicit readiness gate: every condition logged when checked.
	const bool bPieworldValid = CachedPIEWorld.IsValid() && IsValid(CachedPIEWorld.Get());
	const bool bManagerValid = TrainingManager && IsValid(TrainingManager);
	const int32 NRegistered = bManagerValid ? TrainingManager->GetRegisteredAgentCount() : 0;
	const bool bHasAgents = NRegistered > 0;
	const bool bStillArmed = bArmedForPIE;
	const bool bPythonSet = !PythonExecutable.IsEmpty();

	if (WorkflowLogVerbosity >= 1)
	{
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Readiness evaluation: PIE_world=%s manager=%s registered_agents=%d armed=%s python_set=%s"),
			bPieworldValid ? TEXT("yes") : TEXT("NO"), bManagerValid ? TEXT("yes") : TEXT("NO"), NRegistered, bStillArmed ? TEXT("yes") : TEXT("no"), bPythonSet ? TEXT("yes") : TEXT("NO"));
	}

	if (!bManagerValid)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] Readiness check FAILED: manager not initialized."));
		SetWorkflowState(ENeatTrainingWorkflowState::TrainingFailed, TEXT("Manager invalid."));
		LastStatusMessage = TEXT("Readiness failed: manager not initialized.");
		return false;
	}
	if (!bHasAgents)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] Readiness check FAILED: no agents registered."));
		SetWorkflowState(ENeatTrainingWorkflowState::TrainingFailed, TEXT("No agents registered."));
		LastStatusMessage = TEXT("Readiness failed: no agents registered.");
		return false;
	}
	if (!bStillArmed)
	{
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] Readiness check: not armed; auto-start skipped (user may have cancelled)."));
		return false;
	}
	if (!bPythonSet)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] Readiness check FAILED: Python executable not set."));
		SetWorkflowState(ENeatTrainingWorkflowState::TrainingFailed, TEXT("Python not set."));
		LastStatusMessage = TEXT("Readiness failed: set Python executable in config.");
		return false;
	}

	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Readiness check passed (PIE_world=%s manager=yes agents=%d armed=yes Python=yes). Auto-starting training (FreshStart=%s)."),
		bPieworldValid ? TEXT("yes") : TEXT("no"), NRegistered, bFreshStart ? TEXT("true") : TEXT("false"));
	SetWorkflowState(ENeatTrainingWorkflowState::TrainingStarting, TEXT("Readiness passed; starting training."));
	ApplyConfigToManager();
	TrainingManager->StartTraining();
	bTrainingInProgress = true;
	SetWorkflowState(ENeatTrainingWorkflowState::TrainingRunning);
	return true;
}

void UNeatTrainingEditorWidget::OnPIEEnded()
{
	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] PIE ended; deferred training request cancelled, clearing stale references."));
	// Clear discovery timer so no more polls run.
	if (GEditor && GEditor->GetEditorWorldContext().World())
	{
		GEditor->GetEditorWorldContext().World()->GetTimerManager().ClearTimer(AgentDiscoveryTimerHandle);
	}
	AgentDiscoveryTimerHandle.Invalidate();
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
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] RegisterAgents: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("ERROR: Initialize manager before registering agents");
		return;
	}

	UWorld* World = GetEditorOrPIEWorld();
	if (!World)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] RegisterAgents: no valid world found; enter PIE or open a level"));
		LastStatusMessage = TEXT("ERROR: No valid world; enter PIE or open a level");
		return;
	}

	TrainingManager->UnregisterAllAgents();

	int32 TriedCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<URacingAgentComponent*> AgentComps;
		(*It)->GetComponents<URacingAgentComponent>(AgentComps);
		for (URacingAgentComponent* Comp : AgentComps)
		{
			if (Comp && IsValid(Comp))
			{
				TrainingManager->RegisterAgent(Comp);
				TriedCount++;
			}
		}
	}

	RegisteredAgentCount = TrainingManager->GetRegisteredAgentCount();

	if (RegisteredAgentCount == 0)
	{
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] RegisterAgents: no URacingAgentComponent found in world (%s). Ensure agents are in the level and PIE is active."),
			*World->GetName());
		LastStatusMessage = TEXT("WARNING: No agents found; check level and PIE state");
	}
	else
	{
		SetWorkflowState(ENeatTrainingWorkflowState::AgentsRegistered);
		UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Agent registration: tried %d component(s), registered %d agent(s) in world '%s'"),
			TriedCount, RegisteredAgentCount, *World->GetName());
	}
}

// ============================================================================
// Training Control
// ============================================================================

void UNeatTrainingEditorWidget::StartTraining()
{
	if (!TrainingManager || !IsValid(TrainingManager))
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: manager not initialized; call Initialize Manager first"));
		LastStatusMessage = TEXT("ERROR: Initialize manager before starting training");
		return;
	}

	if (RegisteredAgentCount == 0)
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: no agents registered; call Register Agents first"));
		LastStatusMessage = TEXT("ERROR: No agents registered; call Register Agents first");
		return;
	}

	if (PythonExecutable.IsEmpty())
	{
		UE_LOG(LogCarAIEditor, Error, TEXT("[NeatTrainingEditorWidget] StartTraining: PythonExecutable is empty; set a valid Python path"));
		LastStatusMessage = TEXT("ERROR: Python executable is not set");
		return;
	}

	ApplyConfigToManager();

	SetWorkflowState(ENeatTrainingWorkflowState::TrainingStarting);
	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Training start requested: FreshStart=%s Agents=%d Generations=%d PopulationSize=%d MaxEpisodeDuration=%.1fs Python='%s'"),
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
		UE_LOG(LogCarAIEditor, Warning, TEXT("[NeatTrainingEditorWidget] StopTraining: no manager exists; nothing to stop"));
		LastStatusMessage = TEXT("No manager to stop");
		return;
	}

	UE_LOG(LogCarAIEditor, Display, TEXT("[NeatTrainingEditorWidget] Training stop requested"));
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
		bArmTrainingEnabled = false;
		bRegisterAgentsEnabled = false;
		bStartTrainingEnabled = false;
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

	// If manager finished or stopped, align workflow state so status text is correct.
	if (WorkflowState == ENeatTrainingWorkflowState::TrainingRunning && !bTrainingInProgress)
	{
		SetWorkflowState(ENeatTrainingWorkflowState::TrainingCompleted);
	}

	bArmTrainingEnabled = !bTrainingInProgress;
	bRegisterAgentsEnabled = (GetEditorOrPIEWorld() != nullptr);
	bStartTrainingEnabled = (RegisteredAgentCount > 0 && !PythonExecutable.IsEmpty());

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

	UE_LOG(LogCarAIEditor, Verbose, TEXT("[NeatTrainingEditorWidget] Config applied to manager: FreshStart=%s PopSize=%d NumGen=%d EpDur=%.1f Python='%s'"),
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
