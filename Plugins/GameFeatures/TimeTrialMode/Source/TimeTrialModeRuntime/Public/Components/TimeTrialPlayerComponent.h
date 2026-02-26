// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeTrialPlayerComponent.generated.h"

class UTimeTrialUserWidget;
class UInputComponent;

/**
 * Player-side component that spawns the time trial UI and subscribes to the
 * game-state time trial interface for lap/gate updates.
 *
 * Intended to exist on the locally controlled pawn / player actor.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TIMETRIALMODERUNTIME_API UTimeTrialPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeTrialPlayerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeTrial|UI")
	TSubclassOf<UTimeTrialUserWidget> TimeTrialWidgetClass;

private:
	UFUNCTION()
	void UpdateLapTimes(AActor* TrackedActor, int32 Lap, int32 GateIndex, float Time);

	UFUNCTION()
	void UpdateNewRecordTime(AActor* TrackedActor, float Time);

	UFUNCTION()
	void OnRaceStarted();

	UFUNCTION()
	void RaceHasFinished();

private:
	UPROPERTY(Transient)
	bool bHasRaceStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UTimeTrialUserWidget> TimeTrialWidget = nullptr;

	/** The component on GameState that implements ITimeTrialGameStateInterface */
	UPROPERTY(Transient)
	TObjectPtr<UActorComponent> CachedTimeTrialGameStateComponent = nullptr;
};
