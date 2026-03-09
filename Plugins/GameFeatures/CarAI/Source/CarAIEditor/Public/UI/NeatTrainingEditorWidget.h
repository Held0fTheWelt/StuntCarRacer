#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "NeatTrainingEditorWidget.generated.h"

class UNEATTrainingManager;
class URacingAgentComponent;

/**
 * Editor-side workflow state for deferred NEAT training.
 * Separates "user requested training" (armed) from "runtime conditions ready to start now".
 * PIE lifecycle and auto-register/auto-start are driven by these states in later MVPs.
 */
UENUM(BlueprintType)
enum class ENeatTrainingWorkflowState : uint8
{
	Idle,
	ManagerInitialized,
	ArmedForPIE,
	WaitingForPIEWorld,
	WaitingForRuntimeAgents,
	RegisteringAgents,
	ReadyToStart,
	AgentsRegistered,
	TrainingStarting,
	TrainingRunning,
	TrainingFailed,
	TrainingCompleted,
	PIEEnded
};

/**
 * Real Editor Utility Widget (C++ base) for the NEAT training control surface.
 * This is the single source of editor workflow; no placeholder or data-only widget.
 *
 * This class is the editor-facing control panel only. All training logic lives in
 * UNEATTrainingManager. The widget creates and holds a manager instance, exposes
 * config properties editable in the Details panel, and calls into the manager.
 * Essential workflow (arm, discovery, registration, auto-start) is implemented here
 * and in the manager; the Blueprint may add UI only.
 *
 * Blueprint asset: Content/Editor/EUW_NeatTraining.uasset
 * The Blueprint must be reparented to this class in the Unreal Editor.
 *
 * Primary workflow (pre-PIE armed):
 *   1. Open EUW_NeatTraining via Window > Car AI > NEAT Training.
 *   2. Set config (FreshStart, PopulationSize, NumGenerations, etc.).
 *   3. Click Initialize Manager.
 *   4. Click Arm Training (or Start When Ready).
 *   5. Start PIE — the system will wait for PIE world and GameFeature-added agents, then auto-register and auto-start training.
 *
 * Secondary workflow (PIE already running):
 *   Initialize Manager, then Register Agents, then Start Training (manual).
 */
