#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/RacingAgentTypes.h"
#include "RacingAgentComponent.generated.h"

class USplineComponent;
class APlayerStart;
class USimpleNeuralNetwork;

/**
 * Racing AI Agent with Adaptive Ray-based Vision and NEAT Evolution.
 *
 * Sensorik:
 * - 8 Adaptive Ray Traces (5 horizontal + 2 fixed vertical + 1 ground)
 * - Rays adjust pitch angle automatically to track limits
 * - IMU Sensor (Gravity direction for loops)
 * - Vehicle State (Speed, Angular Velocities)
 *
 * Training:
 * - NEAT (NeuroEvolution of Augmenting Topologies)
 * - Fitness = Distance + Speed Bonus
 * - Spawning at Player Start
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CARAIRUNTIME_API URacingAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URacingAgentComponent();

	// ===== Lifecycle =====

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void Initialize();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void ResetEpisode();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void StepOnce(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void SetNeuralNetwork(USimpleNeuralNetwork* Network);

	// ===== Observation =====

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	FRacingObservation BuildObservation();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	FRacingObservation GetLastObservation() const { return LastObservation; }

	// ===== Reward =====

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	FRewardBreakdown ComputeReward(const FRacingObservation& Obs, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	float GetEpisodeFitness() const;

	// ===== Episode Stats =====

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	FEpisodeStats GetEpisodeStats() const { return EpisodeStats; }

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	bool IsDone() const { return bEpisodeDone; }

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	int32 GetEpisodeStepCount() const { return EpisodeStepCount; }

	// ===== Configuration =====

	// --- Adaptive Ray Trace Settings ---

	/** Max ray trace distance (cm) */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	float RayMaxDistanceCm = 2000.f;

	/** Forward offset from vehicle center for ray origin */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	float RayStartOffsetCm = 100.f;

	/** Height offset above vehicle center for ray origin */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	float RayHeightOffsetCm = 50.f;

	/** Collision channel for ray traces */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	TEnumAsByte<ECollisionChannel> RayTraceChannel = ECC_Visibility;

	/** Adaptation rate (degrees per frame) - how fast rays adjust */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays", meta = (ClampMin = 0.5, ClampMax = 10.0))
	float RayAdaptationRate = 2.f;

	/** Target distance for rays to maintain (normalized 0-1) */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays", meta = (ClampMin = 0.3, ClampMax = 0.9))
	float RayTargetDistNorm = 0.7f;

	/** Enable adaptive ray angle adjustment */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	bool bEnableAdaptiveRays = true;

	/** Draw debug lines for ray traces */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	bool bDrawRayDebug = false;

	/** Debug line thickness */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	float RayDebugLineThickness = 3.f;

	/** Draw adaptive ray angles as text */
	UPROPERTY(EditAnywhere, Category = "Racing|Adaptive Rays")
	bool bDrawRayAnglesDebug = false;

	// --- Ground Ray Settings ---

	/** Max ground trace distance (cm) */
	UPROPERTY(EditAnywhere, Category = "Racing|Ground Ray")
	float GroundRayMaxDistanceCm = 500.f;

	// --- LIDAR Settings ---

	/** Enable optional LIDAR ring sensor.
	 *  Appends LidarNumRays values to the observation vector.
	 *  Requires a freshly initialized neural network — incompatible with
	 *  models trained without LIDAR. */
	UPROPERTY(EditAnywhere, Category = "Racing|LIDAR")
	bool bEnableLidar = false;

	/** Number of evenly spaced horizontal rays in the LIDAR ring (4–32). */
	UPROPERTY(EditAnywhere, Category = "Racing|LIDAR", meta = (ClampMin = 4, ClampMax = 32, EditCondition = "bEnableLidar"))
	int32 LidarNumRays = 16;

	/** Max LIDAR ray distance (cm). */
	UPROPERTY(EditAnywhere, Category = "Racing|LIDAR", meta = (EditCondition = "bEnableLidar"))
	float LidarMaxDistanceCm = 2000.f;

	// --- IMU Sensor Settings ---

	/** Enable IMU sensor (gravity direction) */
	UPROPERTY(EditAnywhere, Category = "Racing|IMU Sensor")
	bool bEnableIMUSensor = true;

	/** Smooth gravity direction over multiple frames */
	UPROPERTY(EditAnywhere, Category = "Racing|IMU Sensor")
	bool bSmoothGravity = true;

	/** Gravity smoothing factor [0,1] - 1=instant, 0=frozen */
	UPROPERTY(EditAnywhere, Category = "Racing|IMU Sensor", meta = (ClampMin = 0.1, ClampMax = 1.0))
	float GravitySmoothingFactor = 0.3f;

	// --- Observation Normalization ---

	UPROPERTY(EditAnywhere, Category = "Racing|Normalization")
	float SpeedNormCmPerSec = 4500.f;

	UPROPERTY(EditAnywhere, Category = "Racing|Normalization")
	float AngVelNormDegPerSec = 220.f;

	// --- Reward Configuration ---

	UPROPERTY(EditAnywhere, Category = "Racing|Reward")
	FRacingRewardConfig RewardCfg;

	// --- Spawning ---

	UPROPERTY(EditAnywhere, Category = "Racing|Spawning")
	float SpawnLateralOffsetMaxCm = 300.f;

	UPROPERTY(EditAnywhere, Category = "Racing|Spawning")
	float SpawnHeightOffsetCm = 50.f;

	UPROPERTY(EditAnywhere, Category = "Racing|Spawning")
	int32 SpawnRandomSeed = 0;

	// --- NEAT Settings ---

	UPROPERTY(VisibleAnywhere, Category = "Racing|NEAT")
	int32 GenomeID = -1;

	UPROPERTY(VisibleAnywhere, Category = "Racing|NEAT")
	int32 Generation = 0;

	// --- Debug ---

	UPROPERTY(EditAnywhere, Category = "Racing|Debug")
	bool bEnableLogging = false;

	UPROPERTY(EditAnywhere, Category = "Racing|Debug")
	bool bDrawObservationHUD = false;

	// ===== Events =====

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEpisodeDone, const FEpisodeStats&, Stats);
	UPROPERTY(BlueprintAssignable, Category = "Racing Agent")
	FOnEpisodeDone OnEpisodeDone;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStepCompleted, const FRacingObservation&, Obs, const FRewardBreakdown&, Reward);
	UPROPERTY(BlueprintAssignable, Category = "Racing Agent")
	FOnStepCompleted OnStepCompleted;

