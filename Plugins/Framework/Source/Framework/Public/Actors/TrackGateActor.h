#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrackGateActor.generated.h"

class UBoxComponent;
class USceneComponent;

UCLASS()
class FRAMEWORK_API ATrackGateActor : public AActor
{
	GENERATED_BODY()

public:
	ATrackGateActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnGateBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TrackGate")
	int32 GateIndex = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> CollisionBox = nullptr;
};
