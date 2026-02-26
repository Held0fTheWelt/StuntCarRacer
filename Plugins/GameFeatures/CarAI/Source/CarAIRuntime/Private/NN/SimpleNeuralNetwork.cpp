#include "NN/SimpleNeuralNetwork.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// ============================================================================
// Dense Layer Implementation
// ============================================================================

void FDenseLayer::Initialize(int32 InSize, int32 OutSize, EActivationType Act, FRandomStream& Rng)
{
	InputSize = InSize;
	OutputSize = OutSize;
	Activation = Act;

	const int32 NumWeights = InputSize * OutputSize;

	Weights.SetNum(NumWeights);
	Biases.SetNum(OutputSize);
	WeightGrads.SetNum(NumWeights);
	BiasGrads.SetNum(OutputSize);

	// Adam State
	WeightM.SetNumZeroed(NumWeights);
	WeightV.SetNumZeroed(NumWeights);
	BiasM.SetNumZeroed(OutputSize);
	BiasV.SetNumZeroed(OutputSize);

	// Xavier/He Initialization
	const float Scale = (Act == EActivationType::ReLU || Act == EActivationType::LeakyReLU)
		? FMath::Sqrt(2.f / InputSize)  // He
		: FMath::Sqrt(1.f / InputSize); // Xavier

	for (int32 i = 0; i < NumWeights; ++i)
	{
		Weights[i] = Rng.FRandRange(-Scale, Scale);
	}

	for (int32 i = 0; i < OutputSize; ++i)
	{
		Biases[i] = 0.f;
	}

	ZeroGradients();
}

void FDenseLayer::Forward(const TArray<float>& Input, TArray<float>& Output)
{
	check(Input.Num() == InputSize);

	LastInput = Input;
	LastPreActivation.SetNum(OutputSize);
	Output.SetNum(OutputSize);

	// Y = X * W^T + B
	for (int32 o = 0; o < OutputSize; ++o)
	{
		float Sum = Biases[o];
		const int32 RowStart = o * InputSize;

		for (int32 i = 0; i < InputSize; ++i)
		{
			Sum += Input[i] * Weights[RowStart + i];
		}

		LastPreActivation[o] = Sum;
	}

	// Apply activation
	Output = LastPreActivation;

	switch (Activation)
	{
	case EActivationType::ReLU:
		for (float& V : Output) V = FMath::Max(0.f, V);
		break;
	case EActivationType::Tanh:
		for (float& V : Output) V = FMath::Tanh(V);
		break;
	case EActivationType::Sigmoid:
		for (float& V : Output) V = 1.f / (1.f + FMath::Exp(-V));
		break;
	case EActivationType::LeakyReLU:
		for (float& V : Output) V = (V >= 0.f) ? V : 0.01f * V;
		break;
	default:
		break;
	}

	LastOutput = Output;
}

void FDenseLayer::Backward(const TArray<float>& OutputGrad, TArray<float>& InputGrad)
{
	check(OutputGrad.Num() == OutputSize);

	// Compute activation gradient
	TArray<float> PreActGrad;
	PreActGrad.SetNum(OutputSize);

	for (int32 o = 0; o < OutputSize; ++o)
	{
		float Grad = OutputGrad[o];
		const float PreAct = LastPreActivation[o];
		const float Out = LastOutput[o];

		switch (Activation)
		{
		case EActivationType::ReLU:
			Grad *= (PreAct > 0.f) ? 1.f : 0.f;
			break;
		case EActivationType::Tanh:
			Grad *= (1.f - Out * Out);
			break;
		case EActivationType::Sigmoid:
			Grad *= Out * (1.f - Out);
			break;
		case EActivationType::LeakyReLU:
			Grad *= (PreAct >= 0.f) ? 1.f : 0.01f;
			break;
		default:
			break;
		}

		PreActGrad[o] = Grad;
	}

	// Accumulate gradients
	for (int32 o = 0; o < OutputSize; ++o)
	{
		BiasGrads[o] += PreActGrad[o];
		const int32 RowStart = o * InputSize;

		for (int32 i = 0; i < InputSize; ++i)
		{
			WeightGrads[RowStart + i] += PreActGrad[o] * LastInput[i];
		}
	}

	// Compute input gradient
	InputGrad.SetNumZeroed(InputSize);
	for (int32 i = 0; i < InputSize; ++i)
	{
		float Sum = 0.f;
		for (int32 o = 0; o < OutputSize; ++o)
		{
			Sum += PreActGrad[o] * Weights[o * InputSize + i];
		}
		InputGrad[i] = Sum;
	}
}

