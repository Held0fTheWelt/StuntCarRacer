// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controller/CarAIController.h"
#include "Interfaces/CarInterface.h"
#include "GameFramework/Pawn.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"

AActor* ACarAIController::GetCarActor_Implementation() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}
	if (ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_GetCarActor(ControlledPawn);
	}
	return ControlledPawn;
}

UPrimitiveComponent* ACarAIController::GetCarRootPrimitive_Implementation() const
{
	AActor* Car = GetCarActor_Implementation();
	if (!Car)
	{
		return nullptr;
	}
	if (Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_GetCarRootPrimitive(Car);
	}
	return Cast<UPrimitiveComponent>(Car->GetRootComponent());
}

const UChaosWheeledVehicleMovementComponent* ACarAIController::GetCarChaosVehicleMovement_Implementation() const
{
	AActor* Car = GetCarActor_Implementation();
	if (!Car)
	{
		return nullptr;
	}
	if (Car->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_GetCarChaosVehicleMovement(Car);
	}
	return Car->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
}

void ACarAIController::Steering_Implementation(float SteeringValue)
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_Steering(ControlledPawn, SteeringValue);
	}
}

void ACarAIController::Throttle_Implementation(float ThrottleValue)
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_Throttle(ControlledPawn, ThrottleValue);
	}
}

void ACarAIController::Brake_Implementation(float BrakeValue)
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_Brake(ControlledPawn, BrakeValue);
	}
}

void ACarAIController::StartBrake_Implementation()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_StartBrake(ControlledPawn);
	}
}

void ACarAIController::StopBrake_Implementation()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_StopBrake(ControlledPawn);
	}
}

void ACarAIController::StartHandbrake_Implementation()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_StartHandbrake(ControlledPawn);
	}
}

void ACarAIController::StopHandbrake_Implementation()
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		ICarInterface::Execute_StopHandbrake(ControlledPawn);
	}
}

float ACarAIController::GetForwardSpeedCmPerSec_Implementation() const
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_GetForwardSpeedCmPerSec(ControlledPawn);
	}
	return 0.f;
}

FVector ACarAIController::GetAngularVelocityDegPerSec_Implementation() const
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_GetAngularVelocityDegPerSec(ControlledPawn);
	}
	return FVector::ZeroVector;
}

bool ACarAIController::IsAirborne_Implementation() const
{
	APawn* ControlledPawn = GetPawn();
	if (ControlledPawn && ControlledPawn->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		return ICarInterface::Execute_IsAirborne(ControlledPawn);
	}
	return false;
}
