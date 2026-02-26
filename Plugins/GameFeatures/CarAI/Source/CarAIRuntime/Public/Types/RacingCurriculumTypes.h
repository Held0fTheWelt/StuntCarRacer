#pragma once

#include "CoreMinimal.h"
#include "RacingCurriculumTypes.generated.h"


UENUM(BlueprintType, meta = (Bitflags))
enum class ERacingCurriculumTag : uint8
{
	None = 0 UMETA(Hidden),
	Corner = 1 << 0,
	Downhill = 1 << 1,
	Uphill = 1 << 2,
	RampApproach = 1 << 3,
	OnRamp = 1 << 4,
};
ENUM_CLASS_FLAGS(ERacingCurriculumTag)


USTRUCT(BlueprintType)
struct FRacingCurriculumBuildSettings
{
	GENERATED_BODY()

	// Sampling
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Sampling")
	float SampleStepCm = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Sampling")
	float CurvatureWindowCm = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Sampling")
	bool bLoopedTrack = true;

	// Normalization (should match your agent's CurvatureNormInvCm default)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Thresholds")
	float CurvatureNormInvCm = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Thresholds")
	float CornerCurvNormThreshold = 0.55f; // abs(curvNorm) > => Corner

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Thresholds")
	float UphillTangentZThreshold = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Thresholds")
	float DownhillTangentZThreshold = -0.18f;

	// Ramp detection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Ramp")
	float RampLookaheadCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Ramp")
	float RampRiseThresholdCm = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Ramp")
	float RampTangentZThreshold = 0.18f;

	// Speed hints (0..1 == your Obs.SpeedNorm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpeedHints")
	float SuggestedSpeedStraight = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpeedHints")
	float SuggestedSpeedCorner = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpeedHints")
	float SuggestedSpeedDownhill = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpeedHints")
	float JumpMinSpeedNorm = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpeedHints")
	float JumpMaxSteer = 0.18f;

	// Segment merge behavior
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Merge")
	float MergeSpeedTolerance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Merge")
	float MergeSteerTolerance = 0.05f;
};

USTRUCT(BlueprintType)
struct FRacingCurriculumSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	float StartDistanceCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	float EndDistanceCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum",
		meta = (Bitmask, BitmaskEnum = "/Script/CarAIRuntime.ERacingCurriculumTag"))
	int32 TagMask = 0;

	// Hints for curriculum / heuristics / policy shaping
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	float SuggestedSpeedNorm = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	float MaxSteerHint = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curriculum")
	FString Note;
};
