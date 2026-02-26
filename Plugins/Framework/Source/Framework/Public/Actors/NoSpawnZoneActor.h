#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NoSpawnZoneActor.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class ENoSpawnExitMode : uint8
{
	Backward UMETA(DisplayName = "Backward (spawn earlier)"),
	Forward  UMETA(DisplayName = "Forward (spawn later)")
};

UCLASS()
class FRAMEWORK_API ANoSpawnZoneActor : public AActor
{
	GENERATED_BODY()

public:
	ANoSpawnZoneActor();

	ENoSpawnExitMode GetExitMode() const { return ExitMode; }
	float GetPushDistanceCm() const { return PushDistanceCm; }
	float GetSafetyExtraCm() const { return SafetyExtraCm; }

	/** Respects rotation (OBB) and supports extra margin. */
	UFUNCTION(BlueprintCallable, Category = "NoSpawn")
	bool ContainsPointWithMargin(const FVector& WorldPoint, float ExtraCm) const;

	/** Backwards compatible helper (uses SafetyExtraCm). */
	UFUNCTION(BlueprintCallable, Category = "NoSpawn")
	bool ContainsPoint(const FVector& WorldPoint) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NoSpawn")
	TObjectPtr<UBoxComponent> Box;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NoSpawn")
	ENoSpawnExitMode ExitMode = ENoSpawnExitMode::Backward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NoSpawn")
	float PushDistanceCm = 1600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NoSpawn")
	float SafetyExtraCm = 100.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
