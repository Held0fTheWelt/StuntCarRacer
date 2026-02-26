// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/CarControlComponentInterface.h"
#include "Interfaces/ControlComponentInterface.h"
#include "CarControlComponent.generated.h"

class UInputAction;
struct FInputActionValue;

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CARCONTROLLERRUNTIME_API UCarControlComponent : public UActorComponent, public ICarControlComponentInterface, public IControlComponentInterface
{
	GENERATED_BODY()

public:	
	UCarControlComponent();

protected:
	/** Steering Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SteeringAction;

	/** Throttle Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ThrottleAction;

	/** Brake Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* BrakeAction;

	/** Handbrake Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* HandbrakeAction;

protected:
	virtual void BeginPlay() override;

private:
	virtual void SetupControlComponent_Implementation(class UInputComponent* PlayerInputComponent) override;

	/** Handles steering input */
	void Steering(const FInputActionValue& Value);

	/** Handles throttle input */
	void Throttle(const FInputActionValue& Value);

	/** Handles brake input */
	void Brake(const FInputActionValue& Value);

	/** Handles brake start/stop inputs */
	void StartBrake(const FInputActionValue& Value);
	void StopBrake(const FInputActionValue& Value);

	/** Handles handbrake start/stop inputs */
	void StartHandbrake(const FInputActionValue& Value);
	void StopHandbrake(const FInputActionValue& Value);
};
