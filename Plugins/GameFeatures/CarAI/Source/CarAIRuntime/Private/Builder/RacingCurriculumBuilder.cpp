#include "Builder/RacingCurriculumBuilder.h"

#include "Components/TrackFrameProviderComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// NoSpawnZone support
#include "Actors/NoSpawnZoneActor.h"

static bool FindSplineLengthCm(UTrackFrameProviderComponent* TrackProvider, float& OutLenCm)
{
	OutLenCm = 0.f;
	if (!TrackProvider) return false;

	AActor* Owner = TrackProvider->GetOwner();
	if (!Owner) return false;

	USplineComponent* Spline = Owner->FindComponentByClass<USplineComponent>();
	if (!Spline) return false;

	OutLenCm = Spline->GetSplineLength();
	return (OutLenCm > 1.f);
}

float URacingCurriculumBuilder::WrapDistance(float S, float LengthCm, bool bLooped)
{
	if (LengthCm <= 1.f) return 0.f;

	if (bLooped)
	{
		float X = FMath::Fmod(S, LengthCm);
		if (X < 0.f) X += LengthCm;
		return X;
	}

	return FMath::Clamp(S, 0.f, LengthCm);
}

float URacingCurriculumBuilder::ComputeCurvatureInvCm(
	UTrackFrameProviderComponent* TrackProvider,
	float DistanceCm,
	float WindowCm,
	bool bLooped,
	float SplineLengthCm,
	FVector& InOutForward)
{
	if (!TrackProvider || SplineLengthCm <= 1.f)
	{
		return 0.f;
	}

	const float ds = FMath::Max(10.f, WindowCm);
	const float SA = WrapDistance(DistanceCm - ds, SplineLengthCm, bLooped);
	const float SB = WrapDistance(DistanceCm + ds, SplineLengthCm, bLooped);

	const FTrackFrame A = TrackProvider->ComputeFrameAtDistance(SA, InOutForward);
	InOutForward = A.Tangent.GetSafeNormal();

	const FTrackFrame B = TrackProvider->ComputeFrameAtDistance(SB, InOutForward);
	InOutForward = B.Tangent.GetSafeNormal();

	const FVector TA = A.Tangent.GetSafeNormal();
	const FVector TB = B.Tangent.GetSafeNormal();

	const float dT = (TB - TA).Size();
	return dT / FMath::Max(2.f * ds, 1.f); // ~1/cm
}

int32 URacingCurriculumBuilder::BuildTagMask(
	UTrackFrameProviderComponent* TrackProvider,
	float S,
	float SplineLengthCm,
	const FRacingCurriculumBuildSettings& Settings,
	FVector& InOutForward,
	float& OutCurvNormAbs,
	float& OutSlopeZ,
	bool& bOutRampApproach,
	bool& bOutOnRamp)
{
	OutCurvNormAbs = 0.f;
	OutSlopeZ = 0.f;
	bOutRampApproach = false;
	bOutOnRamp = false;

	if (!TrackProvider || SplineLengthCm <= 1.f)
	{
		return 0;
	}

	const float SS = WrapDistance(S, SplineLengthCm, Settings.bLoopedTrack);
	const FTrackFrame Base = TrackProvider->ComputeFrameAtDistance(SS, InOutForward);
	InOutForward = Base.Tangent.GetSafeNormal();

	const float SlopeZ = Base.Tangent.GetSafeNormal().Z;
	OutSlopeZ = SlopeZ;

	const float CurvInvCm = ComputeCurvatureInvCm(TrackProvider, SS, Settings.CurvatureWindowCm, Settings.bLoopedTrack, SplineLengthCm, InOutForward);
	const float CurvNorm = (Settings.CurvatureNormInvCm > KINDA_SMALL_NUMBER) ? (CurvInvCm / Settings.CurvatureNormInvCm) : 0.f;
	OutCurvNormAbs = FMath::Abs(CurvNorm);

	const float SAhead = WrapDistance(SS + Settings.RampLookaheadCm, SplineLengthCm, Settings.bLoopedTrack);
	const FTrackFrame Ahead = TrackProvider->ComputeFrameAtDistance(SAhead, InOutForward);
	InOutForward = Ahead.Tangent.GetSafeNormal();

	const float Rise = Ahead.ClosestPoint.Z - Base.ClosestPoint.Z;
	const float AheadSlopeZ = Ahead.Tangent.GetSafeNormal().Z;

	const bool bRampApproach = (Rise > Settings.RampRiseThresholdCm) && (AheadSlopeZ > Settings.RampTangentZThreshold);
	const bool bOnRamp = (SlopeZ > Settings.RampTangentZThreshold);

	bOutRampApproach = bRampApproach;
	bOutOnRamp = bOnRamp;

	int32 Mask = 0;

	if (OutCurvNormAbs > Settings.CornerCurvNormThreshold)
	{
		Mask |= (int32)ERacingCurriculumTag::Corner;
	}
	if (SlopeZ > Settings.UphillTangentZThreshold)
	{
		Mask |= (int32)ERacingCurriculumTag::Uphill;
	}
	if (SlopeZ < Settings.DownhillTangentZThreshold)
	{
		Mask |= (int32)ERacingCurriculumTag::Downhill;
	}
	if (bRampApproach)
	{
		Mask |= (int32)ERacingCurriculumTag::RampApproach;
	}
	if (bOnRamp)
	{
		Mask |= (int32)ERacingCurriculumTag::OnRamp;
	}

	return Mask;
}

