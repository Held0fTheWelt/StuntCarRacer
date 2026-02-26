#include "Debug/RacingCurriculumDebugActor.h"

#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "UObject/UnrealType.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"

// If your RoadSplineInterface exists (like in your Respawn subsystem), include it:
#include "Interfaces/RoadSplineInterface.h"

// NoSpawnZone support
#include "Actors/NoSpawnZoneActor.h"
#include "Components/BoxComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRacingCurriculumDebug, Log, All);

static float SafeAcos(float X)
{
	return FMath::Acos(FMath::Clamp(X, -1.f, 1.f));
}

static float ComputePitchDegFromForward(const FVector& Forward)
{
	FVector F = Forward;
	if (!F.Normalize()) return 0.f;
	const float XY = FVector2D(F.X, F.Y).Size();
	return FMath::RadiansToDegrees(FMath::Atan2(F.Z, XY));
}

ARacingCurriculumDebugActor::ARacingCurriculumDebugActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARacingCurriculumDebugActor::BeginPlay()
{
	Super::BeginPlay();
	RebuildInternal(false);
}

void ARacingCurriculumDebugActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildInternal(true);
}

#if WITH_EDITOR
void ARacingCurriculumDebugActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildInternal(true);
}
#endif

void ARacingCurriculumDebugActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bDraw)
	{
		return;
	}

	DrawAccum += DeltaSeconds;
	if (DrawAccum < DrawIntervalSeconds)
	{
		return;
	}
	DrawAccum = 0.f;

	DrawInternal();
}

void ARacingCurriculumDebugActor::RebuildNow()
{
	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: RebuildNow() aufgerufen"));
	RebuildInternal(false);
}

void ARacingCurriculumDebugActor::ClearRecordedSpawns()
{
	const int32 OldSpawnCount = SpawnEvents.Num();
	const int32 OldHotspotCount = HotspotsByBin.Num();
	SpawnEvents.Reset();
	HotspotsByBin.Reset();
	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: ClearRecordedSpawns() - %d Spawn-Events und %d Hotspots gelöscht"), OldSpawnCount, OldHotspotCount);
}

void ARacingCurriculumDebugActor::ReportAgentSpawn_Implementation(AActor* Agent, const FTransform& SpawnWorldTransform, FName Reason, float Score)
{
	EnsureTrackSpline();
	if (!TrackSpline || CachedSplineLengthCm <= 1.f)
	{
		return;
	}

	const FVector Loc = SpawnWorldTransform.GetLocation();
	const float Key = TrackSpline->FindInputKeyClosestToWorldLocation(Loc);
	const float S = TrackSpline->GetDistanceAlongSplineAtSplineInputKey(Key);

	FVector Fwd = TrackSpline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
	float CurvNormAbs = 0.f;
	float SlopeZ = 0.f;
	const int32 TagMask = BuildTagMask(S, CachedSplineLengthCm, Fwd, CurvNormAbs, SlopeZ);

	FRC_SpawnEvent E;
	E.TimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	E.Agent = Agent;
	E.Location = Loc;
	E.Rotation = SpawnWorldTransform.Rotator();
	E.S = S;
	E.TagMask = TagMask;
	E.Score = Score;
	E.Reason = Reason;

	SpawnEvents.Add(E);

	if (SpawnEvents.Num() > MaxRecordedSpawnEvents)
	{
		const int32 RemoveCount = SpawnEvents.Num() - MaxRecordedSpawnEvents;
		SpawnEvents.RemoveAt(0, RemoveCount, EAllowShrinking::No);
	}

	RebuildHotspots();
}

void ARacingCurriculumDebugActor::EnsureTrackSpline()
{
	TrackSpline = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogRacingCurriculumDebug, Warning, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Keine World gefunden"));
		return;
	}

	// Auto-find track actor
	if (!TrackActor && bAutoFindTrackActor)
	{
		TArray<AActor*> Tagged;
		UGameplayStatics::GetAllActorsWithTag(World, FName("Track"), Tagged);
		if (Tagged.Num() > 0)
		{
			TrackActor = Tagged[0];
			UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - TrackActor automatisch gefunden: %s"), *TrackActor->GetName());
		}
		else
		{
			UE_LOG(LogRacingCurriculumDebug, Warning, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Kein Actor mit Tag 'Track' gefunden"));
		}
	}

	if (!TrackActor)
	{
		UE_LOG(LogRacingCurriculumDebug, Warning, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - TrackActor ist nullptr"));
		return;
	}

	// 1) Prefer RoadSplineInterface (this is the important fix!)
	if (bPreferRoadSplineInterface &&
		TrackActor->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
	{
		TrackSpline = IRoadSplineInterface::Execute_GetRoadSpline(TrackActor);
		if (TrackSpline)
		{
			UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Spline über RoadSplineInterface gefunden"));
			return;
		}
		else
		{
			UE_LOG(LogRacingCurriculumDebug, Verbose, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - RoadSplineInterface gibt nullptr zurück, versuche Fallback"));
		}
	}

	// 2) Component by exact name
	if (TrackSplineComponentName != NAME_None)
	{
		TInlineComponentArray<USplineComponent*> Splines;
		TrackActor->GetComponents(Splines);

		for (USplineComponent* S : Splines)
		{
			if (S && S->GetFName() == TrackSplineComponentName)
			{
				TrackSpline = S;
				UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Spline per Name gefunden: %s"), *TrackSplineComponentName.ToString());
				return;
			}
		}
		UE_LOG(LogRacingCurriculumDebug, Verbose, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Keine Spline mit Name '%s' gefunden"), *TrackSplineComponentName.ToString());
	}

	// 3) Component by tag
	if (TrackSplineComponentTag != NAME_None)
	{
		TInlineComponentArray<USplineComponent*> Splines;
		TrackActor->GetComponents(Splines);

		for (USplineComponent* S : Splines)
		{
			if (!S) continue;
			if (S->ComponentHasTag(TrackSplineComponentTag))
			{
				TrackSpline = S;
				UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Spline per Tag gefunden: %s"), *TrackSplineComponentTag.ToString());
				return;
			}
		}
		UE_LOG(LogRacingCurriculumDebug, Verbose, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Keine Spline mit Tag '%s' gefunden"), *TrackSplineComponentTag.ToString());
	}

	// 4) Fallback: first spline
	TrackSpline = TrackActor->FindComponentByClass<USplineComponent>();
	if (TrackSpline)
	{
		UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Spline per Fallback (erste gefundene) gefunden"));
	}
	else
	{
		UE_LOG(LogRacingCurriculumDebug, Error, TEXT("RacingCurriculumDebugActor: EnsureTrackSpline() - Keine Spline gefunden! TrackActor: %s"), *TrackActor->GetName());
	}
}

void ARacingCurriculumDebugActor::RebuildInternal(bool bFromConstruction)
{
	(void)bFromConstruction;

	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: RebuildInternal() gestartet (bFromConstruction=%d)"), bFromConstruction ? 1 : 0);

	EnsureTrackSpline();

	CachedSamples.Reset();
	CachedSegments.Reset();
	CachedSplineLengthCm = 0.f;

	if (!TrackSpline)
	{
		UE_LOG(LogRacingCurriculumDebug, Warning, TEXT("RacingCurriculumDebugActor: RebuildInternal() - TrackSpline nicht gefunden! TrackActor=%s, bAutoFindTrackActor=%d"), 
			TrackActor ? *TrackActor->GetName() : TEXT("nullptr"), bAutoFindTrackActor ? 1 : 0);
		bBuiltOnce = false;
		return;
	}

	CachedSplineLengthCm = TrackSpline->GetSplineLength();
	if (CachedSplineLengthCm <= 1.f)
	{
		UE_LOG(LogRacingCurriculumDebug, Warning, TEXT("RacingCurriculumDebugActor: RebuildInternal() - Spline-Länge zu klein: %.1f cm"), CachedSplineLengthCm);
		bBuiltOnce = false;
		return;
	}

	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: TrackSpline gefunden - Länge: %.1f m"), CachedSplineLengthCm / 100.f);

	// Optional: keep segments (not required for the visuals)
	CachedSegments.Reset();

	BuildSamples();
	const int32 NumSamples = CachedSamples.Num();
	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: BuildSamples() abgeschlossen - %d Samples erstellt"), NumSamples);

	RebuildHotspots();
	const int32 NumHotspots = HotspotsByBin.Num();
	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: RebuildHotspots() abgeschlossen - %d Hotspots"), NumHotspots);

	bBuiltOnce = true;
	UE_LOG(LogRacingCurriculumDebug, Log, TEXT("RacingCurriculumDebugActor: RebuildInternal() abgeschlossen - bBuiltOnce=true"));
}

float ARacingCurriculumDebugActor::WrapDistance(float S, float LengthCm) const
{
	if (LengthCm <= 1.f) return 0.f;

	const bool bLoop = TrackSpline ? TrackSpline->IsClosedLoop() : false;
	if (!bLoop)
	{
		return FMath::Clamp(S, 0.f, LengthCm);
	}

	float X = FMath::Fmod(S, LengthCm);
	if (X < 0.f) X += LengthCm;
	return X;
}

bool ARacingCurriculumDebugActor::ChooseBestSurfaceHit(const TArray<FHitResult>& Hits, FHitResult& OutBest) const
{
	if (Hits.Num() == 0)
	{
		return false;
	}

	// Hits are usually already sorted along the ray, but we keep it deterministic anyway.
	int32 BestIdx = INDEX_NONE;

	if (bPreferTrackActorHits && TrackActor)
	{
		for (int32 i = 0; i < Hits.Num(); ++i)
		{
			const FHitResult& H = Hits[i];
			if (!H.bBlockingHit) continue;

			AActor* HitActor = H.GetActor();
			if (HitActor == TrackActor)
			{
				BestIdx = i;
				break;
			}

			// Component owner can differ in some setups
			if (H.Component.IsValid() && H.Component->GetOwner() == TrackActor)
			{
				BestIdx = i;
				break;
			}
		}
	}

	if (BestIdx == INDEX_NONE && bFallbackToAnyHit)
	{
		BestIdx = 0;
	}

	if (BestIdx == INDEX_NONE)
	{
		return false;
	}

	OutBest = Hits[BestIdx];
	return OutBest.bBlockingHit;
}

bool ARacingCurriculumDebugActor::TraceSurfaceAtDistance(float S, FVector& OutSurfacePos, FVector& OutSurfaceNormal) const
{
	OutSurfacePos = FVector::ZeroVector;
	OutSurfaceNormal = FVector::UpVector;

	if (!TrackSpline) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	const float Len = TrackSpline->GetSplineLength();
	if (Len <= 1.f) return false;

	const float SWrapped = WrapDistance(S, Len);

	const FVector SplinePos = TrackSpline->GetLocationAtDistanceAlongSpline(SWrapped, ESplineCoordinateSpace::World);

	FVector UpDir = FVector::UpVector;
	if (bTraceAlongSplineUp)
	{
		UpDir = TrackSpline->GetUpVectorAtDistanceAlongSpline(SWrapped, ESplineCoordinateSpace::World).GetSafeNormal();
		if (UpDir.IsNearlyZero())
		{
			UpDir = FVector::UpVector;
		}
	}

	const FVector Start = SplinePos + UpDir * (PreTraceUpOffsetCm + SurfaceTraceUpCm);
	const FVector End = SplinePos - UpDir * SurfaceTraceDownCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RC_SurfaceTrace), bSurfaceTraceComplex);
	Params.AddIgnoredActor(this);
	for (AActor* A : TraceIgnoreActors)
	{
		if (IsValid(A))
		{
			Params.AddIgnoredActor(A);
		}
	}

	TArray<FHitResult> Hits;
	const bool bAny = World->LineTraceMultiByChannel(Hits, Start, End, SurfaceTraceChannel, Params);

	if (DebugDrawTraceEveryNSamples > 0)
	{
		// Debug: we draw the ray occasionally in BuildSamples
		// (actual drawing happens there so we can mod by sample index)
		(void)bAny;
	}

	if (!bAny || Hits.Num() == 0)
	{
		return false;
	}

	// Ensure sorted by distance from Start
	Hits.Sort([&](const FHitResult& A, const FHitResult& B)
		{
			return A.Distance < B.Distance;
		});

	FHitResult Best;
	if (!ChooseBestSurfaceHit(Hits, Best))
	{
		return false;
	}

	OutSurfacePos = Best.ImpactPoint;
	OutSurfaceNormal = Best.ImpactNormal.GetSafeNormal();
	return true;
}

float ARacingCurriculumDebugActor::ComputeCurvatureInvCm(float S, float WindowCm, float LengthCm, FVector& InOutForward) const
{
	InOutForward = FVector::ForwardVector;

	if (!TrackSpline || LengthCm <= 1.f) return 0.f;

	const float W = FMath::Max(10.f, WindowCm);

	const float Sc = WrapDistance(S, LengthCm);
	const float S0 = WrapDistance(S - W, LengthCm);
	const float S1 = WrapDistance(S + W, LengthCm);

	FVector F0 = TrackSpline->GetDirectionAtDistanceAlongSpline(S0, ESplineCoordinateSpace::World);
	FVector F1 = TrackSpline->GetDirectionAtDistanceAlongSpline(S1, ESplineCoordinateSpace::World);
	FVector Fc = TrackSpline->GetDirectionAtDistanceAlongSpline(Sc, ESplineCoordinateSpace::World);

	if (!F0.Normalize()) F0 = FVector::ForwardVector;
	if (!F1.Normalize()) F1 = FVector::ForwardVector;
	if (!Fc.Normalize()) Fc = FVector::ForwardVector;

	InOutForward = Fc;

	const float AngleRad = SafeAcos(FVector::DotProduct(F0, F1));
	const float Ds = FMath::Max(1.f, 2.f * W); // cm
	return AngleRad / Ds; // 1/cm
}

int32 ARacingCurriculumDebugActor::BuildTagMask(float S, float LengthCm, FVector& InOutForward, float& OutCurvNormAbs, float& OutSlopeZ) const
{
	OutCurvNormAbs = 0.f;
	OutSlopeZ = 0.f;

	if (!TrackSpline || LengthCm <= 1.f)
	{
		return 0;
	}

	const float CurvInv = ComputeCurvatureInvCm(S, CurvatureWindowCm, LengthCm, InOutForward);
	const float CurvNorm = (CurvatureBadInvCm > 0.f) ? FMath::Clamp(CurvInv / CurvatureBadInvCm, 0.f, 1.f) : 0.f;

	OutCurvNormAbs = CurvNorm;
	OutSlopeZ = InOutForward.Z;

	// If your ERacingCurriculumTag is a bitmask enum, set bits directly if you want.
	// Here we keep it generic: 0 means "None" and we mainly use Curv/Slope/Score for visualization.
	return 0;
}

void ARacingCurriculumDebugActor::ComputeSpeedAndSteerHints(int32 /*TagMask*/, float CurvNormAbs, float SlopeZ, float& OutSpeedNorm, float& OutMaxSteer) const
{
	const float CurvPenalty = FMath::Clamp(CurvNormAbs, 0.f, 1.f);
	const float SlopePenalty = FMath::Clamp(FMath::Abs(SlopeZ) * 2.f, 0.f, 1.f);

	OutSpeedNorm = 1.f - (0.75f * CurvPenalty) - (0.25f * SlopePenalty);
	OutSpeedNorm = FMath::Clamp(OutSpeedNorm, 0.05f, 1.f);

	OutMaxSteer = 0.20f + 0.80f * CurvPenalty;
	OutMaxSteer = FMath::Clamp(OutMaxSteer, 0.1f, 1.f);
}

float ARacingCurriculumDebugActor::ComputeSpawnScore01(
	bool bHasHit,
	const FVector& SurfaceNormal,
	float CurvatureInvCm,
	float PitchDeg
) const
{
	if (!bHasHit)
	{
		return -1.f; // <- wichtig!
	}

	const float UpDot = FVector::DotProduct(SurfaceNormal.GetSafeNormal(), FVector::UpVector);
	if (UpDot < SurfaceNormalUpMin)
	{
		return 0.05f; // schlecht, aber gültig
	}

	// ❗ SIGNIERT, nicht abs
	const float PitchNorm = (PitchBadDeg > 0.f)
		? FMath::Clamp(PitchDeg / PitchBadDeg, -1.f, 1.f)
		: 0.f;

	float PitchFactor;
	if (PitchNorm > 0.f)
	{
		// bergauf = schlecht
		PitchFactor = 1.f - PitchNorm;
	}
	else
	{
		// bergab = gut
		PitchFactor = 1.f;
	}

	const float CurvNorm = (CurvatureBadInvCm > 0.f)
		? FMath::Clamp(CurvatureInvCm / CurvatureBadInvCm, 0.f, 1.f)
		: 0.f;

	float CurvFactor = 1.f - CurvNorm;

	return FMath::Clamp(
		FMath::Pow(CurvFactor, CurvatureExponent) *
		FMath::Pow(PitchFactor, PitchExponent),
		0.05f, 1.f
	);
}

// Helper: Check if point is in any NoSpawnZone
bool ARacingCurriculumDebugActor::IsInAnyNoSpawnZone(const FVector& WorldPoint) const
{
	UWorld* World = GetWorld();
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

FColor ARacingCurriculumDebugActor::ColorForScore01(float Score01, bool bValidHit) const
{
	if (!bValidHit || Score01 < 0.f)
	{
		return FColor::Black;
	}


	if (Score01 >= ScoreGreenMin)  return FColor::Green;
	if (Score01 >= ScoreYellowMin) return FColor::Yellow;
	if (Score01 >= ScoreOrangeMin)   return FColor::Orange;
	return FColor::Red;
}

FLinearColor ARacingCurriculumDebugActor::HeatColor01(float T) const
{
	const float H = FMath::Lerp(0.66f, 0.0f, FMath::Clamp(T, 0.f, 1.f));
	return FLinearColor::MakeFromHSV8((uint8)(H * 255.f), 230, 255);
}

FLinearColor ARacingCurriculumDebugActor::ColorForTagMask(int32 TagMask) const
{
	if (TagMask == 0)
	{
		return FLinearColor(0.35f, 0.35f, 0.35f, 1.f);
	}

	FLinearColor Sum(0, 0, 0, 1);
	int32 Bits = 0;
	for (int32 b = 0; b < 32; ++b)
	{
		if ((TagMask & (1 << b)) == 0) continue;
		const float Hue = FMath::Fmod((float)b * 0.17f, 1.0f);
		Sum += FLinearColor::MakeFromHSV8((uint8)(Hue * 255.f), 220, 255);
		Bits++;
	}
	if (Bits > 0)
	{
		Sum /= (float)Bits;
		Sum.A = 1.f;
	}
	return Sum;
}

FString ARacingCurriculumDebugActor::TagMaskToString(int32 TagMask) const
{
	if (TagMask == 0) return TEXT("None");

	// Try resolve ERacingCurriculumTag names if it is a UENUM.
	if (UEnum* Enum = StaticEnum<ERacingCurriculumTag>())
	{
		FString Out;
		bool bFirst = true;

		const int32 Num = Enum->NumEnums();
		for (int32 i = 0; i < Num; ++i)
		{
			const FString Name = Enum->GetNameStringByIndex(i);
			if (Name.EndsWith(TEXT("_MAX"))) continue;

			const int64 V = Enum->GetValueByIndex(i);
			if (((int64)TagMask & V) == 0) continue;

			if (!bFirst) Out += TEXT("|");
			Out += Name;
			bFirst = false;
		}

		return Out.IsEmpty() ? FString::Printf(TEXT("0x%08X"), (uint32)TagMask) : Out;
	}

	return FString::Printf(TEXT("0x%08X"), (uint32)TagMask);
}

void ARacingCurriculumDebugActor::BuildSamples()
{
	CachedSamples.Reset();

	if (!TrackSpline || CachedSplineLengthCm <= 1.f)
	{
		return;
	}

	const float Step = FMath::Max(10.f, DrawSampleStepCm);
	const int32 N = FMath::Max(2, FMath::CeilToInt(CachedSplineLengthCm / Step));

	CachedSamples.Reserve(N + 1);

	for (int32 i = 0; i <= N; ++i)
	{
		const float S = WrapDistance(i * Step, CachedSplineLengthCm);

		FRC_DebugSample Smpl;
		Smpl.S = S;

		FVector Fwd = TrackSpline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
		if (!Fwd.Normalize()) Fwd = FVector::ForwardVector;

		float CurvNormAbs = 0.f;
		float SlopeZ = 0.f;
		const int32 TagMask = BuildTagMask(S, CachedSplineLengthCm, Fwd, CurvNormAbs, SlopeZ);

		// Surface trace
		FVector SurfacePos, SurfaceNormal;
		bool bHasSurface = false;

		if (bUseSurfaceTrace)
		{
			bHasSurface = TraceSurfaceAtDistance(S, SurfacePos, SurfaceNormal);

			// Optional debug draw
			if (DebugDrawTraceEveryNSamples > 0 && (i % DebugDrawTraceEveryNSamples) == 0)
			{
				const FVector SplinePos = TrackSpline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
				FVector UpDir = bTraceAlongSplineUp
					? TrackSpline->GetUpVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal()
					: FVector::UpVector;

				if (UpDir.IsNearlyZero()) UpDir = FVector::UpVector;

				const FVector Start = SplinePos + UpDir * (PreTraceUpOffsetCm + SurfaceTraceUpCm);
				const FVector End = SplinePos - UpDir * SurfaceTraceDownCm;

				DrawDebugLine(GetWorld(), Start, End, FColor::Blue, false, DrawIntervalSeconds * 1.2f, 0, 1.f);
				DrawDebugSphere(GetWorld(), Start, 18.f, 8, FColor::Black, false, DrawIntervalSeconds * 1.2f, 0, 1.f);

				if (bHasSurface)
				{
					DrawDebugSphere(GetWorld(), SurfacePos, 18.f, 8, FColor::Cyan, false, DrawIntervalSeconds * 1.2f, 0, 1.f);
				}
			}
		}

		// Final position to draw: surface + normal offset
		FVector DrawPos;
		FVector DrawNormal = FVector::UpVector;

		if (bHasSurface)
		{
			DrawNormal = SurfaceNormal.IsNearlyZero() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
			DrawPos = SurfacePos + DrawNormal * DrawSurfaceOffsetCm;
		}
		else
		{
			// fallback: spline position (NOT landscape!)
			FVector UpDir = bTraceAlongSplineUp
				? TrackSpline->GetUpVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal()
				: FVector::UpVector;

			if (UpDir.IsNearlyZero()) UpDir = FVector::UpVector;

			DrawNormal = UpDir;
			DrawPos = TrackSpline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World) + UpDir * DrawSurfaceOffsetCm;
		}

		// SpawnScore based on curvature + pitch + surface normal
		const float CurvInv = (CurvatureBadInvCm > 0.f) ? (CurvNormAbs * CurvatureBadInvCm) : 0.f;
		const float PitchDeg = ComputePitchDegFromForward(Fwd);
		float SpawnScore01 = ComputeSpawnScore01(bHasSurface, DrawNormal, CurvInv, PitchDeg);
		
		// Setze Score auf 0, wenn in NoSpawnZone
		if (SpawnScore01 >= 0.f && IsInAnyNoSpawnZone(DrawPos))
		{
			SpawnScore01 = 0.f; // Komplett verboten
		}

		float SpeedNorm = 0.f;
		float MaxSteer = 0.f;
		ComputeSpeedAndSteerHints(TagMask, CurvNormAbs, SlopeZ, SpeedNorm, MaxSteer);

		Smpl.P = DrawPos;
		Smpl.Forward = Fwd;
		Smpl.SurfaceNormal = DrawNormal;
		Smpl.bHasSurface = bHasSurface;

		Smpl.TagMask = TagMask;
		Smpl.CurvNormAbs = CurvNormAbs;
		Smpl.SlopeZ = SlopeZ;
		Smpl.SpeedNorm = SpeedNorm;
		Smpl.MaxSteer = MaxSteer;
		Smpl.SpawnScore01 = SpawnScore01;

		CachedSamples.Add(Smpl);
	}
}

void ARacingCurriculumDebugActor::DrawInternal()
{
	if (!TrackSpline || !bBuiltOnce || CachedSamples.Num() < 2)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Polyline
	for (int32 i = 0; i < CachedSamples.Num() - 1; ++i)
	{
		const FRC_DebugSample& A = CachedSamples[i];
		const FRC_DebugSample& B = CachedSamples[i + 1];

		FLinearColor C = FLinearColor::White;

		switch (VizMode)
		{
		case ERacingCurriculumVizMode::Tags:
			C = ColorForTagMask(A.TagMask);
			break;
		case ERacingCurriculumVizMode::Curvature:
			C = HeatColor01(FMath::Clamp(A.CurvNormAbs, 0.f, 1.f));
			break;
		case ERacingCurriculumVizMode::Slope:
			C = HeatColor01(FMath::Clamp(FMath::Abs(A.SlopeZ) * 2.5f, 0.f, 1.f));
			break;
		case ERacingCurriculumVizMode::Difficulty:
		{
			const float T = FMath::Clamp(
				0.60f * A.CurvNormAbs +
				0.20f * FMath::Clamp(FMath::Abs(A.SlopeZ) * 2.0f, 0.f, 1.f) +
				0.20f * FMath::Clamp(A.MaxSteer, 0.f, 1.f),
				0.f, 1.f);
			C = HeatColor01(T);
			break;
		}
		case ERacingCurriculumVizMode::SpawnScore:
		{
			if (!A.bHasSurface)
			{
				C = FLinearColor::Black;
			}
			else
			{
				C = HeatColor01(FMath::Max(0.05f, A.SpawnScore01));
			}
			break;
		}
		case ERacingCurriculumVizMode::SpawnHotspots:
			C = FLinearColor(0.2f, 0.2f, 0.2f, 1.f);
			break;
		}

		DrawDebugLine(World, A.P, B.P, C.ToFColor(true), false, DrawIntervalSeconds * 1.2f, 0, LineThickness);
	}

	// Forward arrows
	if (bDrawForwardArrows)
	{
		const float Every = FMath::Max(100.f, ArrowEveryCm);
		float NextS = 0.f;

		for (const FRC_DebugSample& Smp : CachedSamples)
		{
			if (Smp.S < NextS) continue;
			NextS = Smp.S + Every;

			const FVector Start = Smp.P + FVector(0, 0, 40.f);
			FVector Dir = Smp.Forward;
			Dir.Z = 0.f;
			if (!Dir.Normalize()) Dir = FVector::ForwardVector;

			DrawDebugDirectionalArrow(World, Start, Start + Dir * ArrowSize, 80.f, FColor::White, false, DrawIntervalSeconds * 1.2f, 0, LineThickness);
		}
	}

	// Labels (keep sparse!)
	if (bDrawLabels)
	{
		const int32 N = FMath::Max(1, LabelEveryNSamples);
		for (int32 i = 0; i < CachedSamples.Num(); i += N)
		{
			const FRC_DebugSample& Smp = CachedSamples[i];
			const FString Txt = FString::Printf(
				TEXT("S=%.1fm  Score=%.2f  Hit=%d\nCurv=%.2f  SlopeZ=%.2f\nSpeed=%.2f  Steer=%.2f"),
				Smp.S / 100.f,
				Smp.SpawnScore01,
				Smp.bHasSurface ? 1 : 0,
				Smp.CurvNormAbs,
				Smp.SlopeZ,
				Smp.SpeedNorm,
				Smp.MaxSteer
			);
			DrawDebugString(World, Smp.P + FVector(0, 0, LabelZOffsetCm), Txt, nullptr, FColor::White, DrawIntervalSeconds * 1.2f, false);
		}
	}

	if (bDrawLegend)
	{
		const FVector Base = GetActorLocation() + FVector(0, 0, 220.f);
		DrawDebugString(World, Base,
			TEXT("Curriculum Debug\n- Make sure DebugActor uses the SAME RoadSpline as respawn\n- Disable FallbackToAnyHit while debugging surface trace"),
			nullptr, FColor::Cyan, DrawIntervalSeconds * 1.2f, false);
	}

	if (bDrawSpawnCandidates)
	{
		DrawSpawnCandidates();
	}

	if (bDrawSpawnHotspots && (VizMode == ERacingCurriculumVizMode::SpawnHotspots || SpawnEvents.Num() > 0))
	{
		DrawSpawnHotspots();
	}
}

void ARacingCurriculumDebugActor::DrawSpawnCandidates()
{
	if (!TrackSpline || CachedSplineLengthCm <= 1.f) return;
	UWorld* World = GetWorld();
	if (!World) return;

	const float Step = FMath::Max(50.f, SpawnPointStepCm);
	const int32 N = FMath::Max(1, FMath::CeilToInt(CachedSplineLengthCm / Step));

	for (int32 i = 0; i <= N; ++i)
	{
		const float S = WrapDistance(i * Step, CachedSplineLengthCm);

		// If we already built samples densely, we could reuse nearest sample; keep it simple here:
		FVector Fwd = TrackSpline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
		if (!Fwd.Normalize()) Fwd = FVector::ForwardVector;

		float CurvNormAbs = 0.f;
		float SlopeZ = 0.f;
		BuildTagMask(S, CachedSplineLengthCm, Fwd, CurvNormAbs, SlopeZ);

		FVector SurfacePos, SurfaceNormal;
		const bool bHasSurface = bUseSurfaceTrace ? TraceSurfaceAtDistance(S, SurfacePos, SurfaceNormal) : false;

		FVector P;
		FVector Nrm = FVector::UpVector;

		if (bHasSurface)
		{
			Nrm = SurfaceNormal.GetSafeNormal();
			P = SurfacePos + Nrm * (DrawSurfaceOffsetCm + SpawnPointZOffsetCm);
		}
		else
		{
			// No fallback to landscape here; we show spline point only
			FVector UpDir = bTraceAlongSplineUp
				? TrackSpline->GetUpVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal()
				: FVector::UpVector;
			if (UpDir.IsNearlyZero()) UpDir = FVector::UpVector;

			P = TrackSpline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World) + UpDir * (DrawSurfaceOffsetCm + SpawnPointZOffsetCm);
			Nrm = UpDir;
		}

		const float CurvInv = (CurvatureBadInvCm > 0.f) ? (CurvNormAbs * CurvatureBadInvCm) : 0.f;
		const float PitchDeg = ComputePitchDegFromForward(Fwd);
		float Score01 = ComputeSpawnScore01(bHasSurface, Nrm, CurvInv, PitchDeg);
		
		// Setze Score auf 0, wenn in NoSpawnZone
		if (Score01 >= 0.f && IsInAnyNoSpawnZone(P))
		{
			Score01 = 0.f; // Komplett verboten
		}

		DrawDebugSphere(World, P, SpawnPointRadius, 12, ColorForScore01(Score01, bHasSurface), false, DrawIntervalSeconds * 1.2f, 0, LineThickness);
	}
}

void ARacingCurriculumDebugActor::RebuildHotspots()
{
	HotspotsByBin.Reset();

	if (!TrackSpline || CachedSplineLengthCm <= 1.f)
	{
		return;
	}

	const float Bin = FMath::Max(50.f, HotspotBinSizeCm);

	for (const FRC_SpawnEvent& E : SpawnEvents)
	{
		const int32 BinIdx = FMath::FloorToInt(E.S / Bin);
		FRC_SpawnHotspot& H = HotspotsByBin.FindOrAdd(BinIdx);

		H.Count++;
		const float Alpha = 1.f / (float)H.Count;
		H.AvgLocation = FMath::Lerp(H.AvgLocation, E.Location, Alpha);

		H.SCenter = (BinIdx + 0.5f) * Bin;
		H.LastReason = E.Reason;
		H.LastAgent = E.Agent;
	}
}

void ARacingCurriculumDebugActor::DrawSpawnHotspots()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (const auto& Pair : HotspotsByBin)
	{
		const FRC_SpawnHotspot& H = Pair.Value;
		if (H.Count <= 0) continue;

		const float T = FMath::Clamp((float)H.Count / 10.f, 0.f, 1.f);
		const FLinearColor C = HeatColor01(T);

		const float Radius = HotspotBaseRadius + HotspotRadiusPerCount * (float)H.Count;

		DrawDebugSphere(World, H.AvgLocation + FVector(0, 0, 60.f), Radius, 16, C.ToFColor(true), false, DrawIntervalSeconds * 1.2f, 0, LineThickness);

		const FString Txt = FString::Printf(
			TEXT("HOTSPOT x%d\nS=%.1fm\nReason=%s\nAgent=%s"),
			H.Count,
			H.SCenter / 100.f,
			*H.LastReason.ToString(),
			*GetNameSafe(H.LastAgent.Get())
		);
		DrawDebugString(World, H.AvgLocation + FVector(0, 0, Radius + 80.f), Txt, nullptr, FColor::White, DrawIntervalSeconds * 1.2f, false);
	}
}
