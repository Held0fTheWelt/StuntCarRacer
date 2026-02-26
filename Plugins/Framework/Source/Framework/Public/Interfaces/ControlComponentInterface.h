// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ControlComponentInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UControlComponentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class FRAMEWORK_API IControlComponentInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	void SetupControlComponent(class UInputComponent* PlayerInputComponent);
};