void FDenseLayer::ApplyGradients(float LearningRate, float Beta1, float Beta2, float Epsilon, int32 Step)
{
	const float BC1 = 1.f - FMath::Pow(Beta1, Step);
	const float BC2 = 1.f - FMath::Pow(Beta2, Step);

	// Update Weights
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		const float G = WeightGrads[i];

		WeightM[i] = Beta1 * WeightM[i] + (1.f - Beta1) * G;
		WeightV[i] = Beta2 * WeightV[i] + (1.f - Beta2) * G * G;

		const float MHat = WeightM[i] / BC1;
		const float VHat = WeightV[i] / BC2;

		Weights[i] -= LearningRate * MHat / (FMath::Sqrt(VHat) + Epsilon);
	}

	// Update Biases
	for (int32 i = 0; i < Biases.Num(); ++i)
	{
		const float G = BiasGrads[i];

		BiasM[i] = Beta1 * BiasM[i] + (1.f - Beta1) * G;
		BiasV[i] = Beta2 * BiasV[i] + (1.f - Beta2) * G * G;

		const float MHat = BiasM[i] / BC1;
		const float VHat = BiasV[i] / BC2;

		Biases[i] -= LearningRate * MHat / (FMath::Sqrt(VHat) + Epsilon);
	}
}

void FDenseLayer::ZeroGradients()
{
	for (float& G : WeightGrads) G = 0.f;
	for (float& G : BiasGrads) G = 0.f;
}

// ============================================================================
// Simple Neural Network Implementation
// ============================================================================

void USimpleNeuralNetwork::Initialize(const FNetworkConfig& InConfig, int32 Seed)
{
	NetworkConfig = InConfig;

	if (Seed != 0)
	{
		Rng.Initialize(Seed);
	}
	else
	{
		Rng.GenerateNewSeed();
	}

	// Policy Network: Shared layers + Policy Head
	PolicyLayers.Reset();
	int32 LastSize = NetworkConfig.InputSize;

	for (const FDenseLayerConfig& LC : NetworkConfig.HiddenLayers)
	{
		FDenseLayer Layer;
		Layer.Initialize(LastSize, LC.OutputSize, LC.Activation, Rng);
		PolicyLayers.Add(Layer);
		LastSize = LC.OutputSize;
	}

	// Policy Head: outputs action means
	PolicyHead.Initialize(LastSize, NetworkConfig.PolicyOutputSize, EActivationType::Tanh, Rng);

	// Value Network: Separate layers + Value Head
	ValueLayers.Reset();
	LastSize = NetworkConfig.InputSize;

	for (const FDenseLayerConfig& LC : NetworkConfig.HiddenLayers)
	{
		FDenseLayer Layer;
		Layer.Initialize(LastSize, LC.OutputSize, LC.Activation, Rng);
		ValueLayers.Add(Layer);
		LastSize = LC.OutputSize;
	}

	// Value Head: outputs single scalar
	ValueHead.Initialize(LastSize, NetworkConfig.ValueOutputSize, EActivationType::None, Rng);

	// Learnable log std for actions
	ActionLogStd.SetNum(NetworkConfig.PolicyOutputSize);
	ActionLogStdGrad.SetNum(NetworkConfig.PolicyOutputSize);
	ActionLogStdM.SetNumZeroed(NetworkConfig.PolicyOutputSize);
	ActionLogStdV.SetNumZeroed(NetworkConfig.PolicyOutputSize);

	for (int32 i = 0; i < NetworkConfig.PolicyOutputSize; ++i)
	{
		ActionLogStd[i] = FMath::Loge(0.5f); // Initial std = 0.5
	}

	AdamStep = 0;
	bInitialized = true;
}

void USimpleNeuralNetwork::Forward(const TArray<float>& Input, TArray<float>& PolicyOutput, float& ValueOutput)
{
	ForwardPolicy(Input, PolicyOutput);
	ValueOutput = ForwardValue(Input);
}

