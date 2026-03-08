#include "Components/RacingAgentComponent.h"
#include "CarAIRuntimeLogging.h"
#include "NN/SimpleNeuralNetwork.h"
#include "NN/NEATPolicyBackend.h"
#include "Subsystems/RacingAgentRegistrySubsystem.h"

#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Interfaces/RoadSplineInterface.h"
#include "Interfaces/CarInterface.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"

// ============================================================================
// Lifecycle
// ============================================================================

URacingAgentComponent::URacingAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.TickInterval = 0.f; // Run every frame when active
	// Not auto-activated: manager (or Blueprint) must call Initialize() to activate and start stepping.
	bAutoActivate = false;
}

void URacingAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Explicit idle state on attach: no genome, no authorization, no stepping.
	RuntimeState = EAgentRuntimeState::Idle;

	if (SpawnRandomSeed != 0)
	{
		SpawnRng.Initialize(SpawnRandomSeed);
	}
	else
	{
		SpawnRng.Initialize(FMath::Rand());
	}

	// Initialize adaptive ray states
	RayState_Forward.AdaptationRate = RayAdaptationRate;
	RayState_Forward.TargetDistNorm = RayTargetDistNorm;

	RayState_Left.AdaptationRate = RayAdaptationRate;
	RayState_Left.TargetDistNorm = RayTargetDistNorm;

	RayState_Right.AdaptationRate = RayAdaptationRate;
	RayState_Right.TargetDistNorm = RayTargetDistNorm;

	RayState_Left45.AdaptationRate = RayAdaptationRate;
	RayState_Left45.TargetDistNorm = RayTargetDistNorm;

	RayState_Right45.AdaptationRate = RayAdaptationRate;
	RayState_Right45.TargetDistNorm = RayTargetDistNorm;

	// Self-register with runtime registry so the editor can discover agents without world-scanning.
	if (UWorld* World = GetWorld())
	{
		if (URacingAgentRegistrySubsystem* Registry = World->GetSubsystem<URacingAgentRegistrySubsystem>())
		{
			Registry->RegisterAgent(this);
		}
		else
		{
			UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] No RacingAgentRegistrySubsystem in world; skipping self-registration."), *GetAgentLogId());
		}
	}

	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Component attached (BeginPlay). RuntimeState=Idle. Awaiting registration and genome assignment."), *GetAgentLogId());
}

void URacingAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URacingAgentRegistrySubsystem* Registry = World->GetSubsystem<URacingAgentRegistrySubsystem>())
		{
			Registry->UnregisterAgent(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void URacingAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Hard authorization gate: stepping requires EvaluationAuthorized or Evaluating state.
	// Agents in Idle or Registered state must not step — regardless of activation or genome state.
	if (RuntimeState != EAgentRuntimeState::EvaluationAuthorized && RuntimeState != EAgentRuntimeState::Evaluating)
	{
		if (!bHasLoggedAuthSkipThisEpisode)
		{
			bHasLoggedAuthSkipThisEpisode = true;
			UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Step skipped: evaluation not authorized (RuntimeState=%d, GenomeID=%d). Awaiting GrantEvaluationAuthorization()."),
				*GetAgentLogId(), (int32)RuntimeState, GenomeID);
		}
		return;
	}

	// Within authorized/evaluating state: still guard against NEAT mode without a backend.
	// This should not normally occur (manager assigns backend before authorizing), but log clearly if it does.
	if (GenomeID >= 0 && !HasNEATPolicyBackend())
	{
		if (!bHasLoggedNEATNoBackendThisEpisode)
		{
			bHasLoggedNEATNoBackendThisEpisode = true;
			UE_LOG(LogCarAIAgent, Error, TEXT("[%s] NEAT mode (GenomeID=%d) authorized but no policy backend found; skipping step. This indicates a manager assignment bug."),
				*GetAgentLogId(), GenomeID);
		}
		return;
	}

	// Single runtime stepping path: when active and episode not done, run one policy step each tick.
	const bool bShouldStep = IsActive() && !bEpisodeDone && DeltaTime > 0.f;
	if (bShouldStep)
	{
		StepOnce(DeltaTime);
	}
}

void URacingAgentComponent::Initialize()
{
	// Guard: Initialize() must only be called after evaluation authorization is granted.
	if (RuntimeState != EAgentRuntimeState::EvaluationAuthorized)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] Initialize() called but RuntimeState is not EvaluationAuthorized (state=%d, GenomeID=%d). Call GrantEvaluationAuthorization() first."),
			*GetAgentLogId(), (int32)RuntimeState, GenomeID);
		return;
	}

	FString SetupFailure;
	if (!ValidateCarSetupForEvaluation(&SetupFailure))
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] Initialize() aborted: car setup invalid. %s Fix owner/car (CarInterface or attach component to vehicle) then call Initialize() again."),
			*GetAgentLogId(), *SetupFailure);
		return;
	}

	RuntimeState = EAgentRuntimeState::Evaluating;
	Activate();
	SetComponentTickEnabled(true);
	ResetEpisode();

	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Agent initialized and evaluating. RuntimeState=Evaluating GenomeID=%d. CarInterface=%d. StepOnce will run each tick until episode ends."),
		*GetAgentLogId(), GenomeID, bCarResolvedViaInterface ? 1 : 0);
}

void URacingAgentComponent::MarkRegistered()
{
	if (RuntimeState != EAgentRuntimeState::Idle)
	{
		UE_LOG(LogCarAIAgent, Warning, TEXT("[%s] MarkRegistered() called but RuntimeState is not Idle (state=%d). Ignoring."),
			*GetAgentLogId(), (int32)RuntimeState);
		return;
	}
	RuntimeState = EAgentRuntimeState::Registered;
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Agent registered. RuntimeState=Registered GenomeID=%d. Awaiting genome assignment and evaluation authorization."),
		*GetAgentLogId(), GenomeID);
}

void URacingAgentComponent::GrantEvaluationAuthorization()
{
	if (RuntimeState != EAgentRuntimeState::Registered)
	{
		UE_LOG(LogCarAIAgent, Warning, TEXT("[%s] GrantEvaluationAuthorization() called from unexpected state (state=%d, GenomeID=%d). Expected Registered."),
			*GetAgentLogId(), (int32)RuntimeState, GenomeID);
		return;
	}
	if (GenomeID < 0)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] GrantEvaluationAuthorization() refused: GenomeID=%d is invalid (must be >= 0). Assign a genome first."),
			*GetAgentLogId(), GenomeID);
		return;
	}
	if (!HasNEATPolicyBackend())
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] GrantEvaluationAuthorization() refused: no valid NEAT policy backend (GenomeID=%d). Assign a backend first."),
			*GetAgentLogId(), GenomeID);
		return;
	}
	bHasLoggedAuthSkipThisEpisode = false;
	RuntimeState = EAgentRuntimeState::EvaluationAuthorized;
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Evaluation authorization GRANTED. RuntimeState=EvaluationAuthorized GenomeID=%d. Ready to Initialize() and step."),
		*GetAgentLogId(), GenomeID);
}

void URacingAgentComponent::RevokeEvaluationAuthorization()
{
	if (RuntimeState == EAgentRuntimeState::Idle || RuntimeState == EAgentRuntimeState::Registered)
	{
		return;
	}
	const EAgentRuntimeState PrevState = RuntimeState;
	RuntimeState = EAgentRuntimeState::Registered;
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Evaluation authorization REVOKED (was state=%d). RuntimeState=Registered GenomeID=%d."),
		*GetAgentLogId(), (int32)PrevState, GenomeID);
}

void URacingAgentComponent::ResetToIdle()
{
	RuntimeState = EAgentRuntimeState::Idle;
	GenomeID = -1;
	Generation = 0;
	SetPolicyBackend(nullptr);
	bEpisodeDone = false;
	bHasLoggedAuthSkipThisEpisode = false;
	bHasLoggedNEATNoBackendThisEpisode = false;
	Deactivate();
	SetComponentTickEnabled(false);
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Agent reset to Idle (ResetToIdle). GenomeID cleared. Stepping disabled."), *GetAgentLogId());
}

// ============================================================================
// Episode Management
// ============================================================================

void URacingAgentComponent::ResetEpisode()
{
	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] No vehicle actor!"), *GetAgentLogId());
		return;
	}

	FVector SpawnLoc;
	FRotator SpawnRot;
	bool bSpawnFromTrack = GetSpawnTransformFromTrack(SpawnLoc, SpawnRot);

	if (bSpawnFromTrack)
	{
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset spawn source: track spline (tangent-aligned). DistanceAlongTrack=%.0f cm Location=%s Rotation=%s"),
			*GetAgentLogId(), SpawnDistanceAlongTrackCm, *SpawnLoc.ToString(), *SpawnRot.ToString());
	}
	else
	{
		APlayerStart* PlayerStart = FindPlayerStart();
		if (!PlayerStart)
		{
			UE_LOG(LogCarAIAgent, Error, TEXT("[%s] No Player Start and no track spline; cannot resolve spawn. Tag a Track (IRoadSplineInterface) or place a PlayerStart."), *GetAgentLogId());
			return;
		}
		SpawnLoc = PlayerStart->GetActorLocation();
		SpawnRot = PlayerStart->GetActorRotation();
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset spawn source: PlayerStart fallback (no track spline). Location=%s Rotation=%s"),
			*GetAgentLogId(), *SpawnLoc.ToString(), *SpawnRot.ToString());
	}

	float LateralOffset = 0.f;
	if (SpawnLateralOffsetMaxCm > 0.f)
	{
		LateralOffset = SpawnRng.FRandRange(-SpawnLateralOffsetMaxCm, SpawnLateralOffsetMaxCm);
		FVector RightVec = SpawnRot.RotateVector(FVector::RightVector);
		SpawnLoc += RightVec * LateralOffset;
	}
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset: lateral_offset_cm=%.0f SpawnLoc_after_offset=%s"),
		*GetAgentLogId(), LateralOffset, *SpawnLoc.ToString());

	const float HeightOffsetCm = bEnableForcedForwardDiagnostic ? DiagnosticSpawnHeightOffsetCm : SpawnHeightOffsetCm;
	SpawnLoc.Z += HeightOffsetCm;
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset: height_offset_cm=%.0f SpawnLoc_final=%s"),
		*GetAgentLogId(), HeightOffsetCm, *SpawnLoc.ToString());
	if (bEnableForcedForwardDiagnostic && HeightOffsetCm != SpawnHeightOffsetCm)
	{
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset: diagnostic spawn height override %.0f cm (normal=%.0f)."), *GetAgentLogId(), HeightOffsetCm, SpawnHeightOffsetCm);
	}

	// Physics-safe teleport: required for fully simulated skeletal mesh (avoids "Attempting to move a fully simulated skeletal mesh ... Please use the Teleport flag").
	const bool bTeleportSuccess = Vehicle->SetActorLocationAndRotation(SpawnLoc, SpawnRot, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset teleport: location=%s rotation=%s TeleportPhysics=%s"),
		*GetAgentLogId(), *SpawnLoc.ToString(), *SpawnRot.ToString(), bTeleportSuccess ? TEXT("ok") : TEXT("failed"));

	if (UPrimitiveComponent* RootComp = GetVehicleRootComponent())
	{
		RootComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Reset: linear and angular velocities cleared on root primitive."), *GetAgentLogId());
	}

	// Zero control inputs so the car starts in a clean state (no residual steer/throttle/brake).
	ApplyAction(FVehicleAction{ 0.f, 0.f, 0.f });
	UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Reset: control inputs (steer/throttle/brake) cleared."), *GetAgentLogId());

	EpisodeGraceTimeRemaining = RewardCfg.GraceSecondsAfterReset;
	bHasLoggedGraceProtectionThisEpisode = false;
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Reset: grace window started duration=%.2fs."), *GetAgentLogId(), RewardCfg.GraceSecondsAfterReset);

	if (bEnableForcedForwardDiagnostic)
	{
		UE_LOG(LogCarAIAgent, Warning, TEXT("[%s] FORCED-FORWARD DIAGNOSTIC ENABLED: first %.2fs use Steer=0 Throttle=%.2f Brake=0. Offtrack suppressed until progress>=%.0fcm and grounded 30 frames."), *GetAgentLogId(), ForcedForwardDiagnosticDuration, ForcedForwardDiagnosticThrottle, DiagnosticOfftrackSuppressProgressCm);
	}

	ResetEpisodeAccumulators();
	ResetAdaptiveRays();

	UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Episode reset. RuntimeState=%d GenomeID=%d. Stepping begins this tick."),
		*GetAgentLogId(), (int32)RuntimeState, GenomeID);

	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Config dump: Grace=%.1fs StuckSpeedNorm=%.3f StuckSec=%.1f AirborneSec=%.1f GapTerminal=%.3f CollisionTerminal=%.3f SpawnLateralMax=%.0fcm SpawnHeight=%.0fcm SpawnDistAlongTrack=%.0fcm ForcedForward=%s ForcedForwardDur=%.1fs DiagSuppressCm=%.0f MaxSteps=%d"),
		*GetAgentLogId(),
		RewardCfg.GraceSecondsAfterReset,
		RewardCfg.StuckSpeedNorm,
		RewardCfg.StuckTimeSeconds,
		RewardCfg.AirborneMaxSeconds,
		RewardCfg.GapTerminalThreshold,
		RewardCfg.CollisionTerminalThreshold,
		SpawnLateralOffsetMaxCm,
		SpawnHeightOffsetCm,
		SpawnDistanceAlongTrackCm,
		bEnableForcedForwardDiagnostic ? TEXT("ON") : TEXT("OFF"),
		ForcedForwardDiagnosticDuration,
		DiagnosticOfftrackSuppressProgressCm,
		RewardCfg.MaxEpisodeSteps);
}

void URacingAgentComponent::ResetEpisodeAccumulators()
{
	bEpisodeDone = false;
	EpisodeStepCount = 0;
	EpisodeTimeAccum = 0.f;
	AirborneTimeAccum = 0.f;
	StuckTimeAccum = 0.f;
	bHasLoggedPolicyMissingThisEpisode = false;
	bHasLoggedNEATSchemaThisEpisode = false;
	bHasLoggedNEATNoBackendThisEpisode = false;
	bHasLoggedAuthSkipThisEpisode = false;
	bHasLoggedGraceProtectionThisEpisode = false;
	bHasLoggedDiagnosticOfftrackSuppressThisEpisode = false;
	DiagnosticForwardSpeedSum = 0.f;
	DiagnosticForcedStepCount = 0;
	bDiagnosticSummaryLoggedThisEpisode = false;
	GroundedFrameCount = 0;

	AActor* Vehicle = GetVehicleActor();
	EpisodeStartLocation = Vehicle ? Vehicle->GetActorLocation() : FVector::ZeroVector;

	AccumulatedProgressCm = 0.f;
	bHasLastProgress = false;
	LastLocation = EpisodeStartLocation;
	EnsureTrackSpline();
	if (CachedTrackSpline.IsValid())
	{
		const float Key = CachedTrackSpline->FindInputKeyClosestToWorldLocation(EpisodeStartLocation);
		LastProgressCm = CachedTrackSpline->GetDistanceAlongSplineAtSplineInputKey(Key);
		bHasLastProgress = true;
	}

	EpisodeStats = FEpisodeStats();
	EpisodeStats.StartTime = FDateTime::Now();
	EpisodeStats.AgentInstanceID = (int32)GetUniqueID();
	EpisodeStats.DistanceTraveledCm = 0.f;
	EpisodeStats.MaxSpeed = 0.f;
	EpisodeStats.AvgSpeed = 0.f;

	LastAction = FVehicleAction();

	SmoothedGravityLocal = FVector(0, 0, -1);
}

void URacingAgentComponent::ResetAdaptiveRays()
{
	RayState_Forward.Reset();
	RayState_Left.Reset();
	RayState_Right.Reset();
	RayState_Left45.Reset();
	RayState_Right45.Reset();
}

bool URacingAgentComponent::GetSpawnTransformFromTrack(FVector& OutLoc, FRotator& OutRot)
{
	EnsureTrackSpline();
	USplineComponent* Spline = CachedTrackSpline.Get();
	if (!Spline || Spline->GetSplineLength() < 1.f)
	{
		return false;
	}
	const float DistanceCm = FMath::Clamp(SpawnDistanceAlongTrackCm, 0.f, Spline->GetSplineLength());
	const float Distance = DistanceCm;
	OutLoc = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	FVector Tangent = Spline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector(1.f, 0.f, 0.f);
	}
	Tangent.Normalize();
	OutRot = Tangent.Rotation();
	return true;
}

void URacingAgentComponent::EnsureTrackSpline()
{
	if (CachedTrackSpline.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	TArray<AActor*> Tagged;
	UGameplayStatics::GetAllActorsWithTag(World, FName("Track"), Tagged);
	for (AActor* Actor : Tagged)
	{
		if (Actor && Actor->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
		{
			USplineComponent* Spline = IRoadSplineInterface::Execute_GetRoadSpline(Actor);
			if (Spline)
			{
				CachedTrackSpline = Spline;
			if (bEnableLogging)
			{
				UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Track spline resolved for progress (length %.1fm)"), *GetAgentLogId(), Spline->GetSplineLength() / 100.f);
			}
				return;
			}
		}
	}
}

void URacingAgentComponent::LogOffTrackDiagnostics(float RayGroundDistNorm)
{
	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle || !GetWorld())
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] OFFTRACK DIAG: no vehicle or world."), *GetAgentLogId());
		return;
	}

	const FVector ActorLoc = Vehicle->GetActorLocation();
	FVector GroundStart = ActorLoc;
	FVector GroundEnd = GroundStart - FVector(0, 0, GroundRayMaxDistanceCm);
	FHitResult GroundHit;
	const bool bGroundRayHit = GetWorld()->LineTraceSingleByChannel(
		GroundHit, GroundStart, GroundEnd, ECC_Visibility,
		BuildTraceIgnoreParams(TEXT("OfftrackDiag"))
	);
	const float GroundDistCm = bGroundRayHit ? (GroundHit.ImpactPoint - GroundStart).Size() : GroundRayMaxDistanceCm;

	float NearestSplineDistCm = -1.f;
	float LateralOffsetCm = -1.f;
	FVector NearestTrackPoint = FVector::ZeroVector;
	bool bHasSpline = false;
	EnsureTrackSpline();
	if (USplineComponent* Spline = CachedTrackSpline.Get())
	{
		bHasSpline = true;
		const float Key = Spline->FindInputKeyClosestToWorldLocation(ActorLoc);
		const float DistAlong = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
		NearestSplineDistCm = DistAlong;
		NearestTrackPoint = Spline->GetLocationAtDistanceAlongSpline(NearestSplineDistCm, ESplineCoordinateSpace::World);
		LateralOffsetCm = FVector::Dist(ActorLoc, NearestTrackPoint);
	}

	int32 WheelContactCount = 0;
	const UChaosWheeledVehicleMovementComponent* MovComp = Vehicle->GetClass()->ImplementsInterface(UCarInterface::StaticClass())
		? ICarInterface::Execute_GetCarChaosVehicleMovement(Vehicle)
		: Vehicle->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	if (MovComp)
	{
		const int32 NumWheels = MovComp->GetNumWheels();
		for (int32 i = 0; i < NumWheels; ++i)
		{
			const auto WheelState = MovComp->GetWheelState(i);
			if (WheelState.bInContact)
			{
				WheelContactCount++;
			}
		}
	}

	const bool bTrigger = RayGroundDistNorm < RewardCfg.GapTerminalThreshold;
	UE_LOG(LogCarAIAgent, Error, TEXT("[%s] OFFTRACK TERMINATION DIAG | RayGroundDist=%.3f (1=grounded 0=danger threshold=%.3f) trigger=%s | ground_ray_hit=%s ground_dist_cm=%.0f | spline=%s nearest_dist_cm=%.0f lateral_cm=%.0f | actor_loc=%s wheels_grounded=%d/%d"),
		*GetAgentLogId(), RayGroundDistNorm, RewardCfg.GapTerminalThreshold, bTrigger ? TEXT("yes") : TEXT("no"),
		bGroundRayHit ? TEXT("yes") : TEXT("no"), GroundDistCm,
		bHasSpline ? TEXT("yes") : TEXT("no"), NearestSplineDistCm, LateralOffsetCm,
		*ActorLoc.ToString(), WheelContactCount, MovComp ? MovComp->GetNumWheels() : 0);
}

float URacingAgentComponent::WrapProgressDelta(float Delta, float SplineLen)
{
	if (SplineLen <= 0.f)
	{
		return Delta;
	}
	const float Half = 0.5f * SplineLen;
	if (Delta > Half)
	{
		Delta -= SplineLen;
	}
	else if (Delta < -Half)
	{
		Delta += SplineLen;
	}
	return Delta;
}

float URacingAgentComponent::GetProgressDeltaCmAndUpdate(const FVector& CurrentLoc)
{
	EnsureTrackSpline();
	USplineComponent* Spline = CachedTrackSpline.Get();
	if (Spline && Spline->GetSplineLength() > 1.f)
	{
		const float Key = Spline->FindInputKeyClosestToWorldLocation(CurrentLoc);
		const float CurrentS = Spline->GetDistanceAlongSplineAtSplineInputKey(Key);
		const float Len = Spline->GetSplineLength();
		float Delta = 0.f;
		if (bHasLastProgress)
		{
			Delta = WrapProgressDelta(CurrentS - LastProgressCm, Len);
		}
		LastProgressCm = CurrentS;
		bHasLastProgress = true;
		return Delta;
	}
	// Fallback: path length (actual distance moved)
	const float PathDelta = bHasLastProgress ? (CurrentLoc - LastLocation).Size() : 0.f;
	LastLocation = CurrentLoc;
	bHasLastProgress = true;
	return PathDelta;
}

// ============================================================================
// Training Step
// ============================================================================

void URacingAgentComponent::StepOnce(float DeltaTime)
{
	if (bEpisodeDone)
	{
		return;
	}

	FString FailureReason;
	if (!ValidateCarSetupForEvaluation(&FailureReason))
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] StepOnce: car setup invalid; skipping step. %s"), *GetAgentLogId(), *FailureReason);
		return;
	}

	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] StepOnce: no resolved car actor; skipping step"), *GetAgentLogId());
		return;
	}

	// First step this episode: log resolution and that stepping has started
	if (EpisodeStepCount == 0)
	{
		AActor* Owner = GetOwner();
		const FString OwnerClass = Owner ? Owner->GetClass()->GetName() : TEXT("null");
		UPrimitiveComponent* RootPrim = GetResolvedCarRootComponent();
		const UChaosWheeledVehicleMovementComponent* MovComp = nullptr;
		if (AActor* Car = GetResolvedCarActor())
		{
			MovComp = Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass())
				? ICarInterface::Execute_GetCarChaosVehicleMovement(Car)
				: Car->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
		}
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] First step this episode. owner=%s CarInterface_path=%s resolved_car=%s root_primitive=%s movement=%s RuntimeState=%d GenomeID=%d Generation=%d"),
			*GetAgentLogId(), *OwnerClass, bCarResolvedViaInterface ? TEXT("yes") : TEXT("no"),
			Vehicle ? *Vehicle->GetClass()->GetName() : TEXT("null"), RootPrim ? *RootPrim->GetClass()->GetName() : TEXT("null"), MovComp ? TEXT("present") : TEXT("missing"),
			(int32)RuntimeState, GenomeID, Generation);
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Terminal check config: MaxSteps=%d StuckSpeedNorm=%.3f StuckSec=%.1f AirborneSec=%.1f Grace=%.1f ForcedForward=%s ForcedForwardDur=%.1f"),
			*GetAgentLogId(), RewardCfg.MaxEpisodeSteps, RewardCfg.StuckSpeedNorm, RewardCfg.StuckTimeSeconds,
			RewardCfg.AirborneMaxSeconds, RewardCfg.GraceSecondsAfterReset,
			bEnableForcedForwardDiagnostic ? TEXT("on") : TEXT("off"), ForcedForwardDiagnosticDuration);
	}
	else if (bEnableLogging && (EpisodeStepCount % 100 == 0))
	{
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Step %d (GenomeID=%d)"), *GetAgentLogId(), EpisodeStepCount, GenomeID);
	}

	// 1. Build Observation
	FRacingObservation Obs = BuildObservation();
	LastObservation = Obs;

	if (Obs.Vector.Num() == 0)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] StepOnce: skipping step — observation vector is empty (vehicle-resolution prerequisite failed). Do not proceed into policy evaluation."), *GetAgentLogId());
		return;
	}

	// 2. Progress along track (spline) or path length (fallback)
	FVector CurrentLoc = Vehicle->GetActorLocation();
	ThisStepProgressDeltaCm = GetProgressDeltaCmAndUpdate(CurrentLoc);
	AccumulatedProgressCm += ThisStepProgressDeltaCm;

	// 3. Compute Reward (uses ThisStepProgressDeltaCm for distance)
	FRewardBreakdown Reward = ComputeReward(Obs, DeltaTime);

	// 4. Get Action from policy. NEAT (GenomeID >= 0): only PolicyBackend; no fallback to avoid masking broken integration.
	FVehicleAction Action;
	TArray<float> PolicyOutput;
	bool bGotOutput = false;

	const int32 ExpectedInputs = (PolicyBackend && PolicyBackend->IsValid()) ? PolicyBackend->GetExpectedInputCount() : -1;
	if (ExpectedInputs >= 0 && Obs.Vector.Num() != ExpectedInputs)
	{
		if (!bHasLoggedPolicyMissingThisEpisode)
		{
			bHasLoggedPolicyMissingThisEpisode = true;
			UE_LOG(LogCarAIAgent, Error, TEXT("[%s] Observation size mismatch: vector has %d elements, policy expects %d. NEAT path requires exactly %d inputs (no LIDAR). GenomeID=%d"),
				*GetAgentLogId(), Obs.Vector.Num(), ExpectedInputs, FRacingObservation::NEAT_OBSERVATION_SIZE, GenomeID);
		}
	}
	else if (PolicyBackend && PolicyBackend->IsValid())
	{
		if (GenomeID >= 0)
		{
			if (!bHasLoggedNEATSchemaThisEpisode)
			{
				bHasLoggedNEATSchemaThisEpisode = true;
				UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] NEAT observation schema: size=%d layout=SpeedNorm,YawRateNorm,PitchRateNorm,RollRateNorm,RayForward,RayLeft,RayRight,RayLeft45,RayRight45,RayForwardUp,RayForwardDown,RayGroundDist,GravityX,GravityY,GravityZ (LIDAR not used)"),
					*GetAgentLogId(), FRacingObservation::NEAT_OBSERVATION_SIZE);
			}
			// NEAT evaluation: use only PolicyBackend; no fallback to PolicyNetwork.
			bGotOutput = PolicyBackend->Evaluate(Obs.Vector, PolicyOutput);
			if (!bGotOutput && !bHasLoggedPolicyMissingThisEpisode)
			{
				bHasLoggedPolicyMissingThisEpisode = true;
				UE_LOG(LogCarAIAgent, Error, TEXT("[%s] NEAT inference failed (e.g. observation size mismatch). GenomeID=%d Check evaluator logs."), *GetAgentLogId(), GenomeID);
			}
		}
		else
		{
			bGotOutput = PolicyBackend->Evaluate(Obs.Vector, PolicyOutput);
		}
		if (!bGotOutput && PolicyNetwork)
		{
			PolicyNetwork->ForwardPolicy(Obs.Vector, PolicyOutput);
			bGotOutput = (PolicyOutput.Num() == 3);
		}
	}
	else
	{
		// No valid PolicyBackend: non-NEAT fallback to PolicyNetwork only.
		if (PolicyNetwork)
		{
			PolicyNetwork->ForwardPolicy(Obs.Vector, PolicyOutput);
			bGotOutput = (PolicyOutput.Num() == 3);
		}
	}

	if (bGotOutput && PolicyOutput.Num() >= 3)
	{
		// Clamp at agent layer (not inside backend). Early NEAT training: throttle in [0,1] only (no reverse).
		Action.Steer = FMath::Clamp(PolicyOutput[0], -1.f, 1.f);
		const float RawThrottle = PolicyOutput[1];
		Action.Throttle = FMath::Clamp(RawThrottle, 0.f, 1.f);
		Action.Brake = FMath::Clamp(PolicyOutput[2], 0.f, 1.f);
		if (GenomeID >= 0 && RawThrottle < 0.f && bEnableLogging)
		{
			UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] NEAT early training: throttle clamped to [0,1] (raw=%.2f -> %.2f, no reverse)."), *GetAgentLogId(), RawThrottle, Action.Throttle);
		}
	}
	else
	{
		// No valid output: NEAT path uses zero action; non-NEAT may use fallback drive with explicit log.
		if (GenomeID >= 0)
		{
			Action.Steer = 0.f;
			Action.Throttle = 0.f;
			Action.Brake = 0.f;
		}
		else
		{
			if (!bHasLoggedPolicyMissingThisEpisode)
			{
				bHasLoggedPolicyMissingThisEpisode = true;
				UE_LOG(LogCarAIAgent, Warning, TEXT("[%s] No policy backend or network set. Using fallback forward drive (non-NEAT gameplay only). GenomeID=%d"), *GetAgentLogId(), GenomeID);
			}
			Action.Steer = 0.f;
			Action.Throttle = 0.5f;
			Action.Brake = 0.f;
		}
	}

	// ----- Forced-Forward Diagnostic: bypass NEAT for first N seconds to verify vehicle can drive forward -----
	const bool bInForcedForwardWindow = bEnableForcedForwardDiagnostic && (EpisodeTimeAccum < ForcedForwardDiagnosticDuration);
	float SignedForwardSpeedCmPerSec = 0.f;
	int32 CurrentGear = -1;
	AActor* CarForDiagnostic = GetVehicleActor();
	if (CarForDiagnostic)
	{
		const UChaosWheeledVehicleMovementComponent* MovComp = CarForDiagnostic->GetClass()->ImplementsInterface(UCarInterface::StaticClass())
			? ICarInterface::Execute_GetCarChaosVehicleMovement(CarForDiagnostic)
			: CarForDiagnostic->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
		if (MovComp)
		{
			SignedForwardSpeedCmPerSec = MovComp->GetForwardSpeed();
			CurrentGear = MovComp->GetCurrentGear();
		}
		else if (UPrimitiveComponent* RootComp = GetVehicleRootComponent())
		{
			FVector Vel = RootComp->GetPhysicsLinearVelocity();
			SignedForwardSpeedCmPerSec = FVector::DotProduct(Vel, CarForDiagnostic->GetActorForwardVector());
		}
	}

	if (bInForcedForwardWindow)
	{
		Action.Steer = 0.f;
		Action.Throttle = ForcedForwardDiagnosticThrottle;
		Action.Brake = 0.f;
		DiagnosticForwardSpeedSum += SignedForwardSpeedCmPerSec;
		DiagnosticForcedStepCount++;
		const bool bMovingForward = SignedForwardSpeedCmPerSec > 10.f;
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] FORCED-FORWARD DIAGNOSTIC ACTIVE | step=%d progress_cm=%.0f signed_forward_cm_s=%.1f gear=%d RayGroundDist=%.3f grounded_frames=%d applied=[%.2f,%.2f,%.2f] moving_forward=%s"),
			*GetAgentLogId(), EpisodeStepCount,
			AccumulatedProgressCm, SignedForwardSpeedCmPerSec, CurrentGear,
			LastObservation.RayGroundDist, GroundedFrameCount,
			Action.Steer, Action.Throttle, Action.Brake,
			bMovingForward ? TEXT("yes") : TEXT("no"));
	}
	else if (bEnableForcedForwardDiagnostic && bEnableLogging)
	{
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Forced-forward diagnostic inactive (normal NEAT control). applied=[%.2f,%.2f,%.2f] gear=%d signed_forward_cm_s=%.1f"),
			*GetAgentLogId(), Action.Steer, Action.Throttle, Action.Brake, CurrentGear, SignedForwardSpeedCmPerSec);
	}

	// Smoothness: penalize steer change (was incorrectly steer vs speed)
	Reward.Smoothness = FMath::Abs(Action.Steer - LastAction.Steer) * RewardCfg.W_ActionSmooth;
	Reward.Total = Reward.Distance + Reward.Survival + Reward.Speed + Reward.Smoothness + Reward.Collision + Reward.GapPenalty;
	Reward.Total = FMath::Clamp(Reward.Total, -RewardCfg.MaxAbsTerm, RewardCfg.MaxAbsTerm);

	// 5. Apply Action
	ApplyAction(Action);
	LastAction = Action;

	// 6. Update Episode Stats
	EpisodeStepCount++;
	EpisodeTimeAccum += DeltaTime;
	EpisodeStats.TotalReward += Reward.Total;
	EpisodeStats.StepCount = EpisodeStepCount;
	EpisodeStats.DurationSeconds = EpisodeTimeAccum;
	EpisodeStats.DistanceTraveledCm = AccumulatedProgressCm;

	float CurrentSpeed = Obs.SpeedNorm * SpeedNormCmPerSec;
	EpisodeStats.MaxSpeed = FMath::Max(EpisodeStats.MaxSpeed, CurrentSpeed);

	// ----- Forced-Forward Diagnostic: summary when window ends -----
	if (bEnableForcedForwardDiagnostic && !bDiagnosticSummaryLoggedThisEpisode && EpisodeTimeAccum >= ForcedForwardDiagnosticDuration)
	{
		bDiagnosticSummaryLoggedThisEpisode = true;
		const float AvgSignedSpeed = (DiagnosticForcedStepCount > 0) ? (DiagnosticForwardSpeedSum / (float)DiagnosticForcedStepCount) : 0.f;
		const bool bForwardEstablished = AvgSignedSpeed > 20.f;
		const bool bReverseOrIdle = (DiagnosticForcedStepCount > 0 && AvgSignedSpeed < 10.f) || (DiagnosticForwardSpeedSum < 0.f);
		UE_LOG(LogCarAIAgent, Warning, TEXT("[%s] FORCED-FORWARD DIAGNOSTIC WINDOW ENDED | duration=%.2fs steps_in_window=%d avg_signed_forward_cm_s=%.1f forward_motion_established=%s reverse_or_idle_occurred=%s (Interpret: if forward_motion_established=no the vehicle/transmission path is likely the issue.)"),
			*GetAgentLogId(), ForcedForwardDiagnosticDuration, DiagnosticForcedStepCount, AvgSignedSpeed,
			bForwardEstablished ? TEXT("yes") : TEXT("no"), bReverseOrIdle ? TEXT("yes") : TEXT("no"));
	}

	// 7. Check Terminal Conditions
	FString TermReason;
	if (CheckTerminalConditions(Obs, DeltaTime, TermReason) || Reward.bDone)
	{
		if (Reward.bDone)
		{
			TermReason = Reward.DoneReason;
		}

		FinalizeEpisodeStats(TermReason);
		bEpisodeDone = true;

		OnEpisodeDone.Broadcast(EpisodeStats);

		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Episode completed: %s | Fitness=%.2f Steps=%d Progress=%.1fm GenomeID=%d"),
			*GetAgentLogId(), *TermReason, EpisodeStats.NEATFitness, EpisodeStats.StepCount,
			EpisodeStats.DistanceTraveledCm / 100.f, GenomeID);
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Reward breakdown: Distance=%.3f Survival=%.3f Speed=%.3f Smoothness=%.3f Collision=%.3f Gap=%.3f Total=%.3f"),
			*GetAgentLogId(), Reward.Distance, Reward.Survival, Reward.Speed, Reward.Smoothness, Reward.Collision, Reward.GapPenalty, Reward.Total);

		return;
	}

	// 8. Decrement grace window (time-based); log when it ends
	const float GraceBefore = EpisodeGraceTimeRemaining;
	EpisodeGraceTimeRemaining = FMath::Max(0.f, EpisodeGraceTimeRemaining - DeltaTime);
	if (GraceBefore > 0.f && EpisodeGraceTimeRemaining <= 0.f)
	{
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Grace window ended (%.2fs elapsed). Collision/offtrack will now terminate episode."), *GetAgentLogId(), RewardCfg.GraceSecondsAfterReset);
	}

	// 8b. Grounded frame count for diagnostic offtrack suppress (consecutive frames with RayGroundDist >= 0.15)
	if (LastObservation.RayGroundDist >= 0.15f)
	{
		GroundedFrameCount++;
	}
	else
	{
		GroundedFrameCount = 0;
	}

	// 9. Update Adaptive Rays
	if (bEnableAdaptiveRays)
	{
		UpdateAdaptiveRayAngles();
	}

	// 10. Broadcast step completed
	OnStepCompleted.Broadcast(Obs, Reward);

	// 11. Debug HUD
	if (bDrawObservationHUD)
	{
		DrawObservationHUD();
	}

	if (bDrawRayAnglesDebug)
	{
		DrawRayAnglesDebug();
	}
}

// ============================================================================
// Observation - Adaptive Rays + IMU
// ============================================================================

FRacingObservation URacingAgentComponent::BuildObservation()
{
	FRacingObservation Obs;

	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] BuildObservation: returning empty vector because resolved car actor is null (owner=%s). Fix vehicle resolution (CarInterface on owner or possess a car pawn) before evaluation."),
			*GetAgentLogId(), GetOwner() ? *GetOwner()->GetClass()->GetName() : TEXT("null"));
		return Obs;
	}

	UPrimitiveComponent* RootComp = GetVehicleRootComponent();
	if (!RootComp)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] BuildObservation: returning empty vector because resolved car has no primitive root (car=%s). Observation requires a physics root for velocity and traces."),
			*GetAgentLogId(), *Vehicle->GetName());
		return Obs;
	}

	// ===== Vehicle State =====

	FVector Velocity = RootComp->GetPhysicsLinearVelocity();
	Obs.SpeedNorm = Velocity.Size() / SpeedNormCmPerSec;

	FVector AngVel = RootComp->GetPhysicsAngularVelocityInDegrees();
	Obs.YawRateNorm = AngVel.Z / AngVelNormDegPerSec;
	Obs.PitchRateNorm = AngVel.Y / AngVelNormDegPerSec;
	Obs.RollRateNorm = AngVel.X / AngVelNormDegPerSec;

	// ===== 5 Adaptive Horizontal Rays =====

	FVector Origin = Vehicle->GetActorLocation() + FVector(0, 0, RayHeightOffsetCm);
	FVector Forward = Vehicle->GetActorForwardVector();
	FVector Right = Vehicle->GetActorRightVector();

	// Forward (0° yaw, adaptive pitch)
	Obs.RayForward = TraceAdaptiveRay(Origin, Forward, RayState_Forward, FColor::Red);

	// Left (90° yaw, adaptive pitch)
	Obs.RayLeft = TraceAdaptiveRay(Origin, -Right, RayState_Left, FColor::Blue);

	// Right (-90° yaw, adaptive pitch)
	Obs.RayRight = TraceAdaptiveRay(Origin, Right, RayState_Right, FColor::Green);

	// Left 45° (adaptive pitch)
	FVector Left45 = (Forward - Right).GetSafeNormal();
	Obs.RayLeft45 = TraceAdaptiveRay(Origin, Left45, RayState_Left45, FColor::Cyan);

	// Right 45° (adaptive pitch)
	FVector Right45 = (Forward + Right).GetSafeNormal();
	Obs.RayRight45 = TraceAdaptiveRay(Origin, Right45, RayState_Right45, FColor::Yellow);

	// ===== 2 Fixed Vertical Rays =====

	// Forward-Up (fixed +30° pitch)
	FVector ForwardUp = FRotator(30.f, 0.f, 0.f).RotateVector(Forward);
	Obs.RayForwardUp = TraceFixedRay(Origin, ForwardUp, RayMaxDistanceCm, FColor::Purple);

	// Forward-Down (fixed -30° pitch)
	FVector ForwardDown = FRotator(-30.f, 0.f, 0.f).RotateVector(Forward);
	Obs.RayForwardDown = TraceFixedRay(Origin, ForwardDown, RayMaxDistanceCm, FColor::Orange);

	// ===== Ground Distance Ray =====

	FVector GroundStart = Vehicle->GetActorLocation();
	FVector GroundEnd = GroundStart - FVector(0, 0, GroundRayMaxDistanceCm);
	FHitResult GroundHit;
	bool bHasGround = GetWorld()->LineTraceSingleByChannel(
		GroundHit, GroundStart, GroundEnd, ECC_Visibility,
		BuildTraceIgnoreParams(TEXT("GroundCheck"))
	);

	// RayGroundDist semantics: downstream treats high = grounded/safe, low = offtrack/airborne/danger.
	// Raw ray gives: small distance = ground close (on track), large distance = ground far (airborne). Invert so 1 = safe, 0 = danger.
	if (bHasGround)
	{
		const float GroundDistCm = (GroundHit.ImpactPoint - GroundStart).Size();
		const float GroundDistNorm = FMath::Clamp(GroundDistCm / GroundRayMaxDistanceCm, 0.f, 1.f);
		Obs.RayGroundDist = 1.0f - GroundDistNorm;
		if (bEnableLogging && EpisodeStepCount >= 0 && EpisodeStepCount < 5)
		{
			UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Ground ray: hit=yes raw_cm=%.0f norm_dist=%.3f RayGroundDist=%.3f (1=grounded)"),
				*GetAgentLogId(), GroundDistCm, GroundDistNorm, Obs.RayGroundDist);
		}
	}
	else
	{
		Obs.RayGroundDist = 0.f; // No ground = danger
		if (bEnableLogging && EpisodeStepCount >= 0 && EpisodeStepCount < 5)
		{
			UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Ground ray: hit=no RayGroundDist=0 (danger)"), *GetAgentLogId());
		}
	}

	if (bDrawRayDebug)
	{
		FColor GroundColor = bHasGround ? FColor::Green : FColor::Red;
		DrawDebugLine(GetWorld(), GroundStart, bHasGround ? GroundHit.ImpactPoint : GroundEnd,
			GroundColor, false, -1.f, 0, 2.f);
	}

	// ===== IMU Sensor - Gravity Direction =====

	if (bEnableIMUSensor)
	{
		FVector GravityLocal = ComputeGravityDirection();
		Obs.GravityX = GravityLocal.X;
		Obs.GravityY = GravityLocal.Y;
		Obs.GravityZ = GravityLocal.Z;
	}
	else
	{
		Obs.GravityX = 0.f;
		Obs.GravityY = 0.f;
		Obs.GravityZ = -1.f; // Default: pointing down
	}

	// ===== Optional LIDAR Ring (not used in NEAT path; NEAT contract is fixed 15 inputs) =====
	if (bEnableLidar && !HasNEATPolicyBackend())
	{
		BuildLidarObservation(Origin, Forward, Obs.LidarRays);
	}

	// ===== Build Vector =====
	if (HasNEATPolicyBackend())
	{
		Obs.BuildVectorForNEAT();
	}
	else
	{
		Obs.BuildVector();
	}

	return Obs;
}

// ============================================================================
// Trace Ignore Params
// ============================================================================

FCollisionQueryParams URacingAgentComponent::BuildTraceIgnoreParams(const FName& TraceTag) const
{
    FCollisionQueryParams Params(TraceTag, false);

    // Always ignore own vehicle.
    if (AActor* OwnVehicle = GetVehicleActor())
    {
        Params.AddIgnoredActor(OwnVehicle);
    }

    // Ignore all other registered agent vehicles to prevent trace contamination.
    UWorld* World = GetWorld();
    if (World)
    {
        if (URacingAgentRegistrySubsystem* Registry = World->GetSubsystem<URacingAgentRegistrySubsystem>())
        {
            for (URacingAgentComponent* OtherAgent : Registry->GetRegisteredAgents())
            {
                if (OtherAgent && OtherAgent != this)
                {
                    if (AActor* OtherVehicle = OtherAgent->GetVehicleActor())
                    {
                        Params.AddIgnoredActor(OtherVehicle);
                    }
                }
            }
        }
    }

    if (bDrawRayDebug)
    {
        const int32 IgnoredCount = Params.GetIgnoredActors().Num();
        UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Trace '%s': ignoring %d actors (own + %d other agents)"),
            *GetAgentLogId(), *TraceTag.ToString(), IgnoredCount, IgnoredCount > 0 ? IgnoredCount - 1 : 0);
    }

    return Params;
}

// ============================================================================
// Adaptive Ray Tracing
// ============================================================================

float URacingAgentComponent::TraceAdaptiveRay(
	const FVector& Origin,
	const FVector& YawDirection,
	FAdaptiveRayState& RayState,
	FColor DebugColor)
{
	// Apply pitch rotation to yaw direction
	FRotator PitchRot(RayState.CurrentPitchDeg, 0.f, 0.f);
	FVector Direction = PitchRot.RotateVector(YawDirection);

	FVector Start = Origin + Direction * RayStartOffsetCm;
	FVector End = Start + Direction * RayMaxDistanceCm;

	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		RayTraceChannel,
		BuildTraceIgnoreParams(TEXT("AdaptiveRay"))
	);

	float HitDistNorm = 1.f;
	if (bHit)
	{
		float Distance = (Hit.ImpactPoint - Start).Size();
		HitDistNorm = FMath::Clamp(Distance / RayMaxDistanceCm, 0.f, 1.f);
	}

	// Update ray state for next frame
	RayState.UpdatePitchAngle(bHit, HitDistNorm);

	if (bDrawRayDebug)
	{
		FColor DrawColor = bHit ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
			DrawColor, false, -1.f, 0, RayDebugLineThickness);
	}

	return HitDistNorm;
}

float URacingAgentComponent::TraceFixedRay(
	const FVector& Origin,
	const FVector& Direction,
	float MaxDistance,
	FColor DebugColor)
{
	FVector Start = Origin + Direction * RayStartOffsetCm;
	FVector End = Start + Direction * MaxDistance;

	FHitResult Hit;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		RayTraceChannel,
		BuildTraceIgnoreParams(TEXT("FixedRay"))
	);

	if (bDrawRayDebug)
	{
		FColor DrawColor = bHit ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), Start, bHit ? Hit.ImpactPoint : End,
			DrawColor, false, -1.f, 0, RayDebugLineThickness);
	}

	if (bHit)
	{
		float Distance = (Hit.ImpactPoint - Start).Size();
		return FMath::Clamp(Distance / MaxDistance, 0.f, 1.f);
	}

	return 1.0f;
}

void URacingAgentComponent::BuildLidarObservation(const FVector& Origin, const FVector& Forward, TArray<float>& OutRays)
{
	const int32 N = FMath::Max(4, LidarNumRays);
	OutRays.SetNumUninitialized(N);

	const float StepDeg = 360.f / N;

	for (int32 i = 0; i < N; ++i)
	{
		const FQuat YawRot(FVector::UpVector, FMath::DegreesToRadians(StepDeg * i));
		const FVector Dir = YawRot.RotateVector(Forward).GetSafeNormal();
		OutRays[i] = TraceFixedRay(Origin, Dir, LidarMaxDistanceCm, FColor::White);
	}
}

void URacingAgentComponent::UpdateAdaptiveRayAngles()
{
	// Ray states are already updated in TraceAdaptiveRay()
	// This is just for any post-processing if needed

	// Optional: Synchronize adaptation rate if changed in editor
	RayState_Forward.AdaptationRate = RayAdaptationRate;
	RayState_Forward.TargetDistNorm = RayTargetDistNorm;

	RayState_Left.AdaptationRate = RayAdaptationRate;
	RayState_Left.TargetDistNorm = RayTargetDistNorm;

	RayState_Right.AdaptationRate = RayAdaptationRate;
	RayState_Right.TargetDistNorm = RayTargetDistNorm;

	RayState_Left45.AdaptationRate = RayAdaptationRate;
	RayState_Left45.TargetDistNorm = RayTargetDistNorm;

	RayState_Right45.AdaptationRate = RayAdaptationRate;
	RayState_Right45.TargetDistNorm = RayTargetDistNorm;
}

// ============================================================================
// IMU Sensor - Gravity Direction
// ============================================================================

FVector URacingAgentComponent::ComputeGravityDirection()
{
	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		return FVector(0, 0, -1);
	}

	// World gravity direction (always pointing down)
	FVector WorldGravity = FVector(0, 0, -1);

	// Transform to vehicle's local space
	FTransform VehicleTransform = Vehicle->GetActorTransform();
	FVector GravityLocal = VehicleTransform.InverseTransformVectorNoScale(WorldGravity);
	GravityLocal.Normalize();

	// Apply smoothing
	if (bSmoothGravity)
	{
		SmoothedGravityLocal = FMath::Lerp(SmoothedGravityLocal, GravityLocal, GravitySmoothingFactor);
		SmoothedGravityLocal.Normalize();
		return SmoothedGravityLocal;
	}

	return GravityLocal;
}

// ============================================================================
// Reward
// ============================================================================

FRewardBreakdown URacingAgentComponent::ComputeReward(const FRacingObservation& Obs, float DeltaTime)
{
	FRewardBreakdown R;

	// ===== Distance Reward (progress along track or path length this step) =====

	const float ProgressMetersThisStep = ThisStepProgressDeltaCm / 100.f;
	R.Distance = ProgressMetersThisStep * RewardCfg.W_Distance;

	// ===== Survival Bonus =====

	// Per-step survival bonus (DeltaTime keeps it scale-consistent regardless of episode length)
	R.Survival = DeltaTime * RewardCfg.W_Survival;

	// ===== Speed Bonus (Phase 2, after enough progress) =====

	if (AccumulatedProgressCm > RewardCfg.Phase2ActivationDistanceCm)
	{
		const float SpeedDiff = FMath::Abs(Obs.SpeedNorm - RewardCfg.SpeedTargetNorm);
		R.Speed = (1.f - SpeedDiff) * RewardCfg.W_Speed;
	}

	// ===== Smoothness (set in StepOnce from steer change: |Action.Steer - LastAction.Steer|) =====

	R.Smoothness = 0.f;

	// ===== Collision Penalty (Adaptive Rays) =====

	float MinRayDist = FMath::Min(
		FMath::Min(
			FMath::Min(Obs.RayForward, Obs.RayLeft),
			FMath::Min(Obs.RayRight, Obs.RayLeft45)
		),
		Obs.RayRight45
	);

	if (MinRayDist < RewardCfg.CollisionWarningThreshold)
	{
		R.Collision = RewardCfg.CollisionWarningPenalty;
	}

	if (MinRayDist < RewardCfg.CollisionTerminalThreshold)
	{
		if (EpisodeGraceTimeRemaining > 0.f)
		{
			if (!bHasLoggedGraceProtectionThisEpisode)
			{
				bHasLoggedGraceProtectionThisEpisode = true;
				UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Grace window: collision terminal ignored (grace_remaining=%.2fs)."), *GetAgentLogId(), EpisodeGraceTimeRemaining);
			}
			R.Collision = RewardCfg.CollisionWarningPenalty;
		}
		else
		{
			R.bDone = true;
			R.DoneReason = TEXT("Collision");
			R.Collision = RewardCfg.CollisionTerminalPenalty;
		}
	}

	// ===== Gap Penalty (Ground Ray) =====

	if (Obs.RayGroundDist < RewardCfg.GapWarningThreshold)
	{
		R.GapPenalty = RewardCfg.GapWarningPenalty;
	}

	if (Obs.RayGroundDist < RewardCfg.GapTerminalThreshold)
	{
		const bool bDiagnosticSuppress = bEnableForcedForwardDiagnostic &&
			(EpisodeTimeAccum < ForcedForwardDiagnosticDuration ||
			 AccumulatedProgressCm < DiagnosticOfftrackSuppressProgressCm ||
			 GroundedFrameCount < 30);

		if (bDiagnosticSuppress)
		{
			if (!bHasLoggedDiagnosticOfftrackSuppressThisEpisode)
			{
				bHasLoggedDiagnosticOfftrackSuppressThisEpisode = true;
				UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Diagnostic: offtrack suppressed (progress=%.0fcm grounded_frames=%d threshold=%.0fcm)."),
					*GetAgentLogId(), AccumulatedProgressCm, GroundedFrameCount, DiagnosticOfftrackSuppressProgressCm);
			}
			R.GapPenalty = RewardCfg.GapWarningPenalty;
		}
		else if (EpisodeGraceTimeRemaining > 0.f)
		{
			if (!bHasLoggedGraceProtectionThisEpisode)
			{
				bHasLoggedGraceProtectionThisEpisode = true;
				UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Grace window: offtrack terminal ignored (grace_remaining=%.2fs)."), *GetAgentLogId(), EpisodeGraceTimeRemaining);
			}
			R.GapPenalty = RewardCfg.GapWarningPenalty;
		}
		else
		{
			LogOffTrackDiagnostics(Obs.RayGroundDist);
			R.bDone = true;
			R.DoneReason = TEXT("Fell off track");
			R.GapPenalty = RewardCfg.GapTerminalPenalty;
		}
	}

	// ===== Total =====

	R.Total = R.Distance + R.Survival + R.Speed + R.Smoothness + R.Collision + R.GapPenalty;
	R.Total = FMath::Clamp(R.Total, -RewardCfg.MaxAbsTerm, RewardCfg.MaxAbsTerm);

	return R;
}

float URacingAgentComponent::GetEpisodeFitness() const
{
	return EpisodeStats.NEATFitness;
}

// ============================================================================
// Terminal Conditions
// ============================================================================

bool URacingAgentComponent::CheckTerminalConditions(const FRacingObservation& Obs, float DeltaTime, FString& OutReason)
{
	// Max steps — never suppressed.
	if (EpisodeStepCount >= RewardCfg.MaxEpisodeSteps)
	{
		OutReason = TEXT("MaxSteps");
		return true;
	}

	// Airborne too long — suppressed during grace window (spawn bounce artifact).
	if (EpisodeGraceTimeRemaining <= 0.f)
	{
		if (Obs.RayGroundDist < 0.1f)
		{
			AirborneTimeAccum += DeltaTime;
			if (AirborneTimeAccum >= RewardCfg.AirborneMaxSeconds)
			{
				OutReason = TEXT("AirborneLong");
				return true;
			}
		}
		else
		{
			AirborneTimeAccum = 0.f;
		}
	}
	else
	{
		AirborneTimeAccum = 0.f; // Reset during grace so timer does not carry over.
	}

	// Stuck — suppressed during grace window and during forced-forward diagnostic window.
	const bool bStuckSuppressed = (EpisodeGraceTimeRemaining > 0.f) ||
		(bEnableForcedForwardDiagnostic && EpisodeTimeAccum < ForcedForwardDiagnosticDuration);
	if (!bStuckSuppressed)
	{
		if (Obs.SpeedNorm < RewardCfg.StuckSpeedNorm)
		{
			StuckTimeAccum += DeltaTime;
			if (StuckTimeAccum >= RewardCfg.StuckTimeSeconds)
			{
				OutReason = TEXT("Stuck");
				return true;
			}
		}
		else
		{
			StuckTimeAccum = 0.f;
		}
	}
	else
	{
		StuckTimeAccum = 0.f; // Reset during suppressed window so it starts fresh after grace.
	}

	return false;
}

void URacingAgentComponent::FinalizeEpisodeStats(const FString& TerminationReason)
{
	EpisodeStats.EndTime = FDateTime::Now();
	EpisodeStats.TerminationReason = TerminationReason;

	if (EpisodeStats.StepCount > 0)
	{
		EpisodeStats.AvgSpeed = EpisodeStats.DistanceTraveledCm / EpisodeStats.DurationSeconds;
	}

	EpisodeStats.CalculateNEATFitness();
	const float BaseFitness = EpisodeStats.NEATFitness;

	// Hard penalty for bad terminations so collision/offtrack are not rewarded in early training
	float TerminationPenalty = 0.f;
	if (TerminationReason == TEXT("Collision"))
	{
		TerminationPenalty = RewardCfg.NEATTerminationPenaltyCollision;
		EpisodeStats.NEATFitness -= TerminationPenalty;
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Collision termination: base_fitness=%.2f penalty=%.2f final_fitness=%.2f"),
			*GetAgentLogId(), BaseFitness, TerminationPenalty, EpisodeStats.NEATFitness);
	}
	else if (TerminationReason == TEXT("Fell off track"))
	{
		TerminationPenalty = RewardCfg.NEATTerminationPenaltyFellOffTrack;
		EpisodeStats.NEATFitness -= TerminationPenalty;
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Fell off track termination: base_fitness=%.2f penalty=%.2f final_fitness=%.2f"),
			*GetAgentLogId(), BaseFitness, TerminationPenalty, EpisodeStats.NEATFitness);
	}

	EpisodeStats.NEATFitness = FMath::Max(EpisodeStats.NEATFitness, RewardCfg.NEATTerminationFitnessMin);

	// Fitness breakdown log (for debugging and regression verification)
	{
		const float ProgressM = EpisodeStats.DistanceTraveledCm / 100.f;
		float SpeedBonusEstimate = 0.f;
		if (ProgressM > 50.f && EpisodeStats.DurationSeconds > 0.f)
		{
			const float ProgressKmh = (EpisodeStats.DistanceTraveledCm / EpisodeStats.DurationSeconds / 100.f) * 3.6f;
			SpeedBonusEstimate = ProgressKmh * 0.1f;
		}
		const float ScaleFactor = (EpisodeStats.DurationSeconds < 2.0f) ? 0.5f : 1.0f;
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Fitness breakdown: NEATFitness=%.2f progress_m=%.1f speed_bonus_est=%.2f scale=%.1f duration=%.1fs termination=%s"),
			*GetAgentLogId(), EpisodeStats.NEATFitness, ProgressM, SpeedBonusEstimate * ScaleFactor,
			ScaleFactor, EpisodeStats.DurationSeconds, *EpisodeStats.TerminationReason);
	}
}

// ============================================================================
// Action Application
// ============================================================================

void URacingAgentComponent::ApplyAction(const FVehicleAction& Action)
{
	AActor* Car = GetVehicleActor();
	if (!Car)
	{
		return;
	}

	// Prefer CarInterface so controller-owned agents apply to the correct car.
	if (Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_Steering(Car, Action.Steer);
		ICarInterface::Execute_Throttle(Car, Action.Throttle);
		ICarInterface::Execute_Brake(Car, Action.Brake);
		return;
	}

	UChaosWheeledVehicleMovementComponent* MovementComp = Car->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	if (MovementComp)
	{
		MovementComp->SetSteeringInput(Action.Steer);
		MovementComp->SetThrottleInput(Action.Throttle);
		MovementComp->SetBrakeInput(Action.Brake);
	}
}

// ============================================================================
// Car resolution (CarInterface-driven)
// ============================================================================

AActor* URacingAgentComponent::GetResolvedCarActor() const
{
	AActor* Owner = GetOwner();
	bCarResolvedViaInterface = false;

	if (!Owner)
	{
		return nullptr;
	}

	// 1) Owner implements CarInterface -> use interface to get car actor (handles both pawn and controller implementations).
	if (Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		AActor* Car = ICarInterface::Execute_GetCarActor(Owner);
		if (Car)
		{
			bCarResolvedViaInterface = true;
			return Car;
		}
	}

	// 2) Owner is a controller -> resolve via possessed pawn (pawn may implement CarInterface).
	AController* Controller = Cast<AController>(Owner);
	if (Controller)
	{
		APawn* Pawn = Controller->GetPawn();
		if (Pawn && Pawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
		{
			AActor* Car = ICarInterface::Execute_GetCarActor(Pawn);
			if (Car)
			{
				bCarResolvedViaInterface = true;
				return Car;
			}
		}
		// Possessed pawn as car when it has vehicle movement (legacy or non-interface pawn).
		if (Pawn && Pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>())
		{
			return Pawn;
		}
		return nullptr;
	}

	// 3) Owner is the vehicle actor (component attached directly to pawn).
	if (Owner->FindComponentByClass<UChaosWheeledVehicleMovementComponent>())
	{
		return Owner;
	}

	return nullptr;
}

UPrimitiveComponent* URacingAgentComponent::GetResolvedCarRootComponent() const
{
	AActor* Car = GetResolvedCarActor();
	if (!Car)
	{
		return nullptr;
	}
	if (Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		UPrimitiveComponent* Root = ICarInterface::Execute_GetCarRootPrimitive(Car);
		if (Root)
		{
			return Root;
		}
	}
	return Cast<UPrimitiveComponent>(Car->GetRootComponent());
}

bool URacingAgentComponent::ValidateCarSetupForEvaluation(FString* OutFailureReason) const
{
	AActor* Owner = GetOwner();
	AActor* Car = GetResolvedCarActor();
	UPrimitiveComponent* RootComp = GetResolvedCarRootComponent();
	const UChaosWheeledVehicleMovementComponent* MovementComp = Car ? (Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass()) ? ICarInterface::Execute_GetCarChaosVehicleMovement(Car) : Car->FindComponentByClass<UChaosWheeledVehicleMovementComponent>()) : nullptr;

	const FString OwnerClass = Owner ? Owner->GetClass()->GetName() : TEXT("null");
	const FString CarClass = Car ? Car->GetClass()->GetName() : TEXT("null");
	const FString CarName = Car ? Car->GetName() : TEXT("null");

	if (!Car)
	{
		FString Reason;
		AController* Controller = Owner ? Cast<AController>(Owner) : nullptr;
		if (Controller)
		{
			APawn* Pawn = Controller->GetPawn();
			if (!Pawn)
			{
				Reason = FString::Printf(TEXT("Resolved car is null: Owner is AIController (%s) but GetPawn() is null. Possess a car pawn before authorizing evaluation."), *OwnerClass);
			}
			else if (!Pawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()) && !Pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>())
			{
				Reason = FString::Printf(TEXT("Resolved car is null: Possessed pawn %s (%s) does not implement CarInterface and has no ChaosWheeledVehicleMovementComponent."), *Pawn->GetName(), *Pawn->GetClass()->GetName());
			}
			else
			{
				Reason = FString::Printf(TEXT("Resolved car is null: Owner=%s possessed pawn returned null from GetCarActor."), *OwnerClass);
			}
		}
		else
		{
			Reason = FString::Printf(TEXT("Resolved car is null. Owner class=%s. Use CarInterface on Owner or possessed pawn, or attach component to the vehicle actor."), *OwnerClass);
		}
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] VALIDATION FAIL: %s"), *GetAgentLogId(), *Reason);
		if (OutFailureReason) *OutFailureReason = Reason;
		return false;
	}
	if (!RootComp)
	{
		const FString Reason = FString::Printf(TEXT("Resolved car has no primitive root component. Car=%s (%s)."), *CarName, *CarClass);
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] VALIDATION FAIL: %s"), *GetAgentLogId(), *Reason);
		if (OutFailureReason) *OutFailureReason = Reason;
		return false;
	}
	if (!MovementComp)
	{
		const FString Reason = FString::Printf(TEXT("Resolved car has no ChaosWheeledVehicleMovementComponent. Car=%s (%s)."), *CarName, *CarClass);
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] VALIDATION FAIL: %s"), *GetAgentLogId(), *Reason);
		if (OutFailureReason) *OutFailureReason = Reason;
		return false;
	}

	const FString RootClass = RootComp ? RootComp->GetClass()->GetName() : TEXT("null");
	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Car setup valid for evaluation: owner=%s CarInterface_path=%s resolved_car=%s (%s) root_primitive=%s movement_component=%s"),
		*GetAgentLogId(), *OwnerClass, bCarResolvedViaInterface ? TEXT("yes") : TEXT("no"), *CarName, *CarClass,
		*RootClass, MovementComp ? TEXT("present") : TEXT("missing"));
	return true;
}

// ============================================================================
// Helpers (delegate to resolved car; do not assume Owner is vehicle)
// ============================================================================

AActor* URacingAgentComponent::GetVehicleActor() const
{
	return GetResolvedCarActor();
}

UPrimitiveComponent* URacingAgentComponent::GetVehicleRootComponent() const
{
	return GetResolvedCarRootComponent();
}

APlayerStart* URacingAgentComponent::FindPlayerStart() const
{
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

	if (PlayerStarts.Num() > 0)
	{
		return Cast<APlayerStart>(PlayerStarts[0]);
	}

	return nullptr;
}

FString URacingAgentComponent::GetAgentLogId() const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		return FString::Printf(TEXT("Agent[%s]#%d"), *Owner->GetName(), GenomeID);
	}
	return TEXT("Agent[Unknown]");
}

void URacingAgentComponent::SetNeuralNetwork(USimpleNeuralNetwork* Network)
{
	PolicyNetwork = Network;
}

void URacingAgentComponent::SetPolicyBackend(UPolicyBackend* Backend)
{
	PolicyBackend = Backend;
}

bool URacingAgentComponent::HasNEATPolicyBackend() const
{
	return PolicyBackend && PolicyBackend->IsValid();
}

void URacingAgentComponent::ForceEpisodeDone()
{
	bEpisodeDone = true;
	PolicyBackend = nullptr;
	// Revoke authorization so this agent cannot start a new episode until re-authorized.
	if (RuntimeState == EAgentRuntimeState::EvaluationAuthorized || RuntimeState == EAgentRuntimeState::Evaluating)
	{
		RuntimeState = EAgentRuntimeState::Registered;
	}
	UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Agent deactivated for batch isolation (GenomeID=%d). RuntimeState=Registered. Authorization revoked."), *GetAgentLogId(), GenomeID);
}

// ============================================================================
// Debug Visualization
// ============================================================================

void URacingAgentComponent::DrawObservationHUD()
{
	if (!GEngine || !GetWorld())
	{
		return;
	}

	FString LidarLine = bEnableLidar
		? FString::Printf(TEXT("LIDAR: %d rays | ObsSize: %d\n"), LastObservation.LidarRays.Num(), LastObservation.Vector.Num())
		: TEXT("");

	FString HUDText = FString::Printf(
		TEXT("Agent #%d | Gen %d\n")
		TEXT("Speed: %.2f | YawRate: %.2f\n")
		TEXT("Rays: F=%.2f L=%.2f R=%.2f L45=%.2f R45=%.2f\n")
		TEXT("      FUp=%.2f FDown=%.2f Ground=%.2f\n")
		TEXT("Gravity: [%.2f, %.2f, %.2f]\n")
		TEXT("%s")
		TEXT("Steps: %d | Fitness: %.2f"),
		GenomeID, Generation,
		LastObservation.SpeedNorm, LastObservation.YawRateNorm,
		LastObservation.RayForward, LastObservation.RayLeft, LastObservation.RayRight,
		LastObservation.RayLeft45, LastObservation.RayRight45,
		LastObservation.RayForwardUp, LastObservation.RayForwardDown, LastObservation.RayGroundDist,
		LastObservation.GravityX, LastObservation.GravityY, LastObservation.GravityZ,
		*LidarLine,
		EpisodeStepCount, GetEpisodeFitness()
	);

	GEngine->AddOnScreenDebugMessage(
		(int32)(GetUniqueID() + 1000),
		0.f,
		FColor::Cyan,
		HUDText
	);
}

void URacingAgentComponent::DrawRayAnglesDebug()
{
	if (!GEngine || !GetWorld())
	{
		return;
	}

	FString AnglesText = FString::Printf(
		TEXT("Adaptive Ray Angles (Pitch °):\n")
		TEXT("  Forward: %.1f° | Left: %.1f° | Right: %.1f°\n")
		TEXT("  Left45: %.1f° | Right45: %.1f°"),
		RayState_Forward.CurrentPitchDeg,
		RayState_Left.CurrentPitchDeg,
		RayState_Right.CurrentPitchDeg,
		RayState_Left45.CurrentPitchDeg,
		RayState_Right45.CurrentPitchDeg
	);

	GEngine->AddOnScreenDebugMessage(
		(int32)(GetUniqueID() + 2000),
		0.f,
		FColor::Yellow,
		AnglesText
	);
}