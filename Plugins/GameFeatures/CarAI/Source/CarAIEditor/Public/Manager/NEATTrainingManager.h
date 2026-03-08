#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/RacingAgentTypes.h"
#include "NN/NEATGenomeTypes.h"
#include "NEATTrainingManager.generated.h"

class URacingAgentComponent;
class UPythonTrainingExecutor;
class USimpleNeuralNetwork;

// ============================================================================
// NEAT Training Contract (Unreal = source of truth; passed to Python via manifest)
// ============================================================================

/** Single shared contract for Unreal <-> Python NEAT training. All paths absolute. */
struct FNEATTrainingContract
{
	FString FitnessDir;
	FString GenomeDir;
	FString CheckpointDir;
	FString BestGenomePath;
	int32 ObservationSize = 15;  // Must match FRacingObservation (base without LIDAR)
	int32 ActionSize = 3;        // Steer, Throttle, Brake

	/** Generation file naming: fitness = generation_{N}.json, genomes list = generation_{N}_genomes.json, genome file = genome_{id}.json */
	static inline const TCHAR* FitnessFileNameFormat = TEXT("generation_%d.json");
	static inline const TCHAR* GenomesListFileNameFormat = TEXT("generation_%d_genomes.json");
	static inline const TCHAR* GenomeFileNameFormat = TEXT("genome_%d.json");
	static inline const TCHAR* BestGenomeFileName = TEXT("best_genome.json");
};

/**
 * NEAT Training Manager
 *
 * Coordinates NEAT evolution training cycle:
 * 1. Spawn agents with genomes from Python
 * 2. Evaluate fitness (let agents run episodes)
 * 3. Export fitness back to Python
 * 4. Wait for Python to evolve next generation
 * 5. Load new genomes and repeat
 *
 * Path configuration is the single source of truth; Python receives a manifest JSON.
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

	/** Export directory for fitness values (relative to project saved dir, e.g. "Training/Fitness") */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString FitnessExportDir = TEXT("Training/Fitness");

	/** Input directory for genomes from Python (relative to project saved dir, e.g. "Training/NEAT") */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString GenomeInputDir = TEXT("Training/NEAT");

	/** Python script: name relative to Plugins/GameFeatures/CarAI/Content/Python (e.g. "train_neat.py") */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonScriptPath = TEXT("train_neat.py");

	/** Python executable */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonExecutable = TEXT("python");

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

	/** Write manifest JSON for Python; returns path or empty on failure */
	FString WriteContractManifest(const FNEATTrainingContract& Contract) const;

	/** Callback when Python evolution is complete */
	UFUNCTION()
	void OnPythonEvolutionComplete(bool bSuccess);

	/** Load a single genome from JSON */
	bool LoadGenomeFromJSON(const FString& FilePath, FNEATGenomeData& OutGenome);

	/** Load best genome for inference */
	bool LoadBestGenome();

	/** Tick function for evaluating episodes */
	void TickEvaluation(float DeltaTime);

	/** Episode completed callback */
	UFUNCTION()
	void OnAgentEpisodeDone(const FEpisodeStats& Stats);

	/** Log concise NEAT status: generation, active/completed genomes, last exported fitness file, etc. */
	void LogNEATStatusSummary(const TCHAR* Phase) const;

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