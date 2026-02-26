#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Types/RacingCurriculumTypes.h"

#include "RacingCurriculumBuilder.generated.h"

class UTrackFrameProviderComponent;

UCLASS()
class CARAIRUNTIME_API URacingCurriculumBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Curriculum")
	static bool BuildFromTrackProvider(
		UTrackFrameProviderComponent* TrackProvider,
		const FRacingCurriculumBuildSettings& Settings,
		float& OutSplineLengthCm,
		TArray<FRacingCurriculumSegment>& OutSegments
	);

private:
	static float WrapDistance(float S, float LengthCm, bool bLooped);

	static float ComputeCurvatureInvCm(
		UTrackFrameProviderComponent* TrackProvider,
		float DistanceCm,
		float WindowCm,
		bool bLooped,
		float SplineLengthCm,
		FVector& InOutForward
	);

	static int32 BuildTagMask(
		UTrackFrameProviderComponent* TrackProvider,
		float S,
		float SplineLengthCm,
		const FRacingCurriculumBuildSettings& Settings,
		FVector& InOutForward,
		float& OutCurvNormAbs,
		float& OutSlopeZ,
		bool& bOutRampApproach,
		bool& bOutOnRamp
	);

	static void ComputeSpeedAndSteerHints(
		const FRacingCurriculumBuildSettings& Settings,
		int32 TagMask,
		float CurvNormAbs,
		float SlopeZ,
		float& OutSpeedNorm,
		float& OutMaxSteer
	);
};
