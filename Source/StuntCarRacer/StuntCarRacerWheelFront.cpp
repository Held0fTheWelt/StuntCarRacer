// Copyright Epic Games, Inc. All Rights Reserved.

#include "StuntCarRacerWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UStuntCarRacerWheelFront::UStuntCarRacerWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}