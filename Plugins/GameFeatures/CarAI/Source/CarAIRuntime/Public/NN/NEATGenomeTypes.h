#pragma once

#include "CoreMinimal.h"
#include "NEATGenomeTypes.generated.h"

/**
 * NEAT node activation function.
 * Must match train_neat.py activation_options: sigmoid, tanh, relu.
 */
UENUM(BlueprintType)
enum class ENEATActivation : uint8
{
	Invalid UMETA(DisplayName = "Invalid"),
	Sigmoid UMETA(DisplayName = "Sigmoid"),
	Tanh UMETA(DisplayName = "Tanh"),
	ReLU UMETA(DisplayName = "ReLU")
};

/** Single node in a NEAT genome (id, activation, bias, response). */
USTRUCT(BlueprintType)
struct FNEATNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NodeId = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ENEATActivation Activation = ENEATActivation::Invalid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Bias = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Response = 1.f;
};

/** Directed connection between two nodes. */
USTRUCT(BlueprintType)
struct FNEATConnection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 InNode = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 OutNode = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Weight = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bEnabled = true;
};

/**
 * Runtime representation of a NEAT genome (arbitrary topology).
 * Loaded from genome_*.json / best_genome.json produced by train_neat.py.
 * Input nodes: [0 .. NumInputs-1], Output nodes: [NumInputs .. NumInputs+NumOutputs-1], rest hidden.
 */
USTRUCT(BlueprintType)
struct FNEATGenome
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GenomeID = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Generation = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Fitness = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NumInputs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 NumOutputs = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FNEATNode> Nodes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FNEATConnection> Connections;

	/** Lookup node by id (index in Nodes is not necessarily node id). */
	const FNEATNode* FindNode(int32 NodeId) const;

	/** True if the genome passed validation (structure and activation support). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsValid = false;
};

/** Parse activation string from JSON; returns Invalid and logs if unsupported. */
CARAIRUNTIME_API ENEATActivation NEATActivationFromString(const FString& Name);

/** Convert FNEATGenome to legacy FNEATGenomeData for compatibility with existing manager code. */
struct FNEATGenomeData;
CARAIRUNTIME_API void NEATGenomeToGenomeData(const FNEATGenome& Genome, struct FNEATGenomeData& OutData);