void URacingCurriculumBuilder::ComputeSpeedAndSteerHints(
	const FRacingCurriculumBuildSettings& Settings,
	int32 TagMask,
	float CurvNormAbs,
	float SlopeZ,
	float& OutSpeedNorm,
	float& OutMaxSteer)
{
	OutSpeedNorm = Settings.SuggestedSpeedStraight;
	OutMaxSteer = 1.0f;

	const bool bCorner = (TagMask & (int32)ERacingCurriculumTag::Corner) != 0;
	const bool bDownhill = (TagMask & (int32)ERacingCurriculumTag::Downhill) != 0;
	const bool bRampApproach = (TagMask & (int32)ERacingCurriculumTag::RampApproach) != 0;
	const bool bOnRamp = (TagMask & (int32)ERacingCurriculumTag::OnRamp) != 0;

	if (bCorner)
	{
		const float t = FMath::Clamp(CurvNormAbs, 0.f, 1.f);
		OutSpeedNorm = FMath::Lerp(Settings.SuggestedSpeedStraight, Settings.SuggestedSpeedCorner, t);
	}

	if (bDownhill)
	{
		OutSpeedNorm = FMath::Min(OutSpeedNorm, Settings.SuggestedSpeedDownhill);
	}

	if (bRampApproach || bOnRamp)
	{
		OutSpeedNorm = FMath::Max(OutSpeedNorm, Settings.JumpMinSpeedNorm);
		OutMaxSteer = FMath::Min(OutMaxSteer, Settings.JumpMaxSteer);
	}

	if (SlopeZ < Settings.DownhillTangentZThreshold)
	{
		const float k = FMath::Clamp(FMath::Abs(SlopeZ), 0.f, 1.f);
		OutSpeedNorm = FMath::Min(OutSpeedNorm, FMath::Lerp(OutSpeedNorm, Settings.SuggestedSpeedDownhill, k));
	}
}

	static bool IsInAnyNoSpawnZone(UWorld* World, const FVector& WorldPoint)
	{
		if (!World)
		{
			return false;
		}

		for (TActorIterator<ANoSpawnZoneActor> It(World); It; ++It)
		{
			ANoSpawnZoneActor* Zone = *It;
			if (!IsValid(Zone))
			{
				continue;
			}

			// Use the ContainsPoint method from NoSpawnZoneActor
			if (Zone->ContainsPoint(WorldPoint))
			{
				return true;
			}
		}

		return false;
	}

	bool URacingCurriculumBuilder::BuildFromTrackProvider(
	UTrackFrameProviderComponent* TrackProvider,
	const FRacingCurriculumBuildSettings& Settings,
	float& OutSplineLengthCm,
	TArray<FRacingCurriculumSegment>& OutSegments)
{
	OutSegments.Reset();
	OutSplineLengthCm = 0.f;

	if (!TrackProvider)
	{
		return false;
	}

	if (!FindSplineLengthCm(TrackProvider, OutSplineLengthCm))
	{
		return false;
	}

	// Get World for NoSpawnZone checking
	AActor* Owner = TrackProvider->GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;

	const float Step = FMath::Max(10.f, Settings.SampleStepCm);
	const float Len = OutSplineLengthCm;

	FVector Fwd = FVector::ForwardVector;

	FRacingCurriculumSegment Current;
	bool bHasCurrent = false;

	for (float S = 0.f; S < Len; S += Step)
	{
		float CurvAbs = 0.f;
		float SlopeZ = 0.f;
		bool bRampApproach = false;
		bool bOnRamp = false;

		const int32 TagMask = BuildTagMask(TrackProvider, S, Len, Settings, Fwd, CurvAbs, SlopeZ, bRampApproach, bOnRamp);

		// Check if this point is in a NoSpawnZone
		bool bInNoSpawnZone = false;
		if (World)
		{
			const FTrackFrame Frame = TrackProvider->ComputeFrameAtDistance(S, Fwd);
			bInNoSpawnZone = IsInAnyNoSpawnZone(World, Frame.ClosestPoint);
		}

		// Skip segments that are in NoSpawnZones
		if (bInNoSpawnZone)
		{
			// End current segment if we have one
			if (bHasCurrent)
			{
				Current.EndDistanceCm = FMath::Min(Current.EndDistanceCm + Step, Len);
				OutSegments.Add(Current);
				bHasCurrent = false;
			}
			continue;
		}

		float SpeedHint = Settings.SuggestedSpeedStraight;
		float SteerHint = 1.0f;
		ComputeSpeedAndSteerHints(Settings, TagMask, CurvAbs, SlopeZ, SpeedHint, SteerHint);

		if (!bHasCurrent)
		{
			Current.StartDistanceCm = S;
			Current.EndDistanceCm = S;
			Current.TagMask = TagMask;
			Current.SuggestedSpeedNorm = SpeedHint;
			Current.MaxSteerHint = SteerHint;
			Current.Note = TEXT("");
			bHasCurrent = true;
			continue;
		}

		const bool bSameTags = (Current.TagMask == TagMask);
		const bool bCloseSpeed = FMath::Abs(Current.SuggestedSpeedNorm - SpeedHint) <= Settings.MergeSpeedTolerance;
		const bool bCloseSteer = FMath::Abs(Current.MaxSteerHint - SteerHint) <= Settings.MergeSteerTolerance;

		if (bSameTags && bCloseSpeed && bCloseSteer)
		{
			Current.EndDistanceCm = S;
		}
		else
		{
			Current.EndDistanceCm = FMath::Min(Current.EndDistanceCm + Step, Len);
			OutSegments.Add(Current);

			Current.StartDistanceCm = S;
			Current.EndDistanceCm = S;
			Current.TagMask = TagMask;
			Current.SuggestedSpeedNorm = SpeedHint;
			Current.MaxSteerHint = SteerHint;
			Current.Note = TEXT("");
		}
	}

	if (bHasCurrent)
	{
		Current.EndDistanceCm = Len;
		OutSegments.Add(Current);
	}

	return !OutSegments.IsEmpty();
}
