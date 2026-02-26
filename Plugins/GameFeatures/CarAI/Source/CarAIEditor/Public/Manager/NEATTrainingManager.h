#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/RacingAgentTypes.h"
#include "NEATTrainingManager.generated.h"

class URacingAgentComponent;
class UPythonTrainingExecutor;
class USimpleNeuralNetwork;

/**
 * NEAT Training Manager
 *
 * Coordinates NEAT evolution training cycle:
 * 1. Spawn agents with genomes from Python
 * 2. Evaluate fitness (let agents run episodes)
 * 3. Export fitness back to Python
 * 4. Wait for Python to evolve next generation
 * 5. Load new genomes and repeat
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

	/** Export directory for fitness values */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString FitnessExportDir = TEXT("Saved/Training/Fitness");

	/** Input directory for genomes from Python */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString GenomeInputDir = TEXT("Saved/Training/NEAT");

	/** Python script path */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonScriptPath = TEXT("Content/Python/train_neat.py");

	/** Python executable */
	UPROPERTY(EditAnywhere, Category = "NEAT Config")
	FString PythonExecutable = TEXT("python");

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

	/** Assign genomes to agents */
	void AssignGenomesToAgents();

	/** Start episode evaluation for all agents */
	void StartEpisodeEvaluation();

	/** Check if all agents are done */
	bool AreAllAgentsDone() const;

	/** Export fitness values to JSON for Python */
	void ExportFitnessValues();

	/** Trigger Python training to evolve next generation */
	void TriggerPythonEvolution();

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

private:
	UPROPERTY() ENEATTrainingState TrainingState = ENEATTrainingState::Idle;
	UPROPERTY() FNEATTrainingStats TrainingStats;
	UPROPERTY() int32 CurrentGeneration = 0;
	UPROPERTY() TArray<TWeakObjectPtr<URacingAgentComponent>> Agents;
	UPROPERTY() TArray<FNEATGenomeData> CurrentGenomes;
	UPROPERTY() TMap<int32, float> GenomeFitnessMap; // genome_id -> fitness
	UPROPERTY() TObjectPtr<UPythonTrainingExecutor> PythonExecutor;
	UPROPERTY() float EvaluationTimeElapsed = 0.f;
	UPROPERTY() FTimerHandle EvaluationTickTimer;
	UPROPERTY() bool bWaitingForPython = false;
};