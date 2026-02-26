// Fill out your copyright notice in the Description page of Project Settings.

#include "GameInstance/RespawnGameInstanceSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"

#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"

#include "Interfaces/ResetInterface.h"
#include "Interfaces/EngineInterface.h"
#include "Interfaces/RoadSplineInterface.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

#include "Actors/NoSpawnZoneActor.h"

DEFINE_LOG_CATEGORY_STATIC(Log_RespawnSubsystem, Log, All);

#define RSP_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_RespawnSubsystem, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

// ------------------------------------------------------------
// Track spline resolve
// ------------------------------------------------------------
static USplineComponent* ResolveTrackSpline(UWorld* World, TWeakObjectPtr<AActor>& InOutCachedProvider)
{
	if (!World)
	{
		return nullptr;
	}

	// 1) Use cached provider if still valid
	if (InOutCachedProvider.IsValid())
	{
		AActor* Provider = InOutCachedProvider.Get();
		if (Provider && Provider->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
		{
			return IRoadSplineInterface::Execute_GetRoadSpline(Provider);
		}
	}

	// 2) Find by tag "Track"
	TArray<AActor*> Candidates;
	UGameplayStatics::GetAllActorsWithTag(World, FName("Track"), Candidates);

	for (AActor* Actor : Candidates)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		if (!Actor->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
		{
			continue;
		}

		USplineComponent* Spline = IRoadSplineInterface::Execute_GetRoadSpline(Actor);
		if (Spline)
		{
			InOutCachedProvider = Actor;
			return Spline;
		}
	}

	return nullptr;
}

static float WrapOrClampDistance(const USplineComponent* Spline, float S)
{
	if (!Spline) return 0.f;

	const float Len = Spline->GetSplineLength();
	if (Len <= 1.f) return 0.f;

	if (Spline->IsClosedLoop())
	{
		float X = FMath::Fmod(S, Len);
		if (X < 0.f) X += Len;
		return X;
	}

	return FMath::Clamp(S, 0.f, Len);
}

static float GetActorRadius2D(AActor* Actor)
{
	if (!Actor) return 0.f;

	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	return FMath::Max(Extent.X, Extent.Y); // cm
}

static float GetActorHalfHeight(AActor* Actor)
{
	if (!Actor) return 0.f;

	FVector Origin, Extent;
	Actor->GetActorBounds(true, Origin, Extent);
	return Extent.Z; // cm
}

// ------------------------------------------------------------
// Rotation building: Pitch=0, Yaw from spline tangent XY, Roll from spline roll
// ------------------------------------------------------------
static FQuat MakeRespawnRotationFromSpline(const USplineComponent* Spline, float DistanceCm)
{
	check(Spline);

	FVector Forward = Spline->GetDirectionAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World);
	Forward.Z = 0.f; // Pitch = 0
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}

	const float YawDeg = Forward.Rotation().Yaw;

	const FRotator SplineRot = Spline->GetRotationAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World);
	const float RollDeg = -SplineRot.Roll;

	const FQuat YawQuat = FQuat(FRotator(0.f, YawDeg, 0.f));
	const FQuat RollQuat = FQuat(Forward, FMath::DegreesToRadians(RollDeg));

	// yaw first, then roll around final forward axis
	return RollQuat * YawQuat;
}

// ------------------------------------------------------------
// Surface trace: start above spline and trace down to find REAL track Z (drop-safe)
// ------------------------------------------------------------
static bool TraceTrackSurfaceBelow(
	UWorld* World,
	const FVector& SplinePointWorld,
	AActor* ActorToIgnore,
	const TArray<AActor*>& ExtraIgnoredActors,
	FVector& OutImpactPoint,
	FVector& OutImpactNormal
)
{
	if (!World) return false;

	const float TraceStartAboveCm = 2000.f;
	const float TraceDownCm = 50000.f;

	const FVector Start = SplinePointWorld + FVector(0.f, 0.f, TraceStartAboveCm);
	const FVector End = SplinePointWorld - FVector(0.f, 0.f, TraceDownCm);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RespawnTrackSurfaceTrace), /*bTraceComplex*/ true);
	Params.bReturnPhysicalMaterial = false;

	if (ActorToIgnore)
	{
		Params.AddIgnoredActor(ActorToIgnore);
	}

	for (AActor* A : ExtraIgnoredActors)
	{
		if (IsValid(A))
		{
			Params.AddIgnoredActor(A);
		}
	}

	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params);
	if (!bHit)
	{
		return false;
	}

	OutImpactPoint = Hit.ImpactPoint;
	OutImpactNormal = Hit.ImpactNormal;
	return true;
}

