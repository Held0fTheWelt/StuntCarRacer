#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/RacingAgentTypes.h"
#include "NN/NEATGenomeTypes.h"
#include "NEATTrainingManager.generated.h"

class URacingAgentComponent;
class UPythonTrainingExecutor;

// ============================================================================
// NEAT Training Contract (Unreal = single source of truth; passed to Python via manifest)
// ============================================================================

/**
 * Canonical contract for Unreal <-> Python NEAT training.
 * All paths are absolute. Python receives this via neat_contract.json and must use
 * these values exclusively (no hardcoded fallback paths).
 *
 * File naming (both sides must use):
 *   - Fitness: generation_{N}.json
 *   - Genomes list: generation_{N}_genomes.json
 *   - Genome: genome_{id}.json
 *   - Best genome: best_genome.json
 */
struct FNEATTrainingContract
{
	FString FitnessDir;
	FString GenomeDir;
	FString CheckpointDir;
	FString BestGenomePath;
	int32 ObservationSize = 15;  // Must match FRacingObservation (base without LIDAR)
	int32 ActionSize = 3;         // Steer, Throttle, Brake

	/** File naming: fitness export */
	static inline const TCHAR* FitnessFileNameFormat = TEXT("generation_%d.json");
	/** File naming: genomes list for a generation */
	static inline const TCHAR* GenomesListFileNameFormat = TEXT("generation_%d_genomes.json");
	/** File naming: single genome file */
	static inline const TCHAR* GenomeFileNameFormat = TEXT("genome_%d.json");
	/** File naming: best genome (inference) */
	static inline const TCHAR* BestGenomeFileName = TEXT("best_genome.json");

	/** Manifest filename written by Unreal, consumed by Python. Stored under Saved/Training/. */
	static inline const TCHAR* ManifestFileName = TEXT("neat_contract.json");
	/** Parent dir for manifest and NEAT subdirs (relative to ProjectSavedDir). */
	static inline const TCHAR* TrainingDirName = TEXT("Training");
	/**
	 * Canonical state file written by Python after every generation export (genome_dir/training_state.json).
	 * Contains exported_generation: the generation index Python just exported for Unreal to load.
	 * Unreal reads this to synchronize CurrentGeneration before calling LoadGenerationGenomes().
	 */
	static inline const TCHAR* TrainingStateFileName = TEXT("training_state.json");
};

/**
 * NEAT Training Manager
 *
 * Coordinates NEAT evolution training cycle:
 * 1. Load genomes from Python (generation_N_genomes.json + genome_*.json)
 * 2. Evaluate fitness in batches when population size > agent count (each genome evaluated exactly once)
 * 3. Export fitness only after all genomes in the generation have fitness (no partial export)
 * 4. Trigger Python to evolve next generation
 * 5. Repeat
 *
 * Generation evaluation: Loaded genome count must equal PopulationSize (fail fast otherwise).
 * Batch mode: genomes are assigned in waves to agents until all are evaluated; pending/active/completed
 * are tracked via CurrentBatchStartIndex, NumAgentsInCurrentBatch, GenomeFitnessMap.
 */
UCLASS(BlueprintType)
class CARAIEDITOR_API UNEATTrainingManager : public UObject
{
	GENERATED_BODY()

public:
	// ===== Configuration =====

	/** Number of generations to train */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	int32 NumGenerations = 50;

	/** Population size (must match neat_config.txt) */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	int32 PopulationSize = 50;

	/** Max episode duration (seconds) for each evaluation */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	float MaxEpisodeDuration = 120.f;

	/** Export directory for fitness values (relative to project saved dir). Canonical: Training/Fitness */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString FitnessExportDir = TEXT("Training/Fitness");

	/** Directory for genomes and checkpoint from Python (relative to project saved dir). Canonical: Training/NEAT */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString GenomeInputDir = TEXT("Training/NEAT");

	/** Python script: filename only (e.g. train_neat.py). Resolved relative to CarAI plugin Content/Python. Do not include Content/Python/ in the path. */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonScriptPath = TEXT("train_neat.py");

	/** Python executable */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonExecutable = TEXT("python");

	/**
	 * If true, start a fresh NEAT run: Python deletes its checkpoint and best genome before starting.
	 * If false (default), resume from the latest checkpoint if one exists.
	 * The chosen mode is written into the manifest (training_mode = "fresh" | "resume")
	 * so Python can act on it deterministically. Set this explicitly; do not rely on implicit behavior.
	 */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	bool bFreshStart = false;

	/** Observation size (must match FRacingObservation base size; used in NEAT config) */
	UPROPERTY(EditAnywhere, Category = "NEAT Config", meta = (ClampMin = "1"))
	int32 ObservationSize = 15;

	/** Action size (Steer, Throttle, Brake; must match FVehicleAction) */
	UPROPERTY(EditAnywhere, Category = "NEAT Config", meta = (ClampMin = "1"))
	int32 ActionSize = 3;

	// ===== Training Control =====

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void StartTraining();

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void StopTraining();

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void PauseTraining();

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void ResumeTraining();