UCLASS()
class CARAIEDITOR_API UNeatTrainingEditorWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	// ===== Config (editable in Details panel or wired to BP UI) =====

	/** If true, Python discards its checkpoint and best genome on next start (fresh run). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NEAT Config")
	bool bFreshStart = false;

	/** Python executable name or full path (e.g. "python" or "C:/Python311/python.exe"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NEAT Config")
	FString PythonExecutable = TEXT("python");

	/** Population size — must match the value in neat_config.txt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NEAT Config", meta = (ClampMin = "1"))
	int32 PopulationSize = 50;

	/** Total generations to train in this session. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NEAT Config", meta = (ClampMin = "1"))
	int32 NumGenerations = 50;

	/** Maximum episode duration in seconds before a timeout fitness is assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NEAT Config", meta = (ClampMin = "1.0"))
	float MaxEpisodeDuration = 120.f;

	// ===== Status (Blueprint-readable for UI display) =====

	/** Current editor workflow state (armed, waiting for PIE, training running, etc.). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	ENeatTrainingWorkflowState WorkflowState = ENeatTrainingWorkflowState::Idle;

	/** True when the user has requested training to start automatically once PIE and agents are ready (pre-PIE "Arm Training" action). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bArmedForPIE = false;

	/** True when the manager has been created and is ready. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bManagerReady = false;

	/** Number of agents registered with the manager. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	int32 RegisteredAgentCount = 0;

	/** Current generation as reported by the manager (updated by Refresh Status). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	int32 CurrentGeneration = 0;

	/** True when the manager reports training is actively evaluating. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bTrainingInProgress = false;

	/** Last status message for display in the widget UI. Reflects current workflow state and what the tool is waiting for. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	FString LastStatusMessage = TEXT("Not initialized");

	/** True when "Arm Training" is valid (manager ready, not currently training). Use to enable primary action in UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bArmTrainingEnabled = false;
	/** True when "Register Agents" (manual fallback) is valid (manager + world available). Use to enable fallback button. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bRegisterAgentsEnabled = false;
	/** True when "Start Training" (manual fallback) is valid (manager + agents + Python). Use to enable fallback button. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bStartTrainingEnabled = false;

	// ===== Actions (primary: Initialize Manager → Arm Training → Start PIE) =====

	/**
	 * Create or reuse the NEAT training manager for this editor session.
	 * Safe to call multiple times. Must be called before Arm Training or any manual fallback.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void InitializeManager();

	/**
	 * Primary action: Arm training for automatic start when PIE and runtime agents are ready.
	 * After this, start PIE; the tool will wait for PIE world and GameFeature-added agents, then auto-register and auto-start.
	 * Label in UI: "Arm Training", "Start When Ready", or "Arm Training For PIE".
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void ArmTraining();

	/** Cancel an armed training request (stops waiting for PIE / agents). Safe to call anytime. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void CancelArmedTraining();

	/**
	 * Manual fallback: register agents in current world. Use when PIE is already running and you did not use Arm Training.
	 * Disable this button when bRegisterAgentsEnabled is false.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void RegisterAgents();

	/**
	 * Manual fallback: start training now. Use only when agents are already registered (e.g. after manual Register Agents).
	 * Disable this button when bStartTrainingEnabled is false.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void StartTraining();

	/**
	 * Stop any running training. Safe to call even if training is not running.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void StopTraining();

	/**
	 * Read the current state from the manager and update status properties.
	 * Call from a Timer or button to keep the UI in sync.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void RefreshStatus();

	/** Direct access to the manager for advanced Blueprint use. May return null before InitializeManager. */
	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	UNEATTrainingManager* GetManager() const { return TrainingManager; }

	/** Returns a short user-facing description of the current workflow state (for status text and logs). */
	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	FString GetWorkflowStateDescription() const;

	/** Called by CarAIEditor module when PIE has started. Advances workflow to WaitingForPIEWorld / WaitingForRuntimeAgents. */
	void OnPIEWorldStarted(class UWorld* PIEWorld);
	/** Called by CarAIEditor module when PIE has ended. Cancels deferred flow, clears stale refs, transitions to PIEEnded. */
	void OnPIEEnded();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** The NEAT training manager owned by this widget for the duration of the editor session. */
	UPROPERTY()
	TObjectPtr<UNEATTrainingManager> TrainingManager;

	/** Set workflow state, log the transition (with optional reason), and update LastStatusMessage. */
	void SetWorkflowState(ENeatTrainingWorkflowState NewState, const FString& Reason = FString());

	/** Apply current widget config properties to the manager before training starts. */
	void ApplyConfigToManager();

	/** Return PIE world if active, otherwise the editor world. Used for agent discovery. */
	UWorld* GetEditorOrPIEWorld() const;

	/**
	 * Shared registry-to-manager handoff: get runtime registry snapshot, register agents with manager (idempotent),
	 * run readiness, trigger auto-start. Used by OnPIEWorldStarted (immediate), OnAgentRegistered delegate, and fallback diagnostics.
	 * @return true if handoff succeeded (agents registered and/or training started).
	 */
	bool TryHandoffRegistryAgents(UWorld* PIEWorld, const FString& Reason);

	/** Callback for registry OnAgentRegistered delegate when immediate snapshot had zero agents. */
	UFUNCTION()
	void OnRegistryAgentRegistered(URacingAgentComponent* Agent);

	/** Fallback diagnostics only: periodic retry of TryHandoffRegistryAgents until success or timeout. Not the primary path. */
	void PollForRuntimeAgents();

	/** Run readiness gate (manager, agents, Python). If pass, start training automatically. Returns true if training was started. */
	bool TryAutoStartTraining();

	/** Clear fallback timer and unbind registry delegate. Called on handoff success, PIE end, and timeout. */
	void ClearFallbackDiscoveryState();

private:
	/** Cached PIE world when armed flow is waiting for agents. Cleared on PIE end. */
	TWeakObjectPtr<UWorld> CachedPIEWorld;

	/** Fallback diagnostics only: timer for periodic handoff retry. Not the primary path. */
	FTimerHandle AgentDiscoveryTimerHandle;
	/** When fallback discovery started (seconds, FPlatformTime::Seconds()). Used to enforce max wait. */
	double AgentDiscoveryStartTime = 0.0;

	static constexpr float AgentDiscoveryIntervalSeconds = 0.5f;
	static constexpr float AgentDiscoveryMaxDurationSeconds = 30.0f;

	/** Poll index for fallback diagnostics; incremented each PollForRuntimeAgents tick. */
	int32 AgentDiscoveryPollIndex = 0;

	/** True when OnAgentRegistered is bound to the PIE-world registry; used to unbind on handoff success, timeout, or PIE end. */
	bool bRegistryDelegateBound = false;

	/** Verbosity for workflow/poll logs: 0=Error/Warning only, 1=+Display (transitions), 2=+Log (per-poll), 3=+Verbose (per-component skip). */
	UPROPERTY(EditAnywhere, Category = "NEAT Config", meta = (ClampMin = "0", ClampMax = "3"))
	int32 WorkflowLogVerbosity = 2;
};