// ============================================================================
// Subsystem lifetime
// ============================================================================
void URespawnGameInstanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (bDebug)
		RSP_LOGFMT(Display, "Initialized");
}

void URespawnGameInstanceSubsystem::Deinitialize()
{
	// Clear timers to avoid calling into a deinitialized subsystem
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		for (auto& It : RespawnTimers)
		{
			TM.ClearTimer(It.Value);
		}
	}

	RespawnTimers.Empty();
	ActorsToRespawn.Empty();
	NoSpawnZoneActors.Empty();
	CachedTrackProviderActor.Reset();

	if (bDebug)
		RSP_LOGFMT(Display, "Deinitialized");

	Super::Deinitialize();
}

// ============================================================================
// NoSpawnZone registration
// ============================================================================
void URespawnGameInstanceSubsystem::RegisterNoSpawnZone_Implementation(AActor* ZoneActor)
{
	if (!IsValid(ZoneActor))
	{
		RSP_LOGFMT(Warning, "RegisterNoSpawnZone called with invalid actor.");
		return;
	}

	const TWeakObjectPtr<AActor> ZoneWeak = ZoneActor;

	if (NoSpawnZoneActors.Contains(ZoneWeak))
	{
		RSP_LOGFMT(Verbose, "No-spawn zone already registered. Actor={Actor}", ("Actor", GetNameSafe(ZoneActor)));
		return;
	}

	NoSpawnZoneActors.Add(ZoneWeak);
	if (bDebug)
		RSP_LOGFMT(Log, "No-spawn zone registered. Actor={Actor}", ("Actor", GetNameSafe(ZoneActor)));
}

void URespawnGameInstanceSubsystem::UnRegisterNoSpawnZone_Implementation(AActor* ZoneActor)
{
	if (!IsValid(ZoneActor))
	{
		RSP_LOGFMT(Warning, "UnRegisterNoSpawnZone called with invalid actor.");
		return;
	}

	const TWeakObjectPtr<AActor> ZoneWeak = ZoneActor;

	if (!NoSpawnZoneActors.Contains(ZoneWeak))
	{
		RSP_LOGFMT(Verbose, "No-spawn zone not registered. Actor={Actor}", ("Actor", GetNameSafe(ZoneActor)));
		return;
	}

	NoSpawnZoneActors.Remove(ZoneWeak);
	if (bDebug)
		RSP_LOGFMT(Log, "No-spawn zone unregistered. Actor={Actor}", ("Actor", GetNameSafe(ZoneActor)));
}

// ============================================================================
// UI hook
// ============================================================================
void URespawnGameInstanceSubsystem::AddUISupportForRespawn()
{
	RSP_LOGFMT(Verbose, "AddUISupportForRespawn called");
}

// ============================================================================
// Scheduling
// ============================================================================
void URespawnGameInstanceSubsystem::NotifyRespawn(AActor* ActorToRespawn)
{
	if (!IsValid(ActorToRespawn))
	{
		RSP_LOGFMT(Warning, "NotifyRespawn called with invalid actor.");
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RSP_LOGFMT(Error, "NotifyRespawn: World is null. Actor={Actor}", ("Actor", GetNameSafe(ActorToRespawn)));
		return;
	}

	const TWeakObjectPtr<AActor> ActorWeak = ActorToRespawn;

	// Already scheduled?
	if (ActorsToRespawn.Contains(ActorWeak))
	{
		RSP_LOGFMT(Verbose, "Actor already scheduled for respawn. Actor={Actor}", ("Actor", GetNameSafe(ActorToRespawn)));
		return;
	}

	AddUISupportForRespawn();

	// Disable engine if supported
	if (ActorToRespawn->GetClass()->ImplementsInterface(UEngineInterface::StaticClass()))
	{
		IEngineInterface::Execute_SetEngineOn(ActorToRespawn, false);
		RSP_LOGFMT(Verbose, "Engine OFF for Actor={Actor}", ("Actor", GetNameSafe(ActorToRespawn)));
	}

	// Schedule timer (store handle per actor)
	const FVector ActorLocation = ActorToRespawn->GetActorLocation();

	FTimerHandle& Handle = RespawnTimers.FindOrAdd(ActorWeak);

	World->GetTimerManager().ClearTimer(Handle);

	World->GetTimerManager().SetTimer(
		Handle,
		FTimerDelegate::CreateUObject(this, &URespawnGameInstanceSubsystem::DoRespawn, ActorWeak, ActorLocation),
		RespawnDelaySeconds,
		false
	);

	ActorsToRespawn.Add(ActorWeak);
	
	if (bDebug)
		RSP_LOGFMT(Log, "Respawn scheduled. Actor={Actor}, Delay={Delay}s",
			("Actor", GetNameSafe(ActorToRespawn)),
			("Delay", RespawnDelaySeconds));
}