	// ===== Agent Management =====

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void RegisterAgent(URacingAgentComponent* Agent);

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void UnregisterAgent(URacingAgentComponent* Agent);

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	void UnregisterAllAgents();

	// ===== Status =====

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	ENEATTrainingState GetTrainingState() const { return TrainingState; }

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	FNEATTrainingStats GetTrainingStats() const { return TrainingStats; }

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	int32 GetCurrentGeneration() const { return CurrentGeneration; }

	UFUNCTION(BlueprintCallable, Category = "NEAT Training")
	bool IsTraining() const { return TrainingState == ENEATTrainingState::Evaluating; }

	/** Returns the resolved contract (absolute paths). Log at startup to verify. */
	FNEATTrainingContract GetResolvedContract() const;

	// ===== Events =====

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGenerationComplete, int32, Generation);
	UPROPERTY(BlueprintAssignable, Category = "NEAT Training")
	FOnGenerationComplete OnGenerationComplete;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNewBestGenome, int32, GenomeID, float, Fitness);
	UPROPERTY(BlueprintAssignable, Category = "NEAT Training")
	FOnNewBestGenome OnNewBestGenome;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTrainingComplete);
	UPROPERTY(BlueprintAssignable, Category = "NEAT Training")
	FOnTrainingComplete OnTrainingComplete;

protected:
	// ===== Internal Methods =====

	/** Load genomes for current generation from Python export */
	bool LoadGenerationGenomes();

	/** Assign current batch of genomes to agents (uses CurrentBatchStartIndex). */
	void AssignGenomesToAgents();

	/** Start episode evaluation for agents in current batch */
	void StartEpisodeEvaluation();

	/** True if all agents in the current batch have finished their episode */
	bool AreAllAgentsDone() const;

	/** True when all genomes in this generation have been evaluated (ready to export). */
	bool IsGenerationFullyEvaluated() const;

	/** Export fitness values to JSON for Python */
	void ExportFitnessValues();

	/** Trigger Python training to evolve next generation */
	void TriggerPythonEvolution();

	/** Write manifest JSON for Python (Saved/Training/neat_contract.json). Returns path or empty on failure. */
	FString WriteContractManifest(const FNEATTrainingContract& Contract) const;

	/** Callback when Python evolution is complete */
	UFUNCTION()
	void OnPythonEvolutionComplete(bool bSuccess);

	/** Load a single genome from JSON */
	bool LoadGenomeFromJSON(const FString& FilePath, FNEATGenomeData& OutGenome);

	/** Load best genome for inference */
	bool LoadBestGenome();

	/**
	 * Read the exported generation from training_state.json written by Python after each export.
	 * Returns the exported_generation value, or -1 on any failure (file missing, parse error, field absent).
	 * This is the canonical source of truth for which generation Unreal must load next.
	 */
	int32 ReadExportedGeneration() const;

	/**
	 * Finalize best-genome artifact from the last evaluated generation's fitness file,
	 * then load it for inference. Must be called after ExportFitnessValues() and CurrentGeneration++.
	 * Reads generation_{CurrentGeneration-1}.json, finds best genome_id by fitness,
	 * copies genome_{best_id}.json to BestGenomePath, then calls LoadBestGenome().
	 */
	bool FinalizeAndLoadBestGenome();

	/** Tick function for evaluating episodes */
	void TickEvaluation(float DeltaTime);

	/** Episode completed callback */
	UFUNCTION()
	void OnAgentEpisodeDone(const FEpisodeStats& Stats);

	/** Log concise NEAT status: generation, active/completed genomes, last exported fitness file, etc. */
	void LogNEATStatusSummary(const TCHAR* Phase) const;

	/** Log generation evaluation progress: completed count, total, current batch size. Call at batch start and when batch completes. */
	void LogGenerationEvaluationProgress(const TCHAR* Phase) const;

private:
	UPROPERTY() ENEATTrainingState TrainingState = ENEATTrainingState::Idle;
	UPROPERTY() FNEATTrainingStats TrainingStats;
	UPROPERTY() int32 CurrentGeneration = 0;
	UPROPERTY() TArray<TWeakObjectPtr<URacingAgentComponent>> Agents;
	UPROPERTY() TArray<FNEATGenome> CurrentGenomes;
	UPROPERTY() TMap<int32, float> GenomeFitnessMap; // genome_id -> fitness (accumulated across batches)
	UPROPERTY() TObjectPtr<UPythonTrainingExecutor> PythonExecutor;
	UPROPERTY() float EvaluationTimeElapsed = 0.f;
	UPROPERTY() FTimerHandle EvaluationTickTimer;
	UPROPERTY() bool bWaitingForPython = false;

	/** Batch mode: index into CurrentGenomes for the start of the current batch */
	UPROPERTY() int32 CurrentBatchStartIndex = 0;
	/** Number of agents that were assigned a genome in the current batch (only these must finish) */
	UPROPERTY() int32 NumAgentsInCurrentBatch = 0;
	/** Last successfully exported fitness file path (for diagnostics). */
	UPROPERTY() FString LastExportedFitnessFilePath;
	/** Last successfully loaded best genome path (for diagnostics). */
	UPROPERTY() FString LastLoadedBestGenomePath;
};