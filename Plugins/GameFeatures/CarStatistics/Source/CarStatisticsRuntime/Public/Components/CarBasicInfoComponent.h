// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CarBasicInfoComponent.generated.h"

class USpeedAndGearWidget;
class UChaosWheeledVehicleMovementComponent;

/**
 * Component responsible for displaying basic car information (speed & gear)
 * on the local player's HUD.
 *
 * - Only active for locally controlled vehicles
 * - Creates and manages a SpeedAndGearWidget instance
 * - Updates UI every tick using CarInterface data
 */
UCLASS(ClassGroup = (Car), meta = (BlueprintSpawnableComponent))
class CARSTATISTICSRUNTIME_API UCarBasicInfoComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCarBasicInfoComponent();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;
	//~ End UActorComponent Interface

private:
	/** Widget class used to display speed and gear information */
	UPROPERTY(EditDefaultsOnly, Category = "Car|UI")
	TSubclassOf<USpeedAndGearWidget> SpeedAndGearWidgetClass;

	/** Runtime instance of the speed & gear widget */
	UPROPERTY(Transient)
	TObjectPtr<USpeedAndGearWidget> SpeedAndGearWidgetInstance = nullptr;
};