void USimpleNeuralNetwork::ForwardPolicy(const TArray<float>& Input, TArray<float>& PolicyOutput)
{
	TArray<float> Current = Input;
	TArray<float> Next;

	for (FDenseLayer& Layer : PolicyLayers)
	{
		Layer.Forward(Current, Next);
		Current = Next;
	}

	PolicyHead.Forward(Current, PolicyOutput);
}

float USimpleNeuralNetwork::ForwardValue(const TArray<float>& Input)
{
	TArray<float> Current = Input;
	TArray<float> Next;

	for (FDenseLayer& Layer : ValueLayers)
	{
		Layer.Forward(Current, Next);
		Current = Next;
	}

	TArray<float> ValueOut;
	ValueHead.Forward(Current, ValueOut);

	return (ValueOut.Num() > 0) ? ValueOut[0] : 0.f;
}

float USimpleNeuralNetwork::GaussianLogProb(float X, float Mean, float LogStd)
{
	const float Std = FMath::Exp(LogStd);
	const float Diff = X - Mean;
	return -0.5f * (Diff * Diff / (Std * Std) + 2.f * LogStd + FMath::Loge(2.f * PI));
}

float USimpleNeuralNetwork::SampleGaussian(FRandomStream& R, float Mean, float Std)
{
	// Box-Muller Transform
	const float U1 = FMath::Max(R.FRand(), 1e-7f);
	const float U2 = R.FRand();
	const float Z = FMath::Sqrt(-2.f * FMath::Loge(U1)) * FMath::Cos(2.f * PI * U2);
	return Mean + Std * Z;
}

FVehicleAction USimpleNeuralNetwork::SampleAction(const TArray<float>& State, float NoiseStd, float& OutLogProb)
{
	TArray<float> ActionMeans;
	ForwardPolicy(State, ActionMeans);

	FVehicleAction Action;
	OutLogProb = 0.f;

	if (ActionMeans.Num() >= 3)
	{
		// Sample with noise
		const float SteerStd = FMath::Exp(ActionLogStd[0]) * FMath::Max(NoiseStd, 0.01f);
		const float ThrottleStd = FMath::Exp(ActionLogStd[1]) * FMath::Max(NoiseStd, 0.01f);
		const float BrakeStd = FMath::Exp(ActionLogStd[2]) * FMath::Max(NoiseStd, 0.01f);

		Action.Steer = FMath::Clamp(SampleGaussian(Rng, ActionMeans[0], SteerStd), -1.f, 1.f);
		Action.Throttle = FMath::Clamp(SampleGaussian(Rng, ActionMeans[1], ThrottleStd), 0.f, 1.f);
		Action.Brake = FMath::Clamp(SampleGaussian(Rng, ActionMeans[2], BrakeStd), 0.f, 1.f);

		// Compute log probability
		OutLogProb += GaussianLogProb(Action.Steer, ActionMeans[0], ActionLogStd[0] + FMath::Loge(FMath::Max(NoiseStd, 0.01f)));
		OutLogProb += GaussianLogProb(Action.Throttle, ActionMeans[1], ActionLogStd[1] + FMath::Loge(FMath::Max(NoiseStd, 0.01f)));
		OutLogProb += GaussianLogProb(Action.Brake, ActionMeans[2], ActionLogStd[2] + FMath::Loge(FMath::Max(NoiseStd, 0.01f)));
	}

	return Action;
}

float USimpleNeuralNetwork::ComputeLogProb(const TArray<float>& State, const FVehicleAction& Action)
{
	TArray<float> ActionMeans;
	ForwardPolicy(State, ActionMeans);

	float LogProb = 0.f;

	if (ActionMeans.Num() >= 3)
	{
		LogProb += GaussianLogProb(Action.Steer, ActionMeans[0], ActionLogStd[0]);
		LogProb += GaussianLogProb(Action.Throttle, ActionMeans[1], ActionLogStd[1]);
		LogProb += GaussianLogProb(Action.Brake, ActionMeans[2], ActionLogStd[2]);
	}

	return LogProb;
}

