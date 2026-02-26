// Yves Tanas 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/RacingCurriculumTypes.h"

#include "RacingCurriculumDataAsset.generated.h"

/**
 * Racing curriculum data asset
 * Defines track segments with metadata tags and random access helpers.
 */
UCLASS(BlueprintType)
class CARAIRUNTIME_API URacingCurriculumDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	FRacingCurriculumBuildSettings BuildSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	float SplineLengthCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	TArray<FRacingCurriculumSegment> Segments;

	/** Optional: dump stats on load */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDumpOnLoad = false;

public:
	// ------------------------------------------------------------
	// Main API
	// ------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Curriculum")
	bool FindSegmentAtDistance(float DistanceCm, FRacingCurriculumSegment& OutSegment) const;

	UFUNCTION(BlueprintCallable, Category = "Curriculum")
	bool GetRandomDistanceInTag(int32 InMask, float& OutDistanceCm, FRandomStream& Rng, bool bRequireAllTags = false) const;

	UFUNCTION(BlueprintCallable, Category = "Curriculum|Debug")
	void DumpTagStats() const;

public:
	// ------------------------------------------------------------
	// Lifecycle
	// ------------------------------------------------------------
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
