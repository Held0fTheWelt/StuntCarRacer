// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/ControlComponentInterface.h"
#include "CarCameraControlComponent.generated.h"

class UInputAction;
struct FInputActionValue;

USTRUCT(BlueprintType)
struct FSpringArmAndCameraSettings
{
	GENERATED_BODY()

	FSpringArmAndCameraSettings() = default;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	FVector SpringArmRelativeLocation = FVector(0.f, 0.f, 75.f);

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	float TargetArmLength = 380.0f;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	FVector SocketOffset = FVector(0.f, 0.f, 72.f);

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	FVector TargetOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bEnableCameraRotationLag = true;

	UPROPERTY(EditAnywhere, Category = "SpringArm", meta = (EditCondition = "bEnableCameraRotationLag"))
	float CameraRotationLagSpeed = 10.0f;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bInheritPitch = false;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bInheritYaw = true;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bInheritRoll = false;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bDoCollisionTestSpringArm = true;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bUsePawnControlRotationSpringarm = false;

	UPROPERTY(EditAnywhere, Category = "SpringArm")
	bool bDrawDebugLagMarkers = false;

	UPROPERTY(EditAnywhere, Category = "Camera")
	bool bUsePawnControlRotationCamera = false;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float FieldOfView = 110.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float DefaultYawRealignInterpSpeed = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	bool bRealignCameraYaw = true;
};

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CARCAMERARUNTIME_API UCarCameraControlComponent : public UActorComponent, public IControlComponentInterface
{
	GENERATED_BODY()

public:	
	UCarCameraControlComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	/** Look Around Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAroundAction;

	/** Toggle Camera Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* ToggleCameraAction;


	/** Spring Arm and Camera Settings */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FSpringArmAndCameraSettings SpringArmAndCameraSettings;

protected:
	virtual void BeginPlay() override;

	bool CreateSpringArm(USceneComponent* Root);

	bool CreateCamera(USceneComponent* SprintArm);

private:
	UPROPERTY(Transient)
	class USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(Transient)
	class UCameraComponent* Camera = nullptr;
private:
	virtual void SetupControlComponent_Implementation(class UInputComponent* PlayerInputComponent) override;

	/** Handles look around input */
	void LookAround(const FInputActionValue& Value);

	/** Handles toggle camera input */
	void ToggleCamera(const FInputActionValue& Value);
};
