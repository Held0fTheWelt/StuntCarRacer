// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RoadSplineInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class URoadSplineInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class FRAMEWORK_API IRoadSplineInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Default)
	class USplineComponent* GetRoadSpline() const;
};
