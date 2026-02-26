// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFeaturedGameStateBase.h"
#include "FrameworkGameStateBase.generated.h"

/**
 * 
 */
UCLASS()
class FRAMEWORK_API AFrameworkGameStateBase : public AGameFeaturedGameStateBase
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(Transient)
	FName TrackName;
};
