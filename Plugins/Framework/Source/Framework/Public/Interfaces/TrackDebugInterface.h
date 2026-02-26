// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TrackDebugInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UTrackDebugInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class FRAMEWORK_API ITrackDebugInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** Call this from Respawn subsystem or Agent whenever a spawn/respawn actually happens. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Curriculum|SpawnDebug")
	void ReportAgentSpawn(AActor* Agent, const FTransform& SpawnWorldTransform, FName Reason = NAME_None, float Score = -1.f);

};
