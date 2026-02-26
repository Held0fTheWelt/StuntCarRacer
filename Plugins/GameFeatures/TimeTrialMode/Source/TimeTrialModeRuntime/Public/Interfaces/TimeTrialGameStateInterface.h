// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TimeTrialGameStateInterface.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FTimesTargetTrackedSignature, AActor*, TrackedActor, int32, Round, int32, GateIndex, float, TimeAtGateInSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNewRecordTime, AActor*, TrackedActor, float, Time);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRaceHasStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRaceHasFinished);

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTimeTrialGameStateInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class TIMETRIALMODERUNTIME_API ITimeTrialGameStateInterface
{
	GENERATED_BODY()

public:
	/** Notify about the engine active state change */
	virtual FTimesTargetTrackedSignature& GetTimesTargetTrackedSignature() = 0;
	/** Notify about the second engine active state change */
	virtual FNewRecordTime& GetNewRecordTime() = 0;
	/** Notify that the race has started */
	virtual FRaceHasStarted& GetRaceHasStarted() = 0;
	/** Notify that the race has finished */
	virtual FRaceHasFinished& GetRaceHasFinished() = 0;
};
