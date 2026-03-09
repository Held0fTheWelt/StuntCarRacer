// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Controller/FrameworkAIController.h"
#include "Interfaces/CarInterface.h"
#include "CarAIController.generated.h"

/**
 * AI Controller that implements CarInterface so that components (e.g. RacingAgentComponent)
 * attached to this controller can resolve the actual car via GetCarActor() and related methods.
 * When the controller possesses a pawn that implements CarInterface, all interface calls
 * delegate to the possessed pawn. Missing possessed pawn is handled safely (returns null / no-op).
 */
UCLASS()
class CARS_API ACarAIController : public AFrameworkAIController, public ICarInterface
{
	GENERATED_BODY()

public:
	// ICarInterface: delegate to possessed pawn when it implements the interface.
	virtual AActor* GetCarActor_Implementation() const override;
	virtual UPrimitiveComponent* GetCarRootPrimitive_Implementation() const override;
	virtual const UChaosWheeledVehicleMovementComponent* GetCarChaosVehicleMovement_Implementation() const override;

	virtual void Steering_Implementation(float SteeringValue) override;
	virtual void Throttle_Implementation(float ThrottleValue) override;
	virtual void Brake_Implementation(float BrakeValue) override;
	virtual void StartBrake_Implementation() override;
	virtual void StopBrake_Implementation() override;
	virtual void StartHandbrake_Implementation() override;
	virtual void StopHandbrake_Implementation() override;

	virtual float GetForwardSpeedCmPerSec_Implementation() const override;
	virtual FVector GetAngularVelocityDegPerSec_Implementation() const override;
	virtual bool IsAirborne_Implementation() const override;
};
