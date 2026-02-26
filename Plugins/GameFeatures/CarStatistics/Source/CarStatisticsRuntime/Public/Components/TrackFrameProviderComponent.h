#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrackFrameProviderComponent.generated.h"

class USplineComponent;

/**
 * Track frame result.
 */
USTRUCT(BlueprintType)
struct FTrackFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) float DistanceAlongSpline = 0.0f;
	UPROPERTY(BlueprintReadOnly) FVector ClosestPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly) FVector Tangent = FVector::ForwardVector;
	UPROPERTY(BlueprintReadOnly) FVector Right = FVector::RightVector;
	UPROPERTY(BlueprintReadOnly) FVector Normal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly) float LateralError = 0.0f;           // cm
	UPROPERTY(BlueprintReadOnly) float HeadingError = 0.0f;           // rad
	UPROPERTY(BlueprintReadOnly) float DistanceToClosestPoint = 0.0f; // cm

	UPROPERTY(BlueprintReadOnly) float ProgressDelta = 0.0f;          // cm (signed)
};

UCLASS(ClassGroup = (Track), meta = (BlueprintSpawnableComponent))
class CARSTATISTICSRUNTIME_API UTrackFrameProviderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTrackFrameProviderComponent();

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent Interface

	/** Recompute cached spline length (safe if spline is null). */
	UFUNCTION(BlueprintCallable, Category = "Track|Spline")
	void RefreshSplineLength();

	/** Force reset progress tracking (useful after teleport/respawn). */
	UFUNCTION(BlueprintCallable, Category = "Track|Progress")
	void ResetProgressTracking(float InitialDistanceAlongSpline = 0.0f);

	/**
	 * Compute track frame at closest point to a world location.
	 * If bUpdateProgressTracking=false, ProgressDelta will be 0 and internal state won't change.
	 */
	UFUNCTION(BlueprintCallable, Category = "Track|Frame")
	FTrackFrame ComputeFrameAtLocation(
		const FVector& WorldLocation,
		const FVector& ForwardVector,
		bool bUpdateProgressTracking = true
	);

	/** Compute track frame at an explicit distance along spline. Does NOT update progress tracking. */
	UFUNCTION(BlueprintCallable, Category = "Track|Frame")
	FTrackFrame ComputeFrameAtDistance(float DistanceAlongSpline, const FVector& ForwardVector);

	/** Sample lookahead frames at offsets from a base distance along spline. */
	UFUNCTION(BlueprintCallable, Category = "Track|Lookahead")
	bool SampleLookaheadByDistance(
		float BaseDistanceAlongSpline,
		const TArray<float>& OffsetsCm,
		TArray<FTrackFrame>& OutFrames,
		bool bClampToSplineLength = false
	);

	/** Sample lookahead frames at offsets from the closest distance to a world location. */
	UFUNCTION(BlueprintCallable, Category = "Track|Lookahead")
	bool SampleLookaheadByWorldLocation(
		const FVector& WorldLocation,
		const FVector& ForwardVector,
		const TArray<float>& OffsetsCm,
		TArray<FTrackFrame>& OutFrames,
		bool bClampToSplineLength = false
	);
	void UpdateProgressAtLocation(const FVector& WorldLocation);
	float ConsumeProgressDeltaCm();
protected:
	/** Find and cache the road spline (and provider). */
	bool ResolveRoadSpline();

	/** Closest distance along spline for a world location. */
	float FindClosestDistanceAlongSpline(const FVector& WorldLocation) const;

	/** Wrap delta for continuous progress on closed splines. */
	static float WrapProgressDelta(float Delta, float SplineLen);

	/** Signed angle between vectors around an axis, in radians. */
	static float SignedAngleRadAroundAxis(const FVector& From, const FVector& To, const FVector& Axis);

protected:
	/** Explicit override for road spline provider (must implement RoadSplineInterface). */
	UPROPERTY(EditAnywhere, Category = "Track|Spline")
	TObjectPtr<AActor> RoadSplineProviderActor = nullptr;

	/** Auto-find nearest provider implementing RoadSplineInterface if none is set. */
	UPROPERTY(EditAnywhere, Category = "Track|Spline")
	bool bAutoFindRoadSplineProvider = true;

	/** Optional tag filter for auto-find. */
	UPROPERTY(EditAnywhere, Category = "Track|Spline")
	FName RequiredProviderTag;

	/** If distance to closest point exceeds this (cm), clear cache and re-resolve. <=0 disables. */
	UPROPERTY(EditAnywhere, Category = "Track|Spline")
	float ReResolveIfDistanceAboveCm = 0.0f;

	/** Draw debug basis axes etc. */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDrawDebug = false;
	UPROPERTY() 
	float PendingProgressDelta;
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugAxisLength = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugLifeTime = 0.05f;

	/** Debug log throttling interval (seconds). */
	UPROPERTY(EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugLogIntervalSeconds = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebug = false;
protected:
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> CachedSpline = nullptr;

	UPROPERTY(Transient)
	float CachedSplineLength = 0.0f;

	UPROPERTY(Transient)
	float LastDistanceAlongSpline = 0.0f;

	UPROPERTY(Transient)
	bool bHasLastDistance = false;

	UPROPERTY(Transient)
	double LastLogTimeSeconds = -1.0;
};
