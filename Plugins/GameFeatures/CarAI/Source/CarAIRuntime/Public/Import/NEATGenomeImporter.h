#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NN/NEATGenomeTypes.h"
#include "NEATGenomeImporter.generated.h"

/**
 * Loads and validates NEAT genome JSON files (genome_*.json, best_genome.json)
 * produced by train_neat.py. Fail-fast on invalid structure or unsupported activation.
 * No execution; loading and validation only.
 */
UCLASS()
class CARAIRUNTIME_API UNEATGenomeImporter : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Load and validate a genome from a JSON file.
	 * @param FilePath Absolute or relative path to genome_*.json or best_genome.json
	 * @param OutGenome Filled on success; OutGenome.bIsValid set after validation
	 * @return true if file was read, parsed, and passed validation; false otherwise (logs reason)
	 */
	UFUNCTION(BlueprintCallable, Category = "NEAT", meta = (DisplayName = "Load NEAT Genome"))
	static bool LoadFromFile(const FString& FilePath, FNEATGenome& OutGenome);

	/**
	 * Validate a parsed genome: input/output counts, no duplicate node IDs,
	 * connections reference existing nodes, activation functions supported.
	 * Sets OutGenome.bIsValid and returns false on any failure (with log).
	 */
	static bool ValidateGenome(FNEATGenome& InOutGenome);
};
