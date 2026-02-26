#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RespawnGameInstanceSubsystem.generated.h"

class USplineComponent;
class ANoSpawnZoneActor;

/**
 * GameInstance subsystem that schedules respawns for actors that go out of bounds.
 * - Disables engine (if supported)
 * - After a delay, moves the actor to the closest point on the track spline
 * - Calls Reset (if supported)
 * - Re-enables engine (if supported)
 *
 * Track spline discovery:
 * - Finds an actor tagged "Track" that implements URoadSplineInterface
 * - Caches it (weak) and refreshes if invalid
 */
UCLASS()
class FRAMEWORK_API URespawnGameInstanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void NotifyRespawn(AActor* ActorToRespawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "NoSpawn")
	void RegisterNoSpawnZone(AActor* ZoneActor);
	void RegisterNoSpawnZone_Implementation(AActor* ZoneActor);


	UFUNCTION(BlueprintImplementableEvent, Category = "NoSpawn")
	void UnRegisterNoSpawnZone(AActor* ZoneActor);
	void UnRegisterNoSpawnZone_Implementation(AActor* ZoneActor);

	UFUNCTION(BlueprintCallable, Category = "Respawn")
	bool FindSafeDistanceOnTrackSpline(
		USplineComponent* Spline,
		float StartDistanceCm,
		AActor* Actor,
		float& OutSafeDistanceCm
	);

	/** Returns a safe transform on the track near QueryWorldLocation that is outside any registered NoSpawnZones. */
	bool FindSafeTrackTransform(
		const FVector& QueryWorldLocation,
		float HeightOffsetCm,
		FTransform& OutTransform,
		float SearchStepCm = 400.f,
		int32 MaxSearchSteps = 24,
		float SafetyExtraCm = 100.f
	);

protected:
	void AddUISupportForRespawn();

private:
	void DoRespawn(TWeakObjectPtr<AActor> ActorToRespawn, FVector ActorLocation);

	ANoSpawnZoneActor* FindBlockingNoSpawnZone(const FVector& WorldPoint, float ActorRadiusCm);

private:
	/** Respawn delay in seconds */
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebug = false;

	UPROPERTY(EditAnywhere, Category = "Respawn")
	float RespawnDelaySeconds = 5.0f;

	/** Z-offset added to track location when respawning */
	UPROPERTY(EditAnywhere, Category = "Respawn")
	float RespawnHeightOffsetCm = 200.0f;

	/** Actors currently scheduled for respawn (weak to avoid preventing GC) */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> ActorsToRespawn;

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> NoSpawnZoneActors;

	/** Timer handles per actor so we can cancel/replace safely */
	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> RespawnTimers;

	/** Cached track spline provider actor (weak) */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedTrackProviderActor;
};
