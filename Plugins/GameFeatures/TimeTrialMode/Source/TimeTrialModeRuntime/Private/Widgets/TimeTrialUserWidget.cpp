// Fill out your copyright notice in the Description page of Project Settings.

#include "Widgets/TimeTrialUserWidget.h"
#include "Widgets/CountdownWidget.h"

#include "GameFramework/PlayerController.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TimeTrialUserWidget, Log, All);

#define TTW_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TimeTrialUserWidget, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

void UTimeTrialUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TTW_LOGFMT(Display, "NativeConstruct");

	// Defensive: avoid multiple instances / duplicate bindings if reconstructed
	if (CountdownUIInstance)
	{
		TTW_LOGFMT(Verbose, "CountdownUIInstance already exists. Removing old instance.");
		CountdownUIInstance->OnCountdownFinished.RemoveAll(this);
		CountdownUIInstance->RemoveFromParent();
		CountdownUIInstance = nullptr;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		TTW_LOGFMT(Error, "GetOwningPlayer() is null. Cannot create countdown widget.");
		return;
	}

	if (!CountdownUIClass)
	{
		TTW_LOGFMT(Warning, "CountdownUIClass is null. Countdown will not be shown.");
		return;
	}

	CountdownUIInstance = CreateWidget<UCountdownWidget>(PC, CountdownUIClass);
	if (!CountdownUIInstance)
	{
		TTW_LOGFMT(Error, "Failed to create CountdownUIInstance.");
		return;
	}

	CountdownUIInstance->AddToViewport(0);

	// Bind safely (avoid duplicates)
	CountdownUIInstance->OnCountdownFinished.RemoveAll(this);
	CountdownUIInstance->OnCountdownFinished.AddDynamic(this, &UTimeTrialUserWidget::StartRace);

	CountdownUIInstance->StartCountdown();

	// Your original: bind delegates to BP implementable events.
	// This is redundant (you can call BP_ directly), but keeping behavior:
	OnStopUpdateLaps.RemoveAll(this);
	OnUpdateLap.RemoveAll(this);
	OnUpdateGateTime.RemoveAll(this);

	OnStopUpdateLaps.AddDynamic(this, &UTimeTrialUserWidget::BP_StopUpdateLaps);
	OnUpdateLap.AddDynamic(this, &UTimeTrialUserWidget::BP_UpdateLap);
	OnUpdateGateTime.AddDynamic(this, &UTimeTrialUserWidget::BP_UpdateGateTime);

	TTW_LOGFMT(Display, "Countdown started and delegates bound.");
}

void UTimeTrialUserWidget::NativeDestruct()
{
	TTW_LOGFMT(Display, "NativeDestruct");

	// Unbind internal delegates
	OnStopUpdateLaps.RemoveAll(this);
	OnUpdateLap.RemoveAll(this);
	OnUpdateGateTime.RemoveAll(this);

	// Cleanup countdown widget
	if (CountdownUIInstance)
	{
		CountdownUIInstance->OnCountdownFinished.RemoveAll(this);
		CountdownUIInstance->RemoveFromParent();
		CountdownUIInstance = nullptr;
	}

	Super::NativeDestruct();
}

void UTimeTrialUserWidget::StartRace()
{
	TTW_LOGFMT(Log, "StartRace triggered -> Broadcasting OnRaceStart");
	OnRaceStart.Broadcast();
}

void UTimeTrialUserWidget::UpdateLapCount(int32 Lap, float NewLapStartTime)
{
	LapStartTime = NewLapStartTime;

	const float LapTime = NewLapStartTime - LastLapTime;

	// First lap has no completed lap time yet
	if (Lap > 1)
	{
		if (BestLapTime < 0.0f || LapTime < BestLapTime)
		{
			BestLapTime = LapTime;
		}
	}
	else
	{
		BestLapTime = -1.0f;
	}

	CurrentLap = Lap;
	LastLapTime = NewLapStartTime;

	TTW_LOGFMT(Verbose, "UpdateLapCount: Lap={Lap}, LastLapTime={Last}, LapStart={Start}, Best={Best}",
		("Lap", CurrentLap),
		("Last", LastLapTime),
		("Start", LapStartTime),
		("Best", BestLapTime));

	// Route to BP (direct)
	BP_UpdateLap(CurrentLap);
	BP_UpdateLaps();

	// Also broadcast delegates (if something else listens)
	OnUpdateLap.Broadcast(CurrentLap);
}

void UTimeTrialUserWidget::UpdateLapTimes(AActor* TrackedActor, int32 Lap, int32 GateIndex, float Time)
{
	(void)TrackedActor;

	TTW_LOGFMT(VeryVerbose, "UpdateLapTimes: Lap={Lap}, GateIndex={Gate}, Time={Time}",
		("Lap", Lap),
		("Gate", GateIndex),
		("Time", Time));

	if (GateIndex == 0)
	{
		UpdateLapCount(Lap, Time);
	}
	else
	{
		const float GateTime = Time - LapStartTime;

		// Route to BP
		BP_UpdateGateTime(GateTime);

		// Also broadcast delegate
		OnUpdateGateTime.Broadcast(GateTime);
	}
}