protected:
	// ===== Internal State =====

	UPROPERTY() TObjectPtr<USimpleNeuralNetwork> PolicyNetwork;
	UPROPERTY() FRacingObservation LastObservation;
	UPROPERTY() FVehicleAction LastAction;
	UPROPERTY() FEpisodeStats EpisodeStats;
	UPROPERTY() bool bEpisodeDone = false;
	UPROPERTY() int32 EpisodeStepCount = 0;
	UPROPERTY() float EpisodeTimeAccum = 0.f;
	UPROPERTY() float AirborneTimeAccum = 0.f;
	UPROPERTY() float StuckTimeAccum = 0.f;
	UPROPERTY() FVector EpisodeStartLocation = FVector::ZeroVector;
	UPROPERTY() FRandomStream SpawnRng;

	// ===== Adaptive Ray State =====

	/** State for each adaptive ray */
	UPROPERTY()
	FAdaptiveRayState RayState_Forward;

	UPROPERTY()
	FAdaptiveRayState RayState_Left;

	UPROPERTY()
	FAdaptiveRayState RayState_Right;

	UPROPERTY()
	FAdaptiveRayState RayState_Left45;

	UPROPERTY()
	FAdaptiveRayState RayState_Right45;

	// ===== IMU State =====

	/** Smoothed gravity direction (vehicle-local space) */
	UPROPERTY()
	FVector SmoothedGravityLocal = FVector(0, 0, -1);

	// ===== Helper Methods =====

	AActor* GetVehicleActor() const;
	UPrimitiveComponent* GetVehicleRootComponent() const;
	void ApplyAction(const FVehicleAction& Action);

	/** Trace adaptive ray with current pitch angle */
	float TraceAdaptiveRay(
		const FVector& Origin,
		const FVector& YawDirection,
		FAdaptiveRayState& RayState,
		FColor DebugColor = FColor::Red
	);

	/** Trace fixed ray (no adaptation) */
	float TraceFixedRay(
		const FVector& Origin,
		const FVector& Direction,
		float MaxDistance,
		FColor DebugColor = FColor::Red
	);

	/** Trace evenly spaced horizontal LIDAR ring and populate OutRays. */
	void BuildLidarObservation(const FVector& Origin, const FVector& Forward, TArray<float>& OutRays);

	/** Update all adaptive ray angles based on last hits */
	void UpdateAdaptiveRayAngles();

	/** Compute IMU gravity direction (vehicle-local space) */
	FVector ComputeGravityDirection();

	/** Reset all adaptive ray states */
	void ResetAdaptiveRays();

	APlayerStart* FindPlayerStart() const;
	void ResetEpisodeAccumulators();
	bool CheckTerminalConditions(const FRacingObservation& Obs, float DeltaTime, FString& OutReason);
	void FinalizeEpisodeStats(const FString& TerminationReason);
	FString GetAgentLogId() const;
	void DrawObservationHUD();
	void DrawRayAnglesDebug();
};