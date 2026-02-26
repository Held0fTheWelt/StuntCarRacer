// Fill out your copyright notice in the Description page of Project Settings.


#include "Wheels/FrontWheel_Steering_Default.h"

#include "UObject/ConstructorHelpers.h"

UFrontWheel_Steering_Default::UFrontWheel_Steering_Default()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}