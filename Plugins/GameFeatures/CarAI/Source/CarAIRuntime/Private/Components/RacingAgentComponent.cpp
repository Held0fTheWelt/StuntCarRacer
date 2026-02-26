#include "Components/RacingAgentComponent.h"
#include "NN/SimpleNeuralNetwork.h"

#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "WheeledVehiclePawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"

// ============================================================================
// Lifecycle
// ============================================================================

URacingAgentComponent::URacingAgentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = false;
}

void URacingAgentComponent::BeginPlay()
{
	Super::BeginPlay();

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
}

void URacingAgentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URacingAgentComponent::Initialize()
{
	if (bEnableLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Initialize()"), *GetAgentLogId());
	}

	ResetEpisode();
	SetComponentTickEnabled(true);
}

// ============================================================================
// Episode Management
// ============================================================================

void URacingAgentComponent::ResetEpisode()
{
	APlayerStart* PlayerStart = FindPlayerStart();
	if (!PlayerStart)
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] No Player Start found!"), *GetAgentLogId());
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
		UE_LOG(LogTemp, Error, TEXT("[%s] No vehicle actor!"), *GetAgentLogId());
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

	if (bEnableLogging)
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] Episode reset at Player Start"), *GetAgentLogId());
	}
}

void URacingAgentComponent::ResetEpisodeAccumulators()
{
	bEpisodeDone = false;
	EpisodeStepCount = 0;
	EpisodeTimeAccum = 0.f;
	AirborneTimeAccum = 0.f;
	StuckTimeAccum = 0.f;

	EpisodeStartLocation = GetVehicleActor()->GetActorLocation();

	EpisodeStats = FEpisodeStats();
	EpisodeStats.StartTime = FDateTime::Now();
	EpisodeStats.DistanceTraveledCm = 0.f;
	EpisodeStats.MaxSpeed = 0.f;
	EpisodeStats.AvgSpeed = 0.f;

	LastAction = FVehicleAction();

	// Reset IMU
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

// ============================================================================
// Training Step
// ============================================================================

void URacingAgentComponent::StepOnce(float DeltaTime)
{
	if (bEpisodeDone)
	{
		return;
	}

	// 1. Build Observation
	FRacingObservation Obs = BuildObservation();
	LastObservation = Obs;

	// 2. Compute Reward
	FRewardBreakdown Reward = ComputeReward(Obs, DeltaTime);

	// 3. Get Action from Policy Network
	FVehicleAction Action;
	if (PolicyNetwork)
	{
		TArray<float> PolicyOutput;
		PolicyNetwork->ForwardPolicy(Obs.Vector, PolicyOutput);
		if (PolicyOutput.Num() == 3)
		{
			Action.Steer = FMath::Clamp(PolicyOutput[0], -1.f, 1.f);
			Action.Throttle = FMath::Clamp(PolicyOutput[1], 0.f, 1.f);
			Action.Brake = FMath::Clamp(PolicyOutput[2], 0.f, 1.f);
		}
	}
	else
	{
		// Fallback: Go forward
		Action.Steer = 0.f;
		Action.Throttle = 0.5f;
		Action.Brake = 0.f;
	}

	// 4. Apply Action
	ApplyAction(Action);
	LastAction = Action;

	// 5. Update Episode Stats
	EpisodeStepCount++;
	EpisodeTimeAccum += DeltaTime;
	EpisodeStats.TotalReward += Reward.Total;
	EpisodeStats.StepCount = EpisodeStepCount;
	EpisodeStats.DurationSeconds = EpisodeTimeAccum;

	FVector CurrentLoc = GetVehicleActor()->GetActorLocation();
	float DistThisStep = (CurrentLoc - EpisodeStartLocation).Size();
	EpisodeStats.DistanceTraveledCm = DistThisStep;

	float CurrentSpeed = Obs.SpeedNorm * SpeedNormCmPerSec;
	EpisodeStats.MaxSpeed = FMath::Max(EpisodeStats.MaxSpeed, CurrentSpeed);

	// 6. Check Terminal Conditions
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

		if (bEnableLogging)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Episode done: %s (Fitness: %.2f, Steps: %d, Distance: %.1fm)"),
				*GetAgentLogId(), *TermReason, EpisodeStats.NEATFitness, EpisodeStats.StepCount,
				EpisodeStats.DistanceTraveledCm / 100.f);
		}

		return;
	}

	// 7. Update Adaptive Rays
	if (bEnableAdaptiveRays)
	{
		UpdateAdaptiveRayAngles();
	}

	// 8. Broadcast step completed
	OnStepCompleted.Broadcast(Obs, Reward);

	// 9. Debug HUD
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

	// ===== Optional LIDAR Ring =====

	if (bEnableLidar)
	{
		BuildLidarObservation(Origin, Forward, Obs.LidarRays);
	}

	// ===== Build Vector =====
	Obs.BuildVector();

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

	// ===== Distance Reward =====

	FVector CurrentLoc = GetVehicleActor()->GetActorLocation();
	float DistanceMeters = (CurrentLoc - EpisodeStartLocation).Size() / 100.f;

	R.Distance = DistanceMeters * RewardCfg.W_Distance;

	// ===== Survival Bonus =====

	R.Survival = EpisodeTimeAccum * RewardCfg.W_Survival;

	// ===== Speed Bonus (Phase 2) =====

	if (EpisodeStats.DistanceTraveledCm > RewardCfg.Phase2ActivationDistanceCm)
	{
		float SpeedDiff = FMath::Abs(Obs.SpeedNorm - RewardCfg.SpeedTargetNorm);
		R.Speed = (1.f - SpeedDiff) * RewardCfg.W_Speed;
	}

	// ===== Smoothness =====

	float SteerDiff = FMath::Abs(LastAction.Steer - Obs.SpeedNorm); // Simplified
	R.Smoothness = SteerDiff * RewardCfg.W_ActionSmooth;

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