#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "NeatTrainingEditorWidget.generated.h"

class UNEATTrainingManager;
class URacingAgentComponent;

/**
 * Editor Utility Widget C++ base for the NEAT training control surface.
 *
 * This class is the editor-facing control panel only. All training logic lives in
 * UNEATTrainingManager. The widget creates and holds a manager instance, exposes
 * config properties editable in the Details panel, and calls into the manager.
 *
 * Blueprint asset: Content/Editor/EUW_NeatTraining.uasset
 * The Blueprint must be reparented to this class in the Unreal Editor.
 *
 * Typical workflow:
 *   1. Open EUW_NeatTraining via Window > Car AI > NEAT Training.
 *   2. Set config (FreshStart, PopulationSize, NumGenerations, etc.).
 *   3. Click Initialize Manager.
 *   4. Enter PIE (or load the training level).
 *   5. Click Register Agents.
 *   6. Click Start Training.
 *   7. Monitor status via Refresh Status or watch the Output Log.
 *   8. Click Stop Training to end early.
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

	/** Last status message for display in the widget UI. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	FString LastStatusMessage = TEXT("Not initialized");

	// ===== Actions =====

	/**
	 * Create or reuse the NEAT training manager for this editor session.
	 * Safe to call multiple times — reuses the existing instance if valid.
	 * Must be called before RegisterAgents or StartTraining.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void InitializeManager();

	/**
	 * Find and register all URacingAgentComponent instances in the current
	 * PIE or editor world. Unregisters previous agents first.
	 * Requires the manager to be initialized.
	 */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "NEAT Training")
	void RegisterAgents();

	/**
	 * Apply current config and start NEAT training.
	 * Fails loudly if: no manager, no agents, empty Python executable.
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

private:
	/** The NEAT training manager owned by this widget for the duration of the editor session. */
	UPROPERTY()
	TObjectPtr<UNEATTrainingManager> TrainingManager;

	/** Apply current widget config properties to the manager before training starts. */
	void ApplyConfigToManager();

	/** Return PIE world if active, otherwise the editor world. Used for agent discovery. */
	UWorld* GetEditorOrPIEWorld() const;
};
