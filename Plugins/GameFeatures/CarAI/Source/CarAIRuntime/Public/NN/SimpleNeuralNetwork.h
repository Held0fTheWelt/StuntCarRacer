#pragma once

#include "CoreMinimal.h"
#include "RacingTrainingTypes.h"
#include "SimpleNeuralNetwork.generated.h"

/**
 * Einfache Dense Layer Implementierung.
 * Weights werden als Row-Major Matrix gespeichert: [OutputSize x InputSize]
 */
USTRUCT()
struct CARAIRUNTIME_API FDenseLayer
{
	GENERATED_BODY()

	UPROPERTY() int32 InputSize = 0;
	UPROPERTY() int32 OutputSize = 0;
	UPROPERTY() EActivationType Activation = EActivationType::ReLU;

	// Weights: [OutputSize x InputSize] flattened
	UPROPERTY() TArray<float> Weights;
	UPROPERTY() TArray<float> Biases;

	// Gradients (f�r Training)
	UPROPERTY() TArray<float> WeightGrads;
	UPROPERTY() TArray<float> BiasGrads;

	// Adam Optimizer State
	UPROPERTY() TArray<float> WeightM;  // First moment
	UPROPERTY() TArray<float> WeightV;  // Second moment
	UPROPERTY() TArray<float> BiasM;
	UPROPERTY() TArray<float> BiasV;

	// Cached f�r Backprop (nicht serialisiert)
	TArray<float> LastInput;
	TArray<float> LastPreActivation;
	TArray<float> LastOutput;

	void Initialize(int32 InSize, int32 OutSize, EActivationType Act, FRandomStream& Rng);
	void Forward(const TArray<float>& Input, TArray<float>& Output);
	void Backward(const TArray<float>& OutputGrad, TArray<float>& InputGrad);
	void ApplyGradients(float LearningRate, float Beta1, float Beta2, float Epsilon, int32 Step);
	void ZeroGradients();

	int32 GetNumParameters() const { return Weights.Num() + Biases.Num(); }
};

/**
 * Einfaches MLP (Multi-Layer Perceptron) f�r Policy und Value Networks.
 */
UCLASS(BlueprintType)
class CARAIRUNTIME_API USimpleNeuralNetwork : public UObject
{
	GENERATED_BODY()

public:
	/** Initialisiert das Netzwerk mit der gegebenen Konfiguration */
	void Initialize(const FNetworkConfig& Config, int32 Seed = 0);

	/** Forward Pass - berechnet Output f�r gegebenen Input */
	void Forward(const TArray<float>& Input, TArray<float>& PolicyOutput, float& ValueOutput);

	/** Forward nur f�r Policy (schneller) */
	void ForwardPolicy(const TArray<float>& Input, TArray<float>& PolicyOutput);

	/** Forward nur f�r Value */
	float ForwardValue(const TArray<float>& Input);

	/** Sample Action mit Gaussian Noise */
	FVehicleAction SampleAction(const TArray<float>& State, float NoiseStd, float& OutLogProb);

	/** Berechne Log-Probability einer Aktion */
	float ComputeLogProb(const TArray<float>& State, const FVehicleAction& Action);

	/** Trainingsschritt mit PPO */
	void TrainStep(
		const TArray<FTrainingExperience>& Batch,
		const FPPOHyperparameters& Params,
		float& OutPolicyLoss,
		float& OutValueLoss,
		float& OutEntropyLoss
	);

	/** Serialisierung - speichert als bin�re Datei */
	void SaveToFile(const FString& Filepath);
	bool LoadFromFile(const FString& Filepath);

	/** Setzt Gewichte fr Policy Layer (fr Import) */
	void SetPolicyLayerWeights(int32 LayerIndex, const TArray<float>& Weights, const TArray<float>& Biases);
	
	/** Setzt Gewichte fr Value Layer (fr Import) */
	void SetValueLayerWeights(int32 LayerIndex, const TArray<float>& Weights, const TArray<float>& Biases);
	
	/** Setzt Gewichte fr Policy Head (fr Import) */
	void SetPolicyHeadWeights(const TArray<float>& Weights, const TArray<float>& Biases);
	
	/** Setzt Gewichte fr Value Head (fr Import) */
	void SetValueHeadWeights(const TArray<float>& Weights, const TArray<float>& Biases);
	
	/** Setzt Action Log Std (fr Import) */
	void SetActionLogStd(const TArray<float>& LogStd);

	/** Netzwerk-Info */
	int32 GetNumParameters() const;
	bool IsInitialized() const { return bInitialized; }

	UPROPERTY() FNetworkConfig NetworkConfig;

private:
	UPROPERTY() TArray<FDenseLayer> PolicyLayers;
	UPROPERTY() TArray<FDenseLayer> ValueLayers;
	UPROPERTY() FDenseLayer PolicyHead;  // Output: Mean f�r jede Action
	UPROPERTY() FDenseLayer ValueHead;   // Output: Scalar Value

	// Log Std f�r Actions (lernbar)
	UPROPERTY() TArray<float> ActionLogStd;
	UPROPERTY() TArray<float> ActionLogStdGrad;
	UPROPERTY() TArray<float> ActionLogStdM;
	UPROPERTY() TArray<float> ActionLogStdV;

	UPROPERTY() int32 AdamStep = 0;
	UPROPERTY() bool bInitialized = false;

	FRandomStream Rng;

	// Hilfsfunktionen
	static void ApplyActivation(TArray<float>& X, EActivationType Act);
	static void ApplyActivationGrad(const TArray<float>& Output, const TArray<float>& Grad, TArray<float>& OutGrad, EActivationType Act);
	static float GaussianLogProb(float X, float Mean, float LogStd);
	static float SampleGaussian(FRandomStream& R, float Mean, float Std);
};

// ============================================================================
// Experience Replay Buffer
// ============================================================================

UCLASS(BlueprintType)
class CARAIRUNTIME_API UExperienceBuffer : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(int32 MaxSize);
	void Add(const FTrainingExperience& Exp);
	void AddBatch(const TArray<FTrainingExperience>& Exps);
	void Clear();

	/** Sample ein zuf�lliges Batch */
	void SampleBatch(int32 BatchSize, TArray<FTrainingExperience>& OutBatch, FRandomStream& Rng);

	/** Hole alle Experiences (f�r On-Policy wie PPO) */
	const TArray<FTrainingExperience>& GetAll() const { return Buffer; }
	TArray<FTrainingExperience>& GetAllMutable() { return Buffer; }

	int32 Num() const { return Buffer.Num(); }
	bool IsFull() const { return Buffer.Num() >= Capacity; }

	/** Berechne GAE (Generalized Advantage Estimation) */
	void ComputeGAE(float Gamma, float Lambda);

	/** Normalize Advantages */
	void NormalizeAdvantages();

private:
	UPROPERTY() TArray<FTrainingExperience> Buffer;
	UPROPERTY() int32 Capacity = 10000;
	UPROPERTY() int32 WriteIndex = 0;
};