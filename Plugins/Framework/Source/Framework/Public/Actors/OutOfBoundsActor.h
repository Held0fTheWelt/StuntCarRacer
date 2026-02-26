#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OutOfBoundsActor.generated.h"

class UBoxComponent;

UCLASS()
class FRAMEWORK_API AOutOfBoundsActor : public AActor
{
	GENERATED_BODY()

public:
	AOutOfBoundsActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutOfBounds")
	TObjectPtr<UBoxComponent> CollisionComponent = nullptr;

protected:
	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;
};
