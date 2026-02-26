// Fill out your copyright notice in the Description page of Project Settings.


#include "Pawns/CarPawn.h"

#include "ChaosWheeledVehicleMovementComponent.h"

#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "Interfaces/ControlComponentInterface.h"

ACarPawn::ACarPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
}

void ACarPawn::BeginPlay()
{
	Super::BeginPlay();

	IEngineInterface::Execute_SetEngineOn(this, true);


	if (ChaosVehicleMovement)
	{
		ChaosVehicleMovement->SetUseAutomaticGears(true);
		ChaosVehicleMovement->SetTargetGear(1, true);
		ChaosVehicleMovement->SetBrakeInput(0.0f);
		ChaosVehicleMovement->SetHandbrakeInput(0.0f);
	}
}

void ACarPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	TArray<UActorComponent*> Components = GetComponentsByInterface(UControlComponentInterface::StaticClass());

	if(Components.Num() > 0)
	{
		for(UActorComponent* Component : Components)
		{
			IControlComponentInterface::Execute_SetupControlComponent(Component, PlayerInputComponent);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ACarPawn::SetupPlayerInputComponent: No ControlComponentInterface found on %s"), *GetName());
	}
}

void ACarPawn::Tick(float Delta)
{
	Super::Tick(Delta);
	//if (GetOwner()->GetInstigatorController() != Cast<AController>(GetWorld()->GetFirstPlayerController()))
	//{
	//	if (ChaosVehicleMovement)
	//	{
	//		UE_LOG(LogTemp, Warning,
	//			TEXT("ACarPawn Speed=%.1f cm/s  ThrottleInput=%.2f  BrakeInput=%.2f  Handbrake=%d  Gear=%d  RPM=%.0f"),
	//			ChaosVehicleMovement->GetForwardSpeed(),
	//			ChaosVehicleMovement->GetThrottleInput(),
	//			ChaosVehicleMovement->GetBrakeInput(),
	//			ChaosVehicleMovement->GetHandbrakeInput() ? 1 : 0,
	//			ChaosVehicleMovement->GetCurrentGear(),
	//			ChaosVehicleMovement->GetEngineRotationSpeed()
	//		);

	//	}
	//	else
	//	{
	//		UE_LOG(LogTemp, Error, TEXT("ChaosVehicleMovement is NULL"));
	//	}
	//}
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);
}

//void ACarPawn::BrakeLights(bool bBraking)
//{
//	//Super::Brak
//}

void ACarPawn::Steering_Implementation(float SteeringValue)
{
	// add the input
	ChaosVehicleMovement->SetSteeringInput(FMath::Clamp(SteeringValue, -1.f, 1.f));
}

void ACarPawn::Throttle_Implementation(float ThrottleValue)
{
	if (!bEngineIsOn)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("ACarPawn::Throttle_Implementation: Engine is off, ignoring throttle input."));
		return;
	}
	
	//if (GetOwner()->GetInstigatorController() != Cast<AController>(GetWorld()->GetFirstPlayerController()))
	//{
	//	UE_LOG(LogTemp, Warning, TEXT("ACarPawn::Throttle_Implementation: Not the player's car, throttle input: %f"), ThrottleValue);
	//}
	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);

}

void ACarPawn::Brake_Implementation(float BrakeValue)
{
	// add the input
	ChaosVehicleMovement->SetBrakeInput(BrakeValue);

}

void ACarPawn::StartBrake_Implementation()
{
	// call the Blueprint hook for the brake lights
	//BrakeLights(true);
}

void ACarPawn::StopBrake_Implementation()
{
	// call the Blueprint hook for the brake lights
	//BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ACarPawn::StartHandbrake_Implementation()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);

	// call the Blueprint hook for the break lights
	//BrakeLights(true);
}

void ACarPawn::StopHandbrake_Implementation()
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(false);

	// call the Blueprint hook for the break lights
	//BrakeLights(false);
}

void ACarPawn::Reset_Implementation()
{
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);

	// IMPORTANT: wake after teleport/zeroing
	GetMesh()->WakeAllRigidBodies();

	auto ResetComponentsOnActor = [&](AActor* A)
		{
			if (!A) return;

			TArray<UActorComponent*> Comps;
			A->GetComponents(Comps);

			for (UActorComponent* C : Comps)
			{
				if (C && C->GetClass()->ImplementsInterface(UResetInterface::StaticClass()))
				{
					IResetInterface::Execute_Reset(C);
				}
			}
		};

	// 1) Pawn components (GameFeature components sit here)
	ResetComponentsOnActor(this);

	// 2) Controller components (optional)
	ResetComponentsOnActor(GetController());
}

void ACarPawn::SetEngineOn_Implementation(bool bIsOn)
{
	UE_LOG(LogTemp, Verbose, TEXT("ACarPawn::SetEngineOn_Implementation: Setting engine %s"), bIsOn ? TEXT("ON") : TEXT("OFF"));
	bEngineIsOn = bIsOn;

	if(!bEngineIsOn)
	{
		// reset throttle input to zero
		ChaosVehicleMovement->SetThrottleInput(0.0f);
	}
}

float ACarPawn::GetForwardSpeedCmPerSec_Implementation() const
{
	if (!ChaosVehicleMovement)
	{
		return 0.0f;
	}

	// In Chaos Vehicles, forward speed is typically in cm/s (Unreal units),
	// but verify once with a log vs. HUD speed.
	return ChaosVehicleMovement->GetForwardSpeed();
}


FVector ACarPawn::GetAngularVelocityDegPerSec_Implementation() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return FVector::ZeroVector;
	}

	// degrees/sec
	return MeshComp->GetPhysicsAngularVelocityInDegrees();
}


bool ACarPawn::IsAirborne_Implementation() const
{
	if (!ChaosVehicleMovement)
	{
		return false;
	}

	// Option A (if available in your UE version / setup):
	// return ChaosVehicleMovement->IsInAir();

	// Option B: wheel contact (works well)
	const int32 NumWheels = ChaosVehicleMovement->GetNumWheels();
	for (int32 i = 0; i < NumWheels; ++i)
	{
		// Depending on UE version, wheel state access differs.
		// If you have GetWheelState(i):
		const auto WheelState = ChaosVehicleMovement->GetWheelState(i);
		if (WheelState.bInContact)
		{
			return false;
		}
	}
	return true;
}

