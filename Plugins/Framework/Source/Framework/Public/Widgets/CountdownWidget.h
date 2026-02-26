// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CountdownWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCountdownFinishedDelegate);

/**
 * 
 */
UCLASS(Abstract, editinlinenew, BlueprintType, Blueprintable, meta = (DontUseGenericSpawnObject = "True", DisableNativeTick))
class FRAMEWORK_API UCountdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Starts the race countdown */
	void StartCountdown();

protected:

	/** Passes control to Blueprint to animate the race countdown. FinishCountdown should be called to start the race when it's done. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Countdown", meta = (DisplayName = "Start Countdown"))
	void BP_StartCountdown();

	/** Finishes the countdown and starts the race. */
	UFUNCTION(BlueprintCallable, Category = "Countdown")
	void FinishCountdown();

public:

	FCountdownFinishedDelegate OnCountdownFinished;
};
