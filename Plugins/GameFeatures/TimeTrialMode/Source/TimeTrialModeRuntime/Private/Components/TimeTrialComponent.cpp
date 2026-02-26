// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/TimeTrialComponent.h"

#include "GameInstance/TrackTargetSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TimeTrialComponent, Log, All);

#define TTC_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TimeTrialComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

/**
 * Adds a gate if it is the expected next one.
 * Expectation: GateIndex == Round.Gates.Num() because Gate 0 is added at StartRound.
 */
static bool AddGateIfNext(FRoundInformation& Round, int32 GateIndex, float TimeSec)
{
	if (GateIndex != Round.Gates.Num())
	{
		return false;
	}

	FGateInformation G;
	G.GateIndex = GateIndex;
	G.TimeAtGateInSeconds = TimeSec;
	Round.Gates.Add(G);

	return true;
}

UTimeTrialComponent::UTimeTrialComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTimeTrialComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		TTC_LOGFMT(Error, "BeginPlay: World is null.");
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		TTC_LOGFMT(Error, "BeginPlay: GameInstance is null.");
		return;
	}

	UTrackTargetSubsystem* TrackTargetSubsystem = GameInstance->GetSubsystem<UTrackTargetSubsystem>();
	if (!TrackTargetSubsystem)
	{
		TTC_LOGFMT(Error, "BeginPlay: TrackTargetSubsystem is null.");
		return;
	}

	// Bind once (defensive)
	TrackTargetSubsystem->OnTargetTrackedSignature.RemoveDynamic(this, &UTimeTrialComponent::OnTargetTracked);
	TrackTargetSubsystem->OnTargetTrackedSignature.AddDynamic(this, &UTimeTrialComponent::OnTargetTracked);

	// Count gates
	TArray<AActor*> FoundGates;
	UGameplayStatics::GetAllActorsWithTag(World, FName("TrackGate"), FoundGates);
	NumberOfGates = FoundGates.Num();

	if(bDebug)
		TTC_LOGFMT(Display, "TimeTrialComponent started. Gates={Gates}, Rounds={Rounds}",
			("Gates", NumberOfGates),
			("Rounds", NumberOfRounds));
}

void UTimeTrialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UTrackTargetSubsystem* TrackTargetSubsystem = GameInstance->GetSubsystem<UTrackTargetSubsystem>())
			{
				TrackTargetSubsystem->OnTargetTrackedSignature.RemoveDynamic(this, &UTimeTrialComponent::OnTargetTracked);
			}
		}
	}

	if (bDebug)
		TTC_LOGFMT(Display, "TimeTrialComponent ended.");

	Super::EndPlay(EndPlayReason);
}

void UTimeTrialComponent::OnTargetTracked(AActor* TrackedActor, int32 TargetIndex)
{
	if (!TrackedActor || TargetIndex < 0)
	{
		return;
	}

	// If we have N gates in the level, we treat "finish" as GateIndex N
	// even though the trigger index comes again as 0.
	const int32 FinishGateIndex = NumberOfGates;

	// Find or create entry for this actor
	int32 TimesIndex = TrackedTargets.IndexOfByKey(TrackedActor);
	if (TimesIndex == INDEX_NONE)
	{
		// New actor may only start at TargetIndex 0
		if (TargetIndex != 0)
		{
			return;
		}

		TrackedTargets.Add(TrackedActor);
		TimesIndex = TrackedTargetTimes.AddDefaulted();

		StartRound(TimesIndex);
		return;
	}

	// No rounds yet -> only start on 0
	if (TrackedTargetTimes[TimesIndex].Rounds.Num() == 0)
	{
		if (TargetIndex == 0)
		{
			StartRound(TimesIndex);
		}
		return;
	}

	FTrackTimeInformation& Times = TrackedTargetTimes[TimesIndex];
	FRoundInformation& Round = Times.Rounds.Last();

	// ------------------------------------------------------------------
	// Case A: TargetIndex == 0 -> Start OR Finish
	// ------------------------------------------------------------------
	if (TargetIndex == 0)
	{
		// If round is fresh (only gate 0 exists), ignore repeated start triggering
		if (Round.Gates.Num() <= 1)
		{
			return;
		}

		// Finish is valid only if we collected all intermediate gates.
		// Expected gates: 0..FinishGateIndex-1  -> Round.Gates.Num() == FinishGateIndex
		// because Gate 0 is already included.
		if (Round.Gates.Num() == FinishGateIndex)
		{
			FinishRound(TimesIndex, FinishGateIndex);

			// Finished all rounds?
			if (Times.Round >= NumberOfRounds)
			{
				if (bDebug)
					TTC_LOGFMT(Display, "Target finished all rounds. Target={Target}, Rounds={Rounds}",
						("Target", GetNameSafe(TrackedActor)),
						("Rounds", NumberOfRounds));

				OnRaceHasFinished.Broadcast();
			}
			else
			{
				StartRound(TimesIndex);
			}
		}
		else
		{
			// Too early / broken order -> report last known gate time
			const int32 LastGateIdx = Round.Gates.Num() - 1;
			const int32 LastGate = Round.Gates.IsValidIndex(LastGateIdx) ? Round.Gates[LastGateIdx].GateIndex : -1;
			const float LastTime = Round.Gates.IsValidIndex(LastGateIdx) ? Round.Gates[LastGateIdx].TimeAtGateInSeconds : 0.0f;

			if (bDebug)
				TTC_LOGFMT(Warning, "Finish too early: have {Have} gates, expected {Expected}. LastGate={LastGate}",
					("Have", Round.Gates.Num()),
					("Expected", FinishGateIndex),
					("LastGate", LastGate));

			OnTimesTargetTracked.Broadcast(GetOwner(), Times.Round, LastGate, LastTime);
		}

		return;
	}

	// ------------------------------------------------------------------
	// Case B: Intermediate gates 1..FinishGateIndex-1
	// ------------------------------------------------------------------
	if (TargetIndex > 0 && TargetIndex < FinishGateIndex)
	{
		UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.0f;

		const bool bOk = AddGateIfNext(Round, TargetIndex, Now);
		if (!bOk)
		{
			if (bDebug)
				TTC_LOGFMT(Warning, "Out-of-order gate: got {Got}, expected {Expected}",
					("Got", TargetIndex),
					("Expected", Round.Gates.Num()));
		}

		OnTimesTargetTracked.Broadcast(GetOwner(), Times.Round, TargetIndex, Now);
		return;
	}

	// ------------------------------------------------------------------
	// Everything else is invalid
	// ------------------------------------------------------------------
	TTC_LOGFMT(Warning, "Invalid TargetIndex={Idx} (FinishGateIndex={Finish})",
		("Idx", TargetIndex),
		("Finish", FinishGateIndex));
}

