// Copyright Epic Games, Inc. All Rights Reserved.

#include "StuntCarRacerWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UStuntCarRacerWheelRear::UStuntCarRacerWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}