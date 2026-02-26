#pragma once

#include "CoreMinimal.h"
#include "RacingAgentTypes.generated.h"

// ============================================================================
// Adaptive Ray State (per Ray)
// ============================================================================

USTRUCT(BlueprintType)
struct FAdaptiveRayState
{
	GENERATED_BODY()

	/** Current pitch angle (degrees) - positive=up, negative=down */
	UPROPERTY(VisibleAnywhere)
	float CurrentPitchDeg = 0.f;

	/** Last hit distance (normalized [0,1]) */
	UPROPERTY(VisibleAnywhere)
	float LastHitDistNorm = 1.f;

	/** Number of consecutive misses (no hit) */
	UPROPERTY(VisibleAnywhere)
	int32 ConsecutiveMisses = 0;

	/** Target distance to maintain (normalized) - rays try to hit at this distance */
	UPROPERTY(VisibleAnywhere)
	float TargetDistNorm = 0.7f;

	/** Adaptation rate (degrees per frame) */
	UPROPERTY(VisibleAnywhere)
	float AdaptationRate = 2.f;

	/** Min/Max pitch limits */
	static constexpr float MIN_PITCH_DEG = -45.f;
	static constexpr float MAX_PITCH_DEG = 45.f;

	/** Update pitch angle based on last hit */
	void UpdatePitchAngle(bool bHadHit, float HitDistNorm)
	{
		if (!bHadHit)
		{
			// No hit → Ray is shooting into void → angle DOWN
			ConsecutiveMisses++;
			float AdjustmentDeg = AdaptationRate * FMath::Min(ConsecutiveMisses, 5); // Accelerate after multiple misses
			CurrentPitchDeg = FMath::Clamp(CurrentPitchDeg - AdjustmentDeg, MIN_PITCH_DEG, MAX_PITCH_DEG);
		}
		else
		{
			ConsecutiveMisses = 0;
			LastHitDistNorm = HitDistNorm;

			// Hit too close → angle UP
			if (HitDistNorm < TargetDistNorm - 0.1f)
			{
				CurrentPitchDeg = FMath::Clamp(CurrentPitchDeg + AdaptationRate, MIN_PITCH_DEG, MAX_PITCH_DEG);
			}
			// Hit too far → angle DOWN
			else if (HitDistNorm > TargetDistNorm + 0.1f)
			{
				CurrentPitchDeg = FMath::Clamp(CurrentPitchDeg - AdaptationRate, MIN_PITCH_DEG, MAX_PITCH_DEG);
			}
			// Hit at target distance → no change
		}
	}

	/** Reset to default state */
	void Reset()
	{
		CurrentPitchDeg = 0.f;
		LastHitDistNorm = 1.f;
		ConsecutiveMisses = 0;
	}
};

// ============================================================================
// Observation (Adaptive Ray-based + IMU)
// ============================================================================

USTRUCT(BlueprintType)
struct FRacingObservation
{
	GENERATED_BODY()

	// ===== Vehicle State (4) =====

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float SpeedNorm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float YawRateNorm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float PitchRateNorm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RollRateNorm = 0.f;

	// ===== Adaptive Ray Traces (8) =====

	/** Forward ray (0° yaw, adaptive pitch) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayForward = 1.f;

	/** Left ray (90° yaw, adaptive pitch) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayLeft = 1.f;

	/** Right ray (-90° yaw, adaptive pitch) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayRight = 1.f;

	/** Left-forward ray (45° yaw, adaptive pitch) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayLeft45 = 1.f;

	/** Right-forward ray (-45° yaw, adaptive pitch) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayRight45 = 1.f;

	/** Forward-up ray (0° yaw, fixed +30° pitch) - detect overhangs/loops */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayForwardUp = 1.f;

	/** Forward-down ray (0° yaw, fixed -30° pitch) - detect ramps/drops */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayForwardDown = 1.f;

	/** Ground distance ray (straight down) - gap detection */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float RayGroundDist = 1.f;

	// ===== IMU Sensor - Gravity Direction (3) =====

	/** Gravity vector in vehicle's local space (normalized) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float GravityX = 0.f; // Forward/Backward component

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float GravityY = 0.f; // Left/Right component

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float GravityZ = -1.f; // Up/Down component (default: -1 = pointing down)

	// ===== LIDAR Ring (optional, N values) =====

	/** Evenly spaced horizontal ring rays, normalized [0,1].
	 *  Empty when bEnableLidar = false on the agent component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<float> LidarRays;

	// ===== Flattened Vector =====

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<float> Vector;

	/** Base observation size (without LIDAR). */
	static constexpr int32 BASE_OBSERVATION_SIZE = 15;

	void BuildVector()
	{
		Vector = {
			// Vehicle State (4)
			SpeedNorm,
			YawRateNorm,
			PitchRateNorm,
			RollRateNorm,
			// Adaptive Rays (8)
			RayForward,
			RayLeft,
			RayRight,
			RayLeft45,
			RayRight45,
			RayForwardUp,
			RayForwardDown,
			RayGroundDist,
			// IMU Gravity (3)
			GravityX,
			GravityY,
			GravityZ
		};
		// Append optional LIDAR ring rays
		if (LidarRays.Num() > 0)
		{
			Vector.Append(LidarRays);
		}
	}
};

// ============================================================================
// Reward Breakdown
// ============================================================================

USTRUCT(BlueprintType)
struct FRewardBreakdown
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Distance = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Speed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Survival = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Smoothness = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Collision = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float GapPenalty = 0.f; // NEW: Penalty for approaching gaps

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Total = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bDone = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString DoneReason;
};

// ============================================================================
// Reward Config
// ============================================================================

USTRUCT(BlueprintType)
struct FRacingRewardConfig
{
	GENERATED_BODY()

	// ===== Phase 1: Distance Maximization =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 1 - Distance")
	float W_Distance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 1 - Distance")
	float W_Survival = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 1 - Distance")
	float Phase2ActivationDistanceCm = 5000.f;

	// ===== Phase 2: Speed Optimization =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 2 - Speed")
	float W_Speed = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase 2 - Speed")
	float SpeedTargetNorm = 0.7f;

	// ===== Smoothness =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothness")
	float W_ActionSmooth = -0.02f;

	// ===== Collision Detection (Adaptive Rays) =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionWarningThreshold = 0.15f; // 15% of max ray distance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionTerminalThreshold = 0.05f; // 5% = collision

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionWarningPenalty = -0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	float CollisionTerminalPenalty = -2.0f;

	// ===== Gap Detection (Ground Ray) =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gap Detection")
	float GapWarningThreshold = 0.3f; // 30% of max ground distance

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gap Detection")
	float GapTerminalThreshold = 0.1f; // 10% = falling

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gap Detection")
	float GapWarningPenalty = -0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gap Detection")
	float GapTerminalPenalty = -2.0f;

	// ===== Airborne =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airborne")
	float AirborneMaxSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airborne")
	float W_Airborne = -0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airborne")
	float TerminalPenalty_AirborneLong = -2.0f;

	// ===== Stuck Detection =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stuck Detection")
	float StuckSpeedNorm = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stuck Detection")
	float StuckTimeSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stuck Detection")
	float TerminalPenalty_Stuck = -2.0f;

	// ===== General =====

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	float MaxAbsTerm = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "General")
	int32 MaxEpisodeSteps = 5000;
};

// ============================================================================
// Vehicle Action
// ============================================================================

USTRUCT(BlueprintType)
struct FVehicleAction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Steer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Throttle = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Brake = 0.f;
};

// ============================================================================
// Episode Stats
// ============================================================================

USTRUCT(BlueprintType)
struct FEpisodeStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TotalReward = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 StepCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DurationSeconds = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DistanceTraveledCm = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float AvgSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float MaxSpeed = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString TerminationReason;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FDateTime StartTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FDateTime EndTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float NEATFitness = 0.f;

	void CalculateNEATFitness()
	{
		float DistanceMeters = DistanceTraveledCm / 100.f;
		float SpeedBonus = 0.f;

		if (DistanceMeters > 50.f)
		{
			float AvgSpeedKmh = (AvgSpeed / 100.f) * 3.6f;
			SpeedBonus = AvgSpeedKmh * 0.1f;
		}

		NEATFitness = DistanceMeters + SpeedBonus;

		if (DurationSeconds < 2.0f)
		{
			NEATFitness *= 0.5f;
		}
	}
};

// ============================================================================
// NEAT Genome Data
// ============================================================================

USTRUCT(BlueprintType)
struct FNEATGenomeData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 GenomeID = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 Generation = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Fitness = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<int32> NodeIDs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FString> Connections;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<FString> Activations;
};

// ============================================================================
// Training State
// ============================================================================

UENUM(BlueprintType)
enum class ENEATTrainingState : uint8
{
	Idle,
	Evaluating,
	WaitingForPython,
	Completed
};

USTRUCT(BlueprintType)
struct FNEATTrainingStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CurrentGeneration = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalEvaluations = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float BestFitness = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float AvgFitness = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 BestGenomeID = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FDateTime TrainingStartTime;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float ElapsedSeconds = 0.f;
};