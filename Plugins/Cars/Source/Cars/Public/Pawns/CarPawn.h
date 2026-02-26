// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeturedWheeledVehiclePawn.h"
#include "Interfaces/CarInterface.h"
#include "Interfaces/EngineInterface.h"
#include "Interfaces/ResetInterface.h"
#include "Interfaces/GameActorInterface.h"
#include "CarPawn.generated.h"

class UChaosWheeledVehicleMovementComponent;

struct FInputActionValue;
/**
 * 
 */
UCLASS()
class CARS_API ACarPawn : public AGameFeturedWheeledVehiclePawn, public ICarInterface, public IResetInterface, public IGameActorInterface, public IEngineInterface
{
	GENERATED_BODY()
	
	/** Cast pointer to the Chaos Vehicle movement component */
	TObjectPtr<UChaosWheeledVehicleMovementComponent> ChaosVehicleMovement;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bEngineIsOn = true;

public:
	ACarPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

public:
	/** Update */
	virtual void Tick(float Delta) override;

protected:
	/** Called when the brake lights are turned on or off */
	//UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle")
	//void BrakeLights(bool bBraking);

private:
	/** Handles steering input */
	virtual void Steering_Implementation(float SteeringValue) override;

	/** Handles throttle input */
	virtual void Throttle_Implementation(float ThrottleValue) override;

	/** Handles brake input */
	virtual void Brake_Implementation(float BrakeValue) override;

	/** Handles brake start/stop inputs */
	virtual void StartBrake_Implementation() override;
	virtual void StopBrake_Implementation() override;

	/** Handles handbrake start/stop inputs */
	virtual void StartHandbrake_Implementation() override;
	virtual void StopHandbrake_Implementation() override;

	/** Resets the car to initial state */
	virtual void Reset_Implementation() override;

	/** Sets the engine on or off */
	virtual void SetEngineOn_Implementation(bool bIsOn) override;

	/** Forward speed (cm/s). If you only have km/h, convert. */
	virtual float GetForwardSpeedCmPerSec_Implementation() const override;

	/** Angular velocity in deg/s (or rad/s if you prefer; stay consistent with your obs scaling). */
	virtual FVector GetAngularVelocityDegPerSec_Implementation() const override;

	/** True if vehicle is airborne (optional; return false if unknown). */
	virtual bool IsAirborne_Implementation() const override;
public:
	/** Returns the cast Chaos Vehicle Movement subobject */
	FORCEINLINE const TObjectPtr<UChaosWheeledVehicleMovementComponent>& GetChaosVehicleMovement() const { return ChaosVehicleMovement; }
	virtual const UChaosWheeledVehicleMovementComponent* GetCarChaosVehicleMovement_Implementation() const override
	{
		return ChaosVehicleMovement;
	}
};
