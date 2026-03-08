#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/RacingAgentTypes.h"
#include "RacingAgentComponent.generated.h"

/**
 * Explicit runtime state for a RacingAgentComponent.
 *
 * Transitions (single direction, only manager may advance):
 *   Idle -> Registered -> EvaluationAuthorized -> Evaluating
 * Resets to Idle on PIE end, unregister, or ForceEpisodeDone.
 */
UENUM(BlueprintType)
enum class EAgentRuntimeState : uint8
{
	/** Component attached; no registration, no stepping. Default after BeginPlay. */
	Idle,
	/** Registered with training manager. Still no stepping — awaiting genome assignment. */
	Registered,
	/** Genome assigned and evaluation authorized by the training workflow. Stepping may begin. */
	EvaluationAuthorized,
	/** Currently executing a training episode (stepping each tick). */
	Evaluating,
};

class USplineComponent;
class APlayerStart;
class USimpleNeuralNetwork;
class UPolicyBackend;

/**
 * Racing AI Agent with Adaptive Ray-based Vision and NEAT Evolution.
 *
 * Runtime state machine (EAgentRuntimeState):
 *   Idle -> Registered -> EvaluationAuthorized -> Evaluating
 *
 *   Idle:                 Component attached (BeginPlay). No stepping. No genome. No authorization.
 *   Registered:           MarkRegistered() called by training manager. Still no stepping.
 *                         No genome or policy backend assigned yet.
 *   EvaluationAuthorized: GrantEvaluationAuthorization() called after genome+backend are valid.
 *                         Initialize() may now be called; stepping begins once called.
 *   Evaluating:           Initialize() has been called; TickComponent runs StepOnce() each tick.
 *
 * Hard guards (agent lifecycle / evaluation authorization gate):
 *   - TickComponent() early-outs unless RuntimeState is EvaluationAuthorized or Evaluating.
 *   - StepOnce() will not run a NEAT evaluation step unless GenomeID >= 0 and HasNEATPolicyBackend().
 *   - Registering the agent (MarkRegistered) does NOT grant evaluation authorization.
 *   - GrantEvaluationAuthorization() is only valid when GenomeID >= 0 and HasNEATPolicyBackend(); the manager assigns both before authorizing.
 *   - In the training path, no reward, episode start, or fallback drive can occur before authorization; GenomeID=-1 fallback-driving is impossible once authorized (NEAT path uses zero action if no backend).
 *   - Authorization is revoked on ForceEpisodeDone, ResetToIdle, and PIE end.
 *
 * Policy: NEAT evaluation (GenomeID >= 0) uses only PolicyBackend; no fallback to PolicyNetwork or
 * drive-forward. Missing backend is logged and agent applies zero action. Non-NEAT (GenomeID < 0)
 * may use PolicyBackend then PolicyNetwork, then optional fallback drive with explicit log.
 *
 * Policy state: Use HasNEATPolicyBackend() for executable NEAT policy, HasFixedNetworkPolicy() for
 * fixed network, HasAnyPolicy() for either. No policy loaded = both false.
 *
 * Sensors: 8 Adaptive Rays + IMU + Vehicle State. Training: NEAT fitness = track progress (m) + speed bonus.
 *
 * Developer note: to see verbose lifecycle logs in PIE, add to DefaultEngine.ini:
 *   [Core.Log]
 *   LogCarAIAgent=Verbose
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class CARAIRUNTIME_API URacingAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URacingAgentComponent();

	// ===== Lifecycle =====

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void Initialize();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void ResetEpisode();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void StepOnce(float DeltaTime);

	// ===== Authorization Gate (MVP-RT-01) =====

	/**
	 * Grant evaluation authorization. Must only be called by the training manager after
	 * a valid genome and policy backend have been assigned (GenomeID >= 0, HasNEATPolicyBackend()).
	 * Transitions state: Registered -> EvaluationAuthorized.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void GrantEvaluationAuthorization();

	/**
	 * Revoke evaluation authorization. Transitions state back to Registered (or Idle if unregistered).
	 * Called when PIE ends, the agent is unassigned, or training stops.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void RevokeEvaluationAuthorization();

	/**
	 * Mark the agent as registered with the training manager.
	 * Transitions state: Idle -> Registered. Does NOT grant evaluation authorization.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void MarkRegistered();

	/**
	 * Reset the agent to idle state (Idle). Clears genome, policy, and authorization.
	 * Called on unregister or PIE end.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void ResetToIdle();

	/** Current runtime state (Idle / Registered / EvaluationAuthorized / Evaluating). */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	EAgentRuntimeState GetRuntimeState() const { return RuntimeState; }

	/** True only when evaluation authorization has been explicitly granted and a valid policy is present. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	bool IsEvaluationAuthorized() const { return RuntimeState == EAgentRuntimeState::EvaluationAuthorized || RuntimeState == EAgentRuntimeState::Evaluating; }

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void SetNeuralNetwork(USimpleNeuralNetwork* Network);

	/** Set policy backend (e.g. NEAT graph evaluator). Takes precedence over PolicyNetwork when valid. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void SetPolicyBackend(UPolicyBackend* Backend);

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

	/**
	 * Deactivate this agent for batch isolation: sets episode done and clears the policy backend.
	 * Does NOT broadcast OnEpisodeDone. Call on agents outside the active batch to prevent
	 * stale episode completions from corrupting fitness attribution.
	 */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	void ForceEpisodeDone();

	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	int32 GetEpisodeStepCount() const { return EpisodeStepCount; }

	/** True if a NEAT policy backend is set and valid (executable). Distinguish: no policy vs fixed network vs NEAT. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	bool HasNEATPolicyBackend() const;

	/** True if a fixed network (SimpleNeuralNetwork) is set. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	bool HasFixedNetworkPolicy() const { return PolicyNetwork != nullptr; }

	/** True if the agent has any executable policy (NEAT or fixed network). No policy = both false. */
	UFUNCTION(BlueprintCallable, Category = "Racing Agent")
	bool HasAnyPolicy() const { return HasNEATPolicyBackend() || HasFixedNetworkPolicy(); }

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

	/** Explicit runtime state machine. Default: Idle after BeginPlay. Only manager advances this. */
	UPROPERTY(VisibleAnywhere, Category = "Racing|Debug")
	EAgentRuntimeState RuntimeState = EAgentRuntimeState::Idle;

	/** Throttled log flag: emitted once when a step is skipped due to missing authorization. Reset each episode. */
	UPROPERTY() bool bHasLoggedAuthSkipThisEpisode = false;

	UPROPERTY() TObjectPtr<USimpleNeuralNetwork> PolicyNetwork;
	UPROPERTY() TObjectPtr<UPolicyBackend> PolicyBackend;
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

	/** Progress along track (spline distance) or path length when no spline. cm. */
	UPROPERTY() float AccumulatedProgressCm = 0.f;
	/** Last distance along spline (cm) or last world position for path-length fallback */
	UPROPERTY() float LastProgressCm = 0.f;
	UPROPERTY() bool bHasLastProgress = false;
	UPROPERTY() FVector LastLocation = FVector::ZeroVector;
	/** Progress delta this step (set before ComputeReward so reward uses it). cm. */
	UPROPERTY() float ThisStepProgressDeltaCm = 0.f;
	/** Cached track spline for progress (resolved from world actor with tag "Track", IRoadSplineInterface). */
	UPROPERTY() TWeakObjectPtr<USplineComponent> CachedTrackSpline;

	/** Log policy-missing once per episode to avoid spam */
	UPROPERTY() bool bHasLoggedPolicyMissingThisEpisode = false;
	/** Log NEAT observation schema once per episode */
	UPROPERTY() bool bHasLoggedNEATSchemaThisEpisode = false;
	/** Log once when skipping step because NEAT mode but no backend (readiness failure) */
	UPROPERTY() bool bHasLoggedNEATNoBackendThisEpisode = false;

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

	/** Resolve track spline from world (actor tag "Track", IRoadSplineInterface). */
	void EnsureTrackSpline();
	/** Compute progress delta this step (spline or path length), update Last* state. Returns cm. */
	float GetProgressDeltaCmAndUpdate(const FVector& CurrentLoc);
	/** Wrap progress delta for closed spline (shortest signed delta). */
	static float WrapProgressDelta(float Delta, float SplineLen);
	void DrawObservationHUD();
	void DrawRayAnglesDebug();
};