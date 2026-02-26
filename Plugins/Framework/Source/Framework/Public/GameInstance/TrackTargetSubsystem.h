#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TrackTargetSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTargetTrackedSignature, AActor*, TrackedActor, int32, TargetIndex);

/**
 * GameInstance subsystem that dispatches "target tracked" events (e.g. track gates).
 * Track gates notify this subsystem, and interested systems (TimeTrial, UI, etc.)
 * can bind to OnTargetTrackedSignature.
 */
UCLASS()
class FRAMEWORK_API UTrackTargetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	/** Called by gates/targets when an actor has tracked a target index */
	UFUNCTION(BlueprintCallable, Category = "TrackTarget")
	void NotifyTargetTracking(AActor* TrackedActor, int32 TargetIndex);

public:
	/** Fired whenever a target was tracked */
	UPROPERTY(BlueprintAssignable, Category = "TrackTarget|Events")
	FTargetTrackedSignature OnTargetTrackedSignature;

	/** Optional: enable verbose debug logs */
	UPROPERTY(EditAnywhere, Category = "TrackTarget|Debug")
	bool bLogEvents = false;
};