void USimpleNeuralNetwork::TrainStep(
	const TArray<FTrainingExperience>& Batch,
	const FPPOHyperparameters& Params,
	float& OutPolicyLoss,
	float& OutValueLoss,
	float& OutEntropyLoss
)
{
	if (Batch.Num() == 0) return;

	AdamStep++;

	// Zero all gradients
	for (FDenseLayer& L : PolicyLayers) L.ZeroGradients();
	for (FDenseLayer& L : ValueLayers) L.ZeroGradients();
	PolicyHead.ZeroGradients();
	ValueHead.ZeroGradients();
	for (float& G : ActionLogStdGrad) G = 0.f;

	OutPolicyLoss = 0.f;
	OutValueLoss = 0.f;
	OutEntropyLoss = 0.f;

	const float InvBatchSize = 1.f / Batch.Num();

	for (const FTrainingExperience& Exp : Batch)
	{
		// Forward pass
		TArray<float> ActionMeans;
		ForwardPolicy(Exp.State, ActionMeans);
		const float ValuePred = ForwardValue(Exp.State);

		if (ActionMeans.Num() < 3) continue;

		// Compute new log prob
		const float NewLogProb =
			GaussianLogProb(Exp.Action.Steer, ActionMeans[0], ActionLogStd[0]) +
			GaussianLogProb(Exp.Action.Throttle, ActionMeans[1], ActionLogStd[1]) +
			GaussianLogProb(Exp.Action.Brake, ActionMeans[2], ActionLogStd[2]);

		// PPO Ratio
		const float Ratio = FMath::Exp(NewLogProb - Exp.LogProb);
		const float ClippedRatio = FMath::Clamp(Ratio, 1.f - Params.ClipRange, 1.f + Params.ClipRange);

		// Policy Loss (negative because we want to maximize)
		const float PolicyLoss = -FMath::Min(Ratio * Exp.Advantage, ClippedRatio * Exp.Advantage);
		OutPolicyLoss += PolicyLoss * InvBatchSize;

		// Value Loss
		const float ValueLoss = FMath::Square(ValuePred - Exp.Return);
		OutValueLoss += ValueLoss * InvBatchSize * Params.ValueCoef;

		// Entropy (f�r Exploration)
		float Entropy = 0.f;
		for (int32 i = 0; i < 3; ++i)
		{
			Entropy += ActionLogStd[i] + 0.5f * FMath::Loge(2.f * PI * EULERS_NUMBER);
		}
		OutEntropyLoss -= Entropy * InvBatchSize * Params.EntropyCoef;

		// Backward pass (vereinfacht - volle Backprop w�re komplexer)
		// Hier nur approximative Gradients f�r Demo-Zwecke

		// Policy gradient approximation
		const float PolicyGradScale = -Exp.Advantage * Ratio * InvBatchSize;

		TArray<float> PolicyHeadGrad;
		PolicyHeadGrad.SetNum(3);
		for (int32 i = 0; i < 3; ++i)
		{
			float Action_i = (i == 0) ? Exp.Action.Steer : (i == 1) ? Exp.Action.Throttle : Exp.Action.Brake;
			float Mean_i = ActionMeans[i];
			float Std_i = FMath::Exp(ActionLogStd[i]);

			// d/d_mean of log_prob = (action - mean) / std^2
			PolicyHeadGrad[i] = PolicyGradScale * (Action_i - Mean_i) / (Std_i * Std_i);

			// d/d_logstd of log_prob = (action - mean)^2 / std^2 - 1
			ActionLogStdGrad[i] += PolicyGradScale * ((Action_i - Mean_i) * (Action_i - Mean_i) / (Std_i * Std_i) - 1.f);
		}

		// Backprop through policy network
		TArray<float> CurrentGrad = PolicyHeadGrad;
		TArray<float> NextGrad;
		PolicyHead.Backward(CurrentGrad, NextGrad);
		CurrentGrad = NextGrad;

		for (int32 i = PolicyLayers.Num() - 1; i >= 0; --i)
		{
			PolicyLayers[i].Backward(CurrentGrad, NextGrad);
			CurrentGrad = NextGrad;
		}

		// Value gradient
		const float ValueGradScale = 2.f * (ValuePred - Exp.Return) * Params.ValueCoef * InvBatchSize;
		TArray<float> ValueHeadGrad = { ValueGradScale };

		ValueHead.Backward(ValueHeadGrad, NextGrad);
		CurrentGrad = NextGrad;

		for (int32 i = ValueLayers.Num() - 1; i >= 0; --i)
		{
			ValueLayers[i].Backward(CurrentGrad, NextGrad);
			CurrentGrad = NextGrad;
		}
	}

	// Apply gradients with Adam
	const float Beta1 = 0.9f;
	const float Beta2 = 0.999f;
	const float Epsilon = 1e-8f;

	for (FDenseLayer& L : PolicyLayers)
	{
		L.ApplyGradients(Params.LearningRate, Beta1, Beta2, Epsilon, AdamStep);
	}
	PolicyHead.ApplyGradients(Params.LearningRate, Beta1, Beta2, Epsilon, AdamStep);

	for (FDenseLayer& L : ValueLayers)
	{
		L.ApplyGradients(Params.LearningRate, Beta1, Beta2, Epsilon, AdamStep);
	}
	ValueHead.ApplyGradients(Params.LearningRate, Beta1, Beta2, Epsilon, AdamStep);

	// Update ActionLogStd with Adam
	const float BC1 = 1.f - FMath::Pow(Beta1, AdamStep);
	const float BC2 = 1.f - FMath::Pow(Beta2, AdamStep);

	for (int32 i = 0; i < ActionLogStd.Num(); ++i)
	{
		const float G = ActionLogStdGrad[i];
		ActionLogStdM[i] = Beta1 * ActionLogStdM[i] + (1.f - Beta1) * G;
		ActionLogStdV[i] = Beta2 * ActionLogStdV[i] + (1.f - Beta2) * G * G;

		const float MHat = ActionLogStdM[i] / BC1;
		const float VHat = ActionLogStdV[i] / BC2;

		ActionLogStd[i] -= Params.LearningRate * MHat / (FMath::Sqrt(VHat) + Epsilon);
		ActionLogStd[i] = FMath::Clamp(ActionLogStd[i], FMath::Loge(0.01f), FMath::Loge(2.f));
	}
}