// ============================================================================
// Zone query
// ============================================================================
ANoSpawnZoneActor* URespawnGameInstanceSubsystem::FindBlockingNoSpawnZone(
	const FVector& WorldPoint,
	float ActorRadiusCm
)
{
	for (auto It = NoSpawnZoneActors.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<AActor>& Weak = *It;

		if (!Weak.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		AActor* A = Weak.Get();
		ANoSpawnZoneActor* Zone = Cast<ANoSpawnZoneActor>(A);
		if (!Zone)
		{
			continue;
		}

		const float Extra = Zone->GetSafetyExtraCm() + ActorRadiusCm;

		if (Zone->ContainsPointWithMargin(WorldPoint, Extra))
		{
			return Zone;
		}
	}

	return nullptr;
}

// ============================================================================
// Safe distance along spline
// ============================================================================
bool URespawnGameInstanceSubsystem::FindSafeDistanceOnTrackSpline(
	USplineComponent* Spline,
	float StartDistanceCm,
	AActor* Actor,
	float& OutSafeDistanceCm
)
{
	OutSafeDistanceCm = StartDistanceCm;
	if (!Spline || !Actor) return false;

	const float Len = Spline->GetSplineLength();
	if (Len <= 1.f) return false;

	const float ActorRadiusCm = GetActorRadius2D(Actor);

	float S = WrapOrClampDistance(Spline, StartDistanceCm);

	const int32 MaxIter = 32;
	for (int32 Iter = 0; Iter < MaxIter; ++Iter)
	{
		const FVector P = Spline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);

		if (ANoSpawnZoneActor* Blocking = FindBlockingNoSpawnZone(P, ActorRadiusCm))
		{
			const int32 Sign = (Blocking->GetExitMode() == ENoSpawnExitMode::Backward) ? -1 : +1;

			// Berechne Pawn-Länge für Sicherheitsabstand (doppelte Länge)
			float PawnLengthCm = 400.f; // Default-Fallback
			if (Actor)
			{
				FBoxSphereBounds Bounds = Actor->GetComponentsBoundingBox(true);
				FVector BoxExtent = Bounds.BoxExtent;
				// Verwende die längste Dimension (normalerweise X = Länge)
				PawnLengthCm = FMath::Max(BoxExtent.X * 2.f, FMath::Max(BoxExtent.Y * 2.f, BoxExtent.Z * 2.f));
			}
			
			// Mindestabstand: doppelte Pawn-Länge (damit auch das Heck außerhalb ist)
			const float MinSafeDistanceCm = PawnLengthCm * 2.f;
			
			// Verwende PushDistanceCm vom NoSpawnZoneActor oder mindestens doppelte Pawn-Länge
			const float Push = FMath::Max(Blocking->GetPushDistanceCm(), MinSafeDistanceCm) + Blocking->GetSafetyExtraCm() + ActorRadiusCm;

			S = WrapOrClampDistance(Spline, S + Sign * Push);
			continue;
		}

		OutSafeDistanceCm = S;
		return true;
	}

	return false;
}

