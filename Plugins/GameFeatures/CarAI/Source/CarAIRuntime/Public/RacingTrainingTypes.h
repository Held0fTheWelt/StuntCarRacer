#pragma once

#include "CoreMinimal.h"
#include "Types/RacingAgentTypes.h"
#include "RacingTrainingTypes.generated.h"

// ============================================================================
// Training Experience
// ============================================================================

/** Ein einzelner Erfahrungspunkt f�r das Training */
USTRUCT(BlueprintType)
struct FTrainingExperience
{
	GENERATED_BODY()

	/** Observation zum Zeitpunkt der Aktion */
	UPROPERTY() TArray<float> State;

	/** Ausgef�hrte Aktion */
	UPROPERTY() FVehicleAction Action;

	/** Erhaltener Reward */
	UPROPERTY() float Reward = 0.f;

	/** N�chster State (leer wenn Terminal) */
	UPROPERTY() TArray<float> NextState;

	/** War dies ein Terminal-State? */
	UPROPERTY() bool bDone = false;

	/** Log-Probability der Aktion (f�r PPO) */
	UPROPERTY() float LogProb = 0.f;

	/** Value-Sch�tzung (f�r GAE) */
	UPROPERTY() float Value = 0.f;

	/** Advantage (berechnet nach Rollout) */
	UPROPERTY() float Advantage = 0.f;

	/** Return (discounted sum of rewards) */
	UPROPERTY() float Return = 0.f;

	/** Agent-Index */
	UPROPERTY() int32 AgentIndex = 0;

	/** Zeitstempel */
	UPROPERTY() double Timestamp = 0.0;
};

// ============================================================================
// Episode Statistics
// ============================================================================
//
//USTRUCT(BlueprintType)
//struct FEpisodeStats
//{
//	GENERATED_BODY()
//
//	UPROPERTY(BlueprintReadOnly) int32 AgentIndex = 0;
//	UPROPERTY(BlueprintReadOnly) int32 EpisodeNumber = 0;
//	UPROPERTY(BlueprintReadOnly) float TotalReward = 0.f;
//	UPROPERTY(BlueprintReadOnly) int32 StepCount = 0;
//	UPROPERTY(BlueprintReadOnly) float DurationSeconds = 0.f;
//	UPROPERTY(BlueprintReadOnly) float ProgressMeters = 0.f;
//	UPROPERTY(BlueprintReadOnly) float MaxSpeed = 0.f;
//	UPROPERTY(BlueprintReadOnly) float AvgSpeed = 0.f;
//	UPROPERTY(BlueprintReadOnly) FString TerminationReason;
//	UPROPERTY(BlueprintReadOnly) double StartTime = 0.0;
//	UPROPERTY(BlueprintReadOnly) double EndTime = 0.0;
//};

USTRUCT(BlueprintType)
struct FTrainingSessionStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 TotalEpisodes = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalSteps = 0;
	UPROPERTY(BlueprintReadOnly) int32 TotalUpdates = 0;
	UPROPERTY(BlueprintReadOnly) float TotalTrainingTimeSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly) float AvgEpisodeReward = 0.f;
	UPROPERTY(BlueprintReadOnly) float AvgEpisodeLength = 0.f;
	UPROPERTY(BlueprintReadOnly) float AvgProgressMeters = 0.f;

	UPROPERTY(BlueprintReadOnly) float BestEpisodeReward = -FLT_MAX;
	UPROPERTY(BlueprintReadOnly) int32 BestEpisodeNumber = 0;

	// Gleitender Durchschnitt (letzte 100 Episoden)
	UPROPERTY(BlueprintReadOnly) float RewardMA100 = 0.f;
	UPROPERTY(BlueprintReadOnly) float LengthMA100 = 0.f;
	UPROPERTY(BlueprintReadOnly) float ProgressMA100 = 0.f;
	
	// Durchschnittlicher Reward pro Schritt (letzte 100 Episoden)
	UPROPERTY(BlueprintReadOnly) float AvgRewardPerStepMA100 = 0.f;

	// Loss-Werte
	UPROPERTY(BlueprintReadOnly) float LastPolicyLoss = 0.f;
	UPROPERTY(BlueprintReadOnly) float LastValueLoss = 0.f;
	UPROPERTY(BlueprintReadOnly) float LastEntropyLoss = 0.f;

	// Episoden-Historie f�r Graphen
	UPROPERTY() TArray<float> RewardHistory;
	UPROPERTY() TArray<float> LengthHistory;
	UPROPERTY() TArray<float> ProgressHistory;
};

// ============================================================================
// Neural Network Layer Types
// ============================================================================

UENUM(BlueprintType)
enum class EActivationType : uint8
{
	None,
	ReLU,
	Tanh,
	Sigmoid,
	Softmax,
	LeakyReLU
};

USTRUCT(BlueprintType)
struct FDenseLayerConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 OutputSize = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EActivationType Activation = EActivationType::ReLU;
};

USTRUCT(BlueprintType)
struct FNetworkConfig
{
	GENERATED_BODY()

	/** Input-Gr��e (wird automatisch aus Observation berechnet) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 InputSize = 0;

	/** Hidden Layers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FDenseLayerConfig> HiddenLayers;

	/** Output-Gr��e f�r Policy (3 = Steer, Throttle, Brake) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PolicyOutputSize = 3;

	/** Output-Gr��e f�r Value (1) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ValueOutputSize = 1;

	FNetworkConfig()
	{
		// Default: 2 Hidden Layers mit 128 Neuronen
		HiddenLayers.Add({ 128, EActivationType::ReLU });
		HiddenLayers.Add({ 128, EActivationType::ReLU });
	}
};

// ============================================================================
// Training Hyperparameters
// ============================================================================

USTRUCT(BlueprintType)
struct FPPOHyperparameters
{
	GENERATED_BODY()

	/** Learning Rate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.000001", ClampMax = "0.1"))
	float LearningRate = 0.0003f;

	/** Discount Factor (Gamma) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Gamma = 0.99f;

	/** GAE Lambda */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Lambda = 0.95f;

	/** PPO Clip Range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ClipRange = 0.2f;

	/** Value Loss Coefficient */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ValueCoef = 0.5f;

	/** Entropy Coefficient (Exploration) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntropyCoef = 0.01f;

	/** Max Gradient Norm (Clipping) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float MaxGradNorm = 0.5f;

	/** Batch Size f�r Updates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "16", ClampMax = "4096"))
	int32 BatchSize = 64;

	/** Anzahl Epochs pro Update */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "20"))
	int32 NumEpochs = 4;

	/** Steps pro Rollout bevor Update */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "64", ClampMax = "16384"))
	int32 RolloutSteps = 2048;

	/** Normalize Advantages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNormalizeAdvantages = true;

	/** Clip Value Loss */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bClipValueLoss = true;

	/** Value Clip Range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float ValueClipRange = 0.2f;
};

USTRUCT(BlueprintType)
struct FTrainingConfig
{
	GENERATED_BODY()

	/** Netzwerk-Konfiguration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network")
	FNetworkConfig Network;

	/** PPO Hyperparameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PPO")
	FPPOHyperparameters PPO;

	/** Action Noise (Exploration) - Std Dev f�r Gaussian Noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exploration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActionNoiseStd = 0.2f;

	/** Action Noise Decay pro Episode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exploration", meta = (ClampMin = "0.99", ClampMax = "1.0"))
	float ActionNoiseDecay = 0.9995f;

	/** Minimum Action Noise */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exploration", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float ActionNoiseMin = 0.05f;

	/** Max Steps pro Episode (0 = unbegrenzt) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode")
	int32 MaxStepsPerEpisode = 3000;

	/** Auto-Save alle N Episoden (0 = aus, Standard: 0 für PyTorch-Training) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoints")
	int32 AutoSaveEveryNEpisodes = 0;

	/** Checkpoint Verzeichnis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoints")
	FString CheckpointDirectory = TEXT("Saved/Training/Checkpoints");

	/** Experiment Name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Logging")
	FString ExperimentName = TEXT("RacingAI");

	/** Seed f�r Reproduzierbarkeit (0 = zuf�llig) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Misc")
	int32 RandomSeed = 0;

	/** Wenn true, wird PPO-Update übersprungen (nur Export für PyTorch) - verhindert Stockungen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
	bool bExportOnly = true;

	// ===== Auto-Training (PyTorch) =====
	
	/** Wenn true, wird automatisch PyTorch-Training gestartet und Modelle geladen */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training")
	bool bEnableAutoTraining = true;

	/** Anzahl Rollouts, nach denen automatisch exportiert und trainiert wird (0 = nur beim Stop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training", meta = (ClampMin = "0"))
	int32 AutoTrainAfterNRollouts = 30;

	/** Pfad zum Python-Training-Script (leer = automatisch finden) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training")
	FString PythonTrainingScriptPath;

	/** Pfad zum Python-Executable (leer = "python" aus PATH) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training")
	FString PythonExecutablePath = TEXT("python");

	/** Welches Modell nach Training geladen werden soll (0 = neuestes, >0 = spezifische Epoche) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training", meta = (ClampMin = "0"))
	int32 AutoLoadModelEpoch = 0; // 0 = neuestes, >0 = spezifische Epoche

	/** Wenn true, werden Export-Dateien nach erfolgreichem Training gelöscht (für frischeres Training) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training", meta = (EditCondition = "bEnableAutoTraining"))
	bool bClearExportsAfterTraining = true;

	/** Anzahl Epochen für Python-Training (Standard: 10) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Auto Training", meta = (EditCondition = "bEnableAutoTraining", ClampMin = "1", ClampMax = "50"))
	int32 PythonTrainingEpochs = 10;
};

// ============================================================================
// Training State
// ============================================================================

UENUM(BlueprintType)
enum class ETrainingState : uint8
{
	Idle,
	Running,
	Paused,
	Updating,  // W�hrend Gradient-Update
	Saving,
	Loading
};

USTRUCT(BlueprintType)
struct FAgentTrainingState
{
	GENERATED_BODY()

	UPROPERTY() int32 AgentIndex = 0;
	UPROPERTY() int32 CurrentEpisode = 0;
	UPROPERTY() int32 CurrentStep = 0;
	UPROPERTY() float EpisodeReward = 0.f;
	UPROPERTY() float EpisodeProgress = 0.f;
	UPROPERTY() double EpisodeStartTime = 0.0;

	// Rollout Buffer f�r diesen Agent
	UPROPERTY() TArray<FTrainingExperience> RolloutBuffer;
};