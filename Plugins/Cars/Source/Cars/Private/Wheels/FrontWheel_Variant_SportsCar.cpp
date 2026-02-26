// Fill out your copyright notice in the Description page of Project Settings.


#include "Wheels/FrontWheel_Variant_SportsCar.h"


UFrontWheel_Variant_SportsCar::UFrontWheel_Variant_SportsCar()
{
	WheelRadius = 39.0f;
	WheelWidth = 35.0f;
	FrictionForceMultiplier = 3.0f;

	MaxBrakeTorque = 4500.0f;
	MaxHandBrakeTorque = 6000.0f;
}