// ============================================================================
// FindSafeTrackTransform (surface trace + pitch 0 + roll)
// ============================================================================
bool URespawnGameInstanceSubsystem::FindSafeTrackTransform(
	const FVector& QueryWorldLocation,
	float HeightOffsetCm,
	FTransform& OutTransform,
	float SearchStepCm,
	int32 MaxSearchSteps,
	float SafetyExtraCm
)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	USplineComponent* SplineComp = ResolveTrackSpline(World, CachedTrackProviderActor);
	if (!SplineComp)
	{
		return false;
	}

	const float Len = SplineComp->GetSplineLength();
	if (Len <= 1.f)
	{
		return false;
	}

	const float BaseKey = SplineComp->FindInputKeyClosestToWorldLocation(QueryWorldLocation);
	const float BaseS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(BaseKey);

	const int32 Steps = FMath::Max(0, MaxSearchSteps);
	const float Step = FMath::Max(1.f, SearchStepCm);

	for (int32 i = 0; i <= Steps; ++i)
	{
		const float D = i * Step;

		const int32 TryCount = (i == 0) ? 1 : 2;
		for (int32 t = 0; t < TryCount; ++t)
		{
			const float Sign = (t == 0) ? +1.f : -1.f;
			const float CandS = WrapOrClampDistance(SplineComp, BaseS + Sign * D);

			const FVector SplineLoc = SplineComp->GetLocationAtDistanceAlongSpline(CandS, ESplineCoordinateSpace::World);

			// Check zones (use SafetyExtra as a "radius")
			if (FindBlockingNoSpawnZone(SplineLoc, SafetyExtraCm) != nullptr)
			{
				continue;
			}

			// Surface trace (drop-safe)
			TArray<AActor*> Ignore;
			Ignore.Reserve(NoSpawnZoneActors.Num());

			for (auto It = NoSpawnZoneActors.CreateIterator(); It; ++It)
			{
				if (It->IsValid())
				{
					Ignore.Add(It->Get());
				}
			}

			FVector SurfacePoint = SplineLoc;
			FVector SurfaceNormal = FVector::UpVector;
			const bool bHasSurface = TraceTrackSurfaceBelow(World, SplineLoc, /*ActorToIgnore*/ nullptr, Ignore, SurfacePoint, SurfaceNormal);

			FVector FinalLoc = bHasSurface ? SurfacePoint : SplineLoc;
			FinalLoc.Z += FMath::Max(0.f, HeightOffsetCm);

			const FQuat Rot = MakeRespawnRotationFromSpline(SplineComp, CandS);

			OutTransform = FTransform(Rot, FinalLoc);
			return true;
		}
	}

	return false;
}
#include "Kismet/GameplayStatics.h"
#include "Interfaces/TrackDebugInterface.h"
// ============================================================================
// DoRespawn (drop-safe surface trace + safe height + pitch 0 + roll)
// ============================================================================
void URespawnGameInstanceSubsystem::DoRespawn(TWeakObjectPtr<AActor> ActorToRespawn, FVector ActorLocation)
{
	AActor* Actor = ActorToRespawn.Get();
	if (!IsValid(Actor))
	{
		RSP_LOGFMT(Warning, "DoRespawn: Actor is no longer valid.");
		ActorsToRespawn.Remove(ActorToRespawn);
		RespawnTimers.Remove(ActorToRespawn);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RSP_LOGFMT(Error, "DoRespawn: World is null. Actor={Actor}", ("Actor", GetNameSafe(Actor)));
		ActorsToRespawn.Remove(ActorToRespawn);
		RespawnTimers.Remove(ActorToRespawn);
		return;
	}
	
	if (bDebug)
		RSP_LOGFMT(Display, "DoRespawn for Actor={Actor}", ("Actor", GetNameSafe(Actor)));

	USplineComponent* SplineComp = ResolveTrackSpline(World, CachedTrackProviderActor);

	if (SplineComp)
	{
		const float Key = SplineComp->FindInputKeyClosestToWorldLocation(ActorLocation);
		const float StartS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(Key);

		float SafeS = StartS;
		bool bSafeOk = FindSafeDistanceOnTrackSpline(SplineComp, StartS, Actor, SafeS);

		// Wenn FindSafeDistanceOnTrackSpline fehlschlägt, versuche FindSafeTrackTransform als Fallback
		FTransform FinalXf;
		bool bHasTransform = false;
		
		if (bSafeOk)
		{
			// FindSafeDistanceOnTrackSpline hat einen sicheren Punkt gefunden
			const FVector SplineLoc = SplineComp->GetLocationAtDistanceAlongSpline(SafeS, ESplineCoordinateSpace::World);

			// Build ignore list (zones + the actor itself)
			TArray<AActor*> Ignore;
			Ignore.Reserve(NoSpawnZoneActors.Num() + 1);

			for (auto It = NoSpawnZoneActors.CreateIterator(); It; ++It)
			{
				if (It->IsValid())
				{
					Ignore.Add(It->Get());
				}
			}

			// Surface trace to get REAL track Z (drop-safe)
			FVector SurfacePoint = SplineLoc;
			FVector SurfaceNormal = FVector::UpVector;
			const bool bHasSurface = TraceTrackSurfaceBelow(World, SplineLoc, Actor, Ignore, SurfacePoint, SurfaceNormal);

			// Height offset: variable (property) + minimum bound safety
			const float ActorHalfHeightCm = GetActorHalfHeight(Actor);
			const float SafetyExtraCm = 25.f; // tweakable

			const float HeightOffsetCm = FMath::Max(RespawnHeightOffsetCm, ActorHalfHeightCm + SafetyExtraCm);

			FVector FinalLoc = bHasSurface ? SurfacePoint : SplineLoc;
			FinalLoc.Z += HeightOffsetCm;

			// Rotation: pitch 0, yaw along spline XY, roll from spline
			const FQuat Rot = MakeRespawnRotationFromSpline(SplineComp, SafeS);

			FinalXf = FTransform(Rot, FinalLoc);
			bHasTransform = true;
		}
		else
		{
			// FindSafeDistanceOnTrackSpline hat keinen sicheren Punkt gefunden, verwende FindSafeTrackTransform als Fallback
			const float ActorHalfHeightCm = GetActorHalfHeight(Actor);
			const float HeightOffsetCm = FMath::Max(RespawnHeightOffsetCm, ActorHalfHeightCm + 25.f);
			
			if (FindSafeTrackTransform(ActorLocation, HeightOffsetCm, FinalXf, 400.f, 24, 100.f))
			{
				bHasTransform = true;
				// Berechne SafeS für Logging
				const float SafeKey = SplineComp->FindInputKeyClosestToWorldLocation(FinalXf.GetLocation());
				SafeS = SplineComp->GetDistanceAlongSplineAtSplineInputKey(SafeKey);
				bSafeOk = true; // Markiere als sicher, da FindSafeTrackTransform einen sicheren Punkt gefunden hat
				
				RSP_LOGFMT(Log, "DoRespawn: FindSafeDistanceOnTrackSpline failed, using FindSafeTrackTransform fallback for Actor={Actor}",
					("Actor", GetNameSafe(Actor)));
			}
		}

		if (bHasTransform)
		{
			Actor->SetActorTransform(FinalXf, false, nullptr, ETeleportType::TeleportPhysics);

#if !(UE_BUILD_SHIPPING)
			//DrawDebugSphere(World, FinalXf.GetLocation(), 180.f, 16, bSafeOk ? FColor::Yellow : FColor::Orange, false, 12.f, 0, 5.f);
			//DrawDebugString(World, FinalXf.GetLocation() + FVector(0, 0, 220.f),
			//	bSafeOk ? TEXT("SUBSYSTEM RESPAWN (SAFE)") : TEXT("SUBSYSTEM RESPAWN (FALLBACK)"),
			//	nullptr, FColor::White, 12.f, false);
#endif
			TArray<AActor*> DebugReport;
			UGameplayStatics::GetAllActorsWithInterface(World, UTrackDebugInterface::StaticClass(), DebugReport);

			if(DebugReport.Num()>0)
			{
				for(AActor* DebugActor : DebugReport)
				{
					ITrackDebugInterface::Execute_ReportAgentSpawn(DebugActor, Actor, FinalXf, FName("RespawnSubsystem"), /*Score*/ -1.f);
				}
			}
			if(bDebug)
				RSP_LOGFMT(Log,
					"Actor moved to track. Actor={Actor}, S={S}, Safe={Safe}",
					("Actor", GetNameSafe(Actor)),
					("S", SafeS),
					("Safe", bSafeOk ? 1 : 0));
		}
		else
		{
			RSP_LOGFMT(Warning, "DoRespawn: Could not find safe spawn point for Actor={Actor}", ("Actor", GetNameSafe(Actor)));
		}
	}
	else
	{
		RSP_LOGFMT(Warning, "No valid track spline found (tag 'Track' + RoadSplineInterface). Actor={Actor}",
			("Actor", GetNameSafe(Actor)));
	}

	// Reset if supported
	if (Actor->GetClass()->ImplementsInterface(UResetInterface::StaticClass()))
	{
		IResetInterface::Execute_Reset(Actor);
		RSP_LOGFMT(Verbose, "Reset executed. Actor={Actor}", ("Actor", GetNameSafe(Actor)));
	}

	// Re-enable engine if supported
	if (Actor->GetClass()->ImplementsInterface(UEngineInterface::StaticClass()))
	{
		IEngineInterface::Execute_SetEngineOn(Actor, true);
		RSP_LOGFMT(Verbose, "Engine ON for Actor={Actor}", ("Actor", GetNameSafe(Actor)));
	}

	// Cleanup scheduling state
	ActorsToRespawn.Remove(ActorToRespawn);
	RespawnTimers.Remove(ActorToRespawn);
}
