#include "Components/RacingAgentComponent.h"
#include "CarAIRuntimeLogging.h"
#include "NN/SimpleNeuralNetwork.h"
#include "NN/NEATPolicyBackend.h"
#include "Subsystems/RacingAgentRegistrySubsystem.h"

#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SplineComponent.h"
#include "Interfaces/RoadSplineInterface.h"
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

	RuntimeState = EAgentRuntimeState::Evaluating;
	Activate();
	SetComponentTickEnabled(true);
	ResetEpisode();

	UE_LOG(LogCarAIAgent, Display, TEXT("[%s] Agent initialized and evaluating. RuntimeState=Evaluating GenomeID=%d. StepOnce will run each tick until episode ends."),
		*GetAgentLogId(), GenomeID);
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
	APlayerStart* PlayerStart = FindPlayerStart();
	if (!PlayerStart)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] No Player Start found!"), *GetAgentLogId());
		return;
	}

	FVector SpawnLoc = PlayerStart->GetActorLocation();
	FRotator SpawnRot = PlayerStart->GetActorRotation();

	if (SpawnLateralOffsetMaxCm > 0.f)
	{
		float LateralOffset = SpawnRng.FRandRange(-SpawnLateralOffsetMaxCm, SpawnLateralOffsetMaxCm);
		FVector RightVec = SpawnRot.RotateVector(FVector::RightVector);
		SpawnLoc += RightVec * LateralOffset;
	}

	SpawnLoc.Z += SpawnHeightOffsetCm;

	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] No vehicle actor!"), *GetAgentLogId());
		return;
	}

	Vehicle->SetActorLocationAndRotation(SpawnLoc, SpawnRot);

	if (UPrimitiveComponent* RootComp = GetVehicleRootComponent())
	{
		RootComp->SetPhysicsLinearVelocity(FVector::ZeroVector);
		RootComp->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	ResetEpisodeAccumulators();
	ResetAdaptiveRays();

	UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Episode reset. RuntimeState=%d GenomeID=%d. Stepping begins this tick."),
		*GetAgentLogId(), (int32)RuntimeState, GenomeID);
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

	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		UE_LOG(LogCarAIAgent, Error, TEXT("[%s] StepOnce: no vehicle actor; skipping step"), *GetAgentLogId());
		return;
	}

	// First step this episode: log that stepping has started
	if (EpisodeStepCount == 0)
	{
		UE_LOG(LogCarAIAgent, Display, TEXT("[%s] First step this episode. RuntimeState=%d GenomeID=%d Generation=%d"),
			*GetAgentLogId(), (int32)RuntimeState, GenomeID, Generation);
	}
	else if (bEnableLogging && (EpisodeStepCount % 100 == 0))
	{
		UE_LOG(LogCarAIAgent, Verbose, TEXT("[%s] Step %d (GenomeID=%d)"), *GetAgentLogId(), EpisodeStepCount, GenomeID);
	}

	// 1. Build Observation
	FRacingObservation Obs = BuildObservation();
	LastObservation = Obs;

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
		// Clamp at agent layer (not inside backend)
		Action.Steer = FMath::Clamp(PolicyOutput[0], -1.f, 1.f);
		Action.Throttle = FMath::Clamp(PolicyOutput[1], 0.f, 1.f);
		Action.Brake = FMath::Clamp(PolicyOutput[2], 0.f, 1.f);
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

	// 8. Update Adaptive Rays
	if (bEnableAdaptiveRays)
	{
		UpdateAdaptiveRayAngles();
	}

	// 9. Broadcast step completed
	OnStepCompleted.Broadcast(Obs, Reward);

	// 10. Debug HUD
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
		return Obs;
	}

	UPrimitiveComponent* RootComp = GetVehicleRootComponent();
	if (!RootComp)
	{
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
		FCollisionQueryParams(TEXT("GroundCheck"), false, Vehicle)
	);

	if (bHasGround)
	{
		float GroundDist = (GroundHit.ImpactPoint - GroundStart).Size();
		Obs.RayGroundDist = FMath::Clamp(GroundDist / GroundRayMaxDistanceCm, 0.f, 1.f);
	}
	else
	{
		Obs.RayGroundDist = 0.f; // No ground = danger!
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
		FCollisionQueryParams(TEXT("AdaptiveRay"), false, GetVehicleActor())
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
		FCollisionQueryParams(TEXT("FixedRay"), false, GetVehicleActor())
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
		R.bDone = true;
		R.DoneReason = TEXT("Collision");
		R.Collision = RewardCfg.CollisionTerminalPenalty;
	}

	// ===== Gap Penalty (Ground Ray) =====

	if (Obs.RayGroundDist < RewardCfg.GapWarningThreshold)
	{
		R.GapPenalty = RewardCfg.GapWarningPenalty;
	}

	if (Obs.RayGroundDist < RewardCfg.GapTerminalThreshold)
	{
		R.bDone = true;
		R.DoneReason = TEXT("Fell off track");
		R.GapPenalty = RewardCfg.GapTerminalPenalty;
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
	// Max steps
	if (EpisodeStepCount >= RewardCfg.MaxEpisodeSteps)
	{
		OutReason = TEXT("MaxSteps");
		return true;
	}

	// Airborne too long
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

	// Stuck
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
	AActor* Vehicle = GetVehicleActor();
	if (!Vehicle)
	{
		return;
	}

	UChaosWheeledVehicleMovementComponent* MovementComp = Vehicle->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	if (MovementComp)
	{
		MovementComp->SetSteeringInput(Action.Steer);
		MovementComp->SetThrottleInput(Action.Throttle);
		MovementComp->SetBrakeInput(Action.Brake);
	}
}

// ============================================================================
// Helpers
// ============================================================================

AActor* URacingAgentComponent::GetVehicleActor() const
{
	return GetOwner();
}

UPrimitiveComponent* URacingAgentComponent::GetVehicleRootComponent() const
{
	AActor* Vehicle = GetVehicleActor();
	return Vehicle ? Cast<UPrimitiveComponent>(Vehicle->GetRootComponent()) : nullptr;
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