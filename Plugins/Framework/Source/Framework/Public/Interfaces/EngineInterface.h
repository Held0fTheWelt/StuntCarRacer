// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EngineInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UEngineInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class FRAMEWORK_API IEngineInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void SetEngineOn(bool bIsOn);
};
