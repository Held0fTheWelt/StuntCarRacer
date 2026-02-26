// Fill out your copyright notice in the Description page of Project Settings.


#include "Wheels/RearWheel_Engine_And_Handbrake.h"

#include "UObject/ConstructorHelpers.h"

URearWheel_Engine_And_Handbrake::URearWheel_Engine_And_Handbrake()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}