void USimpleNeuralNetwork::SaveToFile(const FString& Filepath)
{
	TArray<uint8> Data;
	FMemoryWriter Writer(Data);

	// Schreibe Netzwerk-Config
	Writer << NetworkConfig.InputSize;
	Writer << NetworkConfig.PolicyOutputSize;
	Writer << NetworkConfig.ValueOutputSize;
	Writer << AdamStep;

	// Schreibe Layer-Anzahl
	int32 NumPolicyLayers = PolicyLayers.Num();
	int32 NumValueLayers = ValueLayers.Num();
	Writer << NumPolicyLayers;
	Writer << NumValueLayers;

	// Hilfsfunktion zum Schreiben eines Layers
	auto WriteLayer = [&Writer](const FDenseLayer& Layer)
		{
			Writer << const_cast<FDenseLayer&>(Layer).InputSize;
			Writer << const_cast<FDenseLayer&>(Layer).OutputSize;
			int32 ActInt = static_cast<int32>(Layer.Activation);
			Writer << ActInt;
			Writer << const_cast<TArray<float>&>(Layer.Weights);
			Writer << const_cast<TArray<float>&>(Layer.Biases);
		};

	// Schreibe alle Layers
	for (const FDenseLayer& L : PolicyLayers) WriteLayer(L);
	WriteLayer(PolicyHead);
	for (const FDenseLayer& L : ValueLayers) WriteLayer(L);
	WriteLayer(ValueHead);

	// Schreibe ActionLogStd
	Writer << ActionLogStd;

	// Speichere Datei
	FFileHelper::SaveArrayToFile(Data, *Filepath);

	UE_LOG(LogTemp, Log, TEXT("Neural Network saved to: %s (%d bytes)"), *Filepath, Data.Num());
}

