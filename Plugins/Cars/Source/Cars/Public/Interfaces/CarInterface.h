// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CarInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCarInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class CARS_API ICarInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	const UChaosWheeledVehicleMovementComponent* GetCarChaosVehicleMovement() const;

	/** Handles steering input */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void Steering(float SteeringValue);

	/** Handles throttle input */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void Throttle(float ThrottleValue);

	/** Handles brake input */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void Brake(float BrakeValue);

	/** Handles brake start/stop inputs */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void StartBrake();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void StopBrake();

	/** Handles handbrake start/stop inputs */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void StartHandbrake();
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void StopHandbrake();

	/** Forward speed (cm/s). If you only have km/h, convert. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Racing|State")
	float GetForwardSpeedCmPerSec() const;

	/** Angular velocity in deg/s (or rad/s if you prefer; stay consistent with your obs scaling). */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Racing|State")
	FVector GetAngularVelocityDegPerSec() const;

	/** True if vehicle is airborne (optional; return false if unknown). */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Racing|State")
	bool IsAirborne() const;
};
