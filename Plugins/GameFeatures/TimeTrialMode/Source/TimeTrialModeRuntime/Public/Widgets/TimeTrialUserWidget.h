// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimeTrialUserWidget.generated.h"

class UCountdownWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStartRaceDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStopRaceDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdateLap, int32, CurrentLap);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FUpdateGateTime, float, CurrentGateTime);

/**
 * Time trial HUD widget.
 * - Spawns a countdown widget on construct
 * - Broadcasts race start when countdown finishes
 * - Tracks lap start / best lap
 * - Routes updates to Blueprint implementable events
 */
UCLASS(Abstract, editinlinenew, BlueprintType, Blueprintable, meta = (DontUseGenericSpawnObject = "True", DisableNativeTick))
class TIMETRIALMODERUNTIME_API UTimeTrialUserWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	/** Type of start countdown UI widget to spawn */
	UPROPERTY(EditAnywhere, Category = "Start Countdown")
	TSubclassOf<UCountdownWidget> CountdownUIClass;

	/** Countdown instance (kept to unbind / remove) */
	UPROPERTY(Transient)
	TObjectPtr<UCountdownWidget> CountdownUIInstance = nullptr;

	/** Time when the previous lap started, in seconds */
	UPROPERTY(Transient)
	float LastLapTime = 0.0f;

	/** Best lap time, in seconds. -1 means "invalid / not available yet". */
	UPROPERTY(Transient)
	float BestLapTime = -1.0f;

	/** Game time when this lap started */
	UPROPERTY(Transient)
	float LapStartTime = 0.0f;

	/** Current lap number */
	UPROPERTY(BlueprintReadOnly, Category = "Time Trial")
	int32 CurrentLap = 0;

public:
	/** Delegates to broadcast */
	UPROPERTY(BlueprintAssignable, Category = "Time Trial|Events")
	FStartRaceDelegate OnRaceStart;

	UPROPERTY(BlueprintAssignable, Category = "Time Trial|Events")
	FStopRaceDelegate OnStopUpdateLaps;

	UPROPERTY(BlueprintAssignable, Category = "Time Trial|Events")
	FUpdateLap OnUpdateLap;

	UPROPERTY(BlueprintAssignable, Category = "Time Trial|Events")
	FUpdateGateTime OnUpdateGateTime;

protected:
	/** Widget initialization */
	virtual void NativeConstruct() override;

	/** Widget shutdown */
	virtual void NativeDestruct() override;

public:
	/** Called by gameplay logic to update lap/gate timing */
	void UpdateLapTimes(class AActor* TrackedActor, int32 Lap, int32 GateIndex, float Time);

	/** Updates lap counters and best lap */
	void UpdateLapCount(int32 Lap, float NewLapStartTime);

	/** Allows Blueprint control to update the lap tracker widgets */
	UFUNCTION(BlueprintImplementableEvent, Category = "Time Trial", meta = (DisplayName = "Update Laps"))
	void BP_UpdateLaps();

	UFUNCTION(BlueprintImplementableEvent, Category = "Time Trial", meta = (DisplayName = "Stop Update Laps"))
	void BP_StopUpdateLaps();

	UFUNCTION(BlueprintImplementableEvent, Category = "Time Trial", meta = (DisplayName = "Update Lap"))
	void BP_UpdateLap(int32 InCurrentLap);

	UFUNCTION(BlueprintImplementableEvent, Category = "Time Trial", meta = (DisplayName = "Update Gate Time"))
	void BP_UpdateGateTime(float CurrentGateTime);

protected:
	/** Called from the countdown delegate to start the race */
	UFUNCTION()
	void StartRace();

	/** Gets the current lap number */
	UFUNCTION(BlueprintPure, Category = "Time Trial")
	int32 GetCurrentLap() const { return CurrentLap; }

	/** Gets the best lap time saved */
	UFUNCTION(BlueprintPure, Category = "Time Trial")
	float GetBestLapTime() const { return BestLapTime; }

	/** Gets the lap start time saved */
	UFUNCTION(BlueprintPure, Category = "Time Trial")
	float GetLapStartTime() const { return LapStartTime; }
};