bool USimpleNeuralNetwork::LoadFromFile(const FString& Filepath)
{
	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Filepath))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load file: %s"), *Filepath);
		return false;
	}

	FMemoryReader Reader(Data);

	// Lese Netzwerk-Config
	Reader << NetworkConfig.InputSize;
	Reader << NetworkConfig.PolicyOutputSize;
	Reader << NetworkConfig.ValueOutputSize;
	Reader << AdamStep;

	// Lese Layer-Anzahl
	int32 NumPolicyLayers, NumValueLayers;
	Reader << NumPolicyLayers;
	Reader << NumValueLayers;

	// Hilfsfunktion zum Lesen eines Layers
	auto ReadLayer = [&Reader](FDenseLayer& Layer)
		{
			Reader << Layer.InputSize;
			Reader << Layer.OutputSize;
			int32 ActInt;
			Reader << ActInt;
			Layer.Activation = static_cast<EActivationType>(ActInt);
			Reader << Layer.Weights;
			Reader << Layer.Biases;

			// Initialisiere Gradient-Arrays
			Layer.WeightGrads.SetNumZeroed(Layer.Weights.Num());
			Layer.BiasGrads.SetNumZeroed(Layer.Biases.Num());
			Layer.WeightM.SetNumZeroed(Layer.Weights.Num());
			Layer.WeightV.SetNumZeroed(Layer.Weights.Num());
			Layer.BiasM.SetNumZeroed(Layer.Biases.Num());
			Layer.BiasV.SetNumZeroed(Layer.Biases.Num());
		};

	// Lese alle Layers
	PolicyLayers.SetNum(NumPolicyLayers);
	for (FDenseLayer& L : PolicyLayers) ReadLayer(L);
	ReadLayer(PolicyHead);

	ValueLayers.SetNum(NumValueLayers);
	for (FDenseLayer& L : ValueLayers) ReadLayer(L);
	ReadLayer(ValueHead);

	// Lese ActionLogStd
	Reader << ActionLogStd;
	ActionLogStdGrad.SetNumZeroed(ActionLogStd.Num());
	ActionLogStdM.SetNumZeroed(ActionLogStd.Num());
	ActionLogStdV.SetNumZeroed(ActionLogStd.Num());

	bInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("Neural Network loaded from: %s"), *Filepath);
	return true;
}

int32 USimpleNeuralNetwork::GetNumParameters() const
{
	int32 Count = 0;
	for (const FDenseLayer& L : PolicyLayers) Count += L.GetNumParameters();
	for (const FDenseLayer& L : ValueLayers) Count += L.GetNumParameters();
	Count += PolicyHead.GetNumParameters();
	Count += ValueHead.GetNumParameters();
	Count += ActionLogStd.Num();
	return Count;
}

void USimpleNeuralNetwork::SetPolicyLayerWeights(int32 LayerIndex, const TArray<float>& Weights, const TArray<float>& Biases)
{
	if (!PolicyLayers.IsValidIndex(LayerIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPolicyLayerWeights: Invalid layer index %d"), LayerIndex);
		return;
	}

	FDenseLayer& Layer = PolicyLayers[LayerIndex];
	if (Weights.Num() == Layer.Weights.Num() && Biases.Num() == Layer.Biases.Num())
	{
		Layer.Weights = Weights;
		Layer.Biases = Biases;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPolicyLayerWeights: Size mismatch. Expected %d/%d, got %d/%d"),
			Layer.Weights.Num(), Layer.Biases.Num(), Weights.Num(), Biases.Num());
	}
}

void USimpleNeuralNetwork::SetValueLayerWeights(int32 LayerIndex, const TArray<float>& Weights, const TArray<float>& Biases)
{
	if (!ValueLayers.IsValidIndex(LayerIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetValueLayerWeights: Invalid layer index %d"), LayerIndex);
		return;
	}

	FDenseLayer& Layer = ValueLayers[LayerIndex];
	if (Weights.Num() == Layer.Weights.Num() && Biases.Num() == Layer.Biases.Num())
	{
		Layer.Weights = Weights;
		Layer.Biases = Biases;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetValueLayerWeights: Size mismatch. Expected %d/%d, got %d/%d"),
			Layer.Weights.Num(), Layer.Biases.Num(), Weights.Num(), Biases.Num());
	}
}

void USimpleNeuralNetwork::SetPolicyHeadWeights(const TArray<float>& Weights, const TArray<float>& Biases)
{
	if (Weights.Num() == PolicyHead.Weights.Num() && Biases.Num() == PolicyHead.Biases.Num())
	{
		PolicyHead.Weights = Weights;
		PolicyHead.Biases = Biases;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetPolicyHeadWeights: Size mismatch. Expected %d/%d, got %d/%d"),
			PolicyHead.Weights.Num(), PolicyHead.Biases.Num(), Weights.Num(), Biases.Num());
	}
}

void USimpleNeuralNetwork::SetValueHeadWeights(const TArray<float>& Weights, const TArray<float>& Biases)
{
	if (Weights.Num() == ValueHead.Weights.Num() && Biases.Num() == ValueHead.Biases.Num())
	{
		ValueHead.Weights = Weights;
		ValueHead.Biases = Biases;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetValueHeadWeights: Size mismatch. Expected %d/%d, got %d/%d"),
			ValueHead.Weights.Num(), ValueHead.Biases.Num(), Weights.Num(), Biases.Num());
	}
}

void USimpleNeuralNetwork::SetActionLogStd(const TArray<float>& LogStd)
{
	if (LogStd.Num() == ActionLogStd.Num())
	{
		ActionLogStd = LogStd;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SetActionLogStd: Size mismatch. Expected %d, got %d"),
			ActionLogStd.Num(), LogStd.Num());
	}
}

// ============================================================================
// Experience Buffer Implementation
// ============================================================================

void UExperienceBuffer::Initialize(int32 MaxSize)
{
	Capacity = MaxSize;
	Buffer.Reserve(MaxSize);
	Clear();
}

void UExperienceBuffer::Add(const FTrainingExperience& Exp)
{
	if (Buffer.Num() < Capacity)
	{
		Buffer.Add(Exp);
	}
	else
	{
		Buffer[WriteIndex] = Exp;
	}
	WriteIndex = (WriteIndex + 1) % Capacity;
}

void UExperienceBuffer::AddBatch(const TArray<FTrainingExperience>& Exps)
{
	for (const FTrainingExperience& E : Exps)
	{
		Add(E);
	}
}

void UExperienceBuffer::Clear()
{
	Buffer.Reset();
	WriteIndex = 0;
}

void UExperienceBuffer::SampleBatch(int32 BatchSize, TArray<FTrainingExperience>& OutBatch, FRandomStream& Rng)
{
	OutBatch.Reset();
	if (Buffer.Num() == 0) return;

	BatchSize = FMath::Min(BatchSize, Buffer.Num());
	OutBatch.Reserve(BatchSize);

	for (int32 i = 0; i < BatchSize; ++i)
	{
		const int32 Idx = Rng.RandRange(0, Buffer.Num() - 1);
		OutBatch.Add(Buffer[Idx]);
	}
}

void UExperienceBuffer::ComputeGAE(float Gamma, float Lambda)
{
	if (Buffer.Num() == 0) return;

	// Sortiere nach Agent und Zeit
	Buffer.Sort([](const FTrainingExperience& A, const FTrainingExperience& B)
		{
			if (A.AgentIndex != B.AgentIndex) return A.AgentIndex < B.AgentIndex;
			return A.Timestamp < B.Timestamp;
		});

	// GAE f�r jeden Agent
	int32 CurrentAgent = -1;
	float Gae = 0.f;

	for (int32 i = Buffer.Num() - 1; i >= 0; --i)
	{
		FTrainingExperience& Exp = Buffer[i];

		if (Exp.AgentIndex != CurrentAgent)
		{
			CurrentAgent = Exp.AgentIndex;
			Gae = 0.f;
		}

		float NextValue = 0.f;
		if (i + 1 < Buffer.Num() && Buffer[i + 1].AgentIndex == CurrentAgent && !Exp.bDone)
		{
			NextValue = Buffer[i + 1].Value;
		}

		const float Delta = Exp.Reward + Gamma * NextValue - Exp.Value;
		Gae = Delta + Gamma * Lambda * (Exp.bDone ? 0.f : Gae);

		Exp.Advantage = Gae;
		Exp.Return = Exp.Advantage + Exp.Value;
	}
}

void UExperienceBuffer::NormalizeAdvantages()
{
	if (Buffer.Num() < 2) return;

	float Mean = 0.f;
	for (const FTrainingExperience& E : Buffer)
	{
		Mean += E.Advantage;
	}
	Mean /= Buffer.Num();

	float Var = 0.f;
	for (const FTrainingExperience& E : Buffer)
	{
		Var += FMath::Square(E.Advantage - Mean);
	}
	Var /= Buffer.Num();

	const float Std = FMath::Sqrt(Var + 1e-8f);

	for (FTrainingExperience& E : Buffer)
	{
		E.Advantage = (E.Advantage - Mean) / Std;
	}
}