void UTimeTrialComponent::StartRound(int32 TimesIndex)
{
	UWorld* World = GetWorld();
	if (!World || !TrackedTargetTimes.IsValidIndex(TimesIndex))
	{
		TTC_LOGFMT(Error, "StartRound: invalid state. WorldValid={WorldValid}, TimesIndex={TimesIndex}",
			("WorldValid", World != nullptr),
			("TimesIndex", TimesIndex));
		return;
	}

	if (!bRaceHasStarted)
	{
		bRaceHasStarted = true;
		OnRaceHasStarted.Broadcast();
		TTC_LOGFMT(Display, "Race has started by TimesIndex={TimesIndex}", ("TimesIndex", TimesIndex));
	}

	FTrackTimeInformation& Times = TrackedTargetTimes[TimesIndex];

	Times.Round++;
	Times.Rounds.AddDefaulted();

	FRoundInformation& Round = Times.Rounds.Last();
	Round.StartTimeInSeconds = World->GetTimeSeconds();

	// Gate 0 = Start
	FGateInformation G0;
	G0.GateIndex = 0;
	G0.TimeAtGateInSeconds = Round.StartTimeInSeconds;
	Round.Gates.Add(G0);

	if (bDebug)
		TTC_LOGFMT(Display, "Round started. TimesIndex={TimesIndex}, Round={Round}, Start={Start}",
			("TimesIndex", TimesIndex),
			("Round", Times.Round),
			("Start", Round.StartTimeInSeconds));

	OnTimesTargetTracked.Broadcast(GetOwner(), Times.Round, 0, Round.StartTimeInSeconds);
}

void UTimeTrialComponent::FinishRound(int32 TimesIndex, int32 FinishGateIndex)
{
	UWorld* World = GetWorld();
	if (!World || !TrackedTargetTimes.IsValidIndex(TimesIndex))
	{
		TTC_LOGFMT(Error, "FinishRound: invalid state. WorldValid={WorldValid}, TimesIndex={TimesIndex}",
			("WorldValid", World != nullptr),
			("TimesIndex", TimesIndex));
		return;
	}

	FTrackTimeInformation& Times = TrackedTargetTimes[TimesIndex];
	if (Times.Rounds.Num() == 0)
	{
		TTC_LOGFMT(Error, "FinishRound called but no rounds exist. TimesIndex={TimesIndex}", ("TimesIndex", TimesIndex));
		return;
	}

	FRoundInformation& Round = Times.Rounds.Last();

	Round.EndTimeInSeconds = World->GetTimeSeconds();

	// Add finish as Gate N even though trigger index was 0
	const bool bAddedFinish = AddGateIfNext(Round, FinishGateIndex, Round.EndTimeInSeconds);
	if (!bAddedFinish)
	{
		TTC_LOGFMT(Warning, "Finish gate could not be added (order mismatch). FinishGateIndex={Finish}, GatesNum={Num}",
			("Finish", FinishGateIndex),
			("Num", Round.Gates.Num()));
	}

	const float RoundTime = Round.EndTimeInSeconds - Round.StartTimeInSeconds;
	Times.LastTimeInSeconds = RoundTime;

	if (bDebug)
		TTC_LOGFMT(Display, "Round finished. TimesIndex={TimesIndex}, Round={Round}, Time={Time}s",
			("TimesIndex", TimesIndex),
			("Round", Times.Round),
			("Time", RoundTime));

	OnTimesTargetTracked.Broadcast(GetOwner(), Times.Round, FinishGateIndex, Round.EndTimeInSeconds);

	// Best time
	if (Times.BestTimeInSeconds <= 0.0f || RoundTime < Times.BestTimeInSeconds)
	{
		Times.BestTimeInSeconds = RoundTime;
		if (bDebug)
			TTC_LOGFMT(Display, "New best time. TimesIndex={TimesIndex}, Best={Best}s",
				("TimesIndex", TimesIndex),
				("Best", Times.BestTimeInSeconds));

		OnNewRecordTime.Broadcast(GetOwner(), RoundTime);
	}
}
