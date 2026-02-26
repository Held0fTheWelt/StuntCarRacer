// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/TimeTrialPlayerComponent.h"

#include "Interfaces/TimeTrialGameStateInterface.h"
#include "Widgets/TimeTrialUserWidget.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TimeTrialPlayerComponent, Log, All);

#define TTPC_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TimeTrialPlayerComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

UTimeTrialPlayerComponent::UTimeTrialPlayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // no Tick needed
}

void UTimeTrialPlayerComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		TTPC_LOGFMT(Error, "BeginPlay: World is null.");
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		TTPC_LOGFMT(Error, "BeginPlay: Owner is null.");
		return;
	}

	// Only create UI / bind events for locally controlled pawn
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			TTPC_LOGFMT(Verbose, "Not locally controlled. Skipping UI + bindings.");
			return;
		}
	}
	else
	{
		// If not a pawn, we still try but log once
		TTPC_LOGFMT(Verbose, "Owner is not a Pawn. Local-control check skipped.");
	}

	// ---------------------------------------------------------------------
	// Create UI
	// ---------------------------------------------------------------------
	if (!TimeTrialWidgetClass)
	{
		TTPC_LOGFMT(Warning, "TimeTrialWidgetClass is null. UI will not be shown.");
	}
	else
	{
		APlayerController* PC = nullptr;

		if (APawn* PawnOwner = Cast<APawn>(Owner))
		{
			PC = Cast<APlayerController>(PawnOwner->GetController());
		}

		// Fallback: first player controller
		if (!PC)
		{
			PC = World->GetFirstPlayerController();
		}

		if (!PC)
		{
			TTPC_LOGFMT(Error, "No PlayerController available. Cannot create widget.");
		}
		else
		{
			TimeTrialWidget = CreateWidget<UTimeTrialUserWidget>(PC, TimeTrialWidgetClass);
			if (!TimeTrialWidget)
			{
				TTPC_LOGFMT(Error, "Failed to create TimeTrialWidget.");
			}
			else
			{
				TimeTrialWidget->AddToViewport();
				TTPC_LOGFMT(Verbose, "TimeTrialWidget created and added to viewport.");
			}
		}
	}

	// ---------------------------------------------------------------------
	// Bind to GameState component implementing ITimeTrialGameStateInterface
	// ---------------------------------------------------------------------
	AGameStateBase* GameState = World->GetGameState();
	if (!GameState)
	{
		TTPC_LOGFMT(Warning, "GameState is null. Cannot bind TimeTrial interface.");
		return;
	}

	const TArray<UActorComponent*> Components =
		GameState->GetComponentsByInterface(UTimeTrialGameStateInterface::StaticClass());

	if (Components.IsEmpty())
	{
		TTPC_LOGFMT(Warning, "No component implementing TimeTrialGameStateInterface found on GameState.");
		return;
	}

	CachedTimeTrialGameStateComponent = Components[0];
	if (!CachedTimeTrialGameStateComponent)
	{
		TTPC_LOGFMT(Error, "CachedTimeTrialGameStateComponent is null after lookup.");
		return;
	}

	ITimeTrialGameStateInterface* TTInterface = Cast<ITimeTrialGameStateInterface>(CachedTimeTrialGameStateComponent);
	if (!TTInterface)
	{
		TTPC_LOGFMT(Error, "Component does not cast to ITimeTrialGameStateInterface.");
		return;
	}

	// Defensive: avoid duplicate bindings if BeginPlay runs again (PIE / respawn edge cases)
	TTInterface->GetTimesTargetTrackedSignature().RemoveAll(this);
	TTInterface->GetNewRecordTime().RemoveAll(this);
	TTInterface->GetRaceHasStarted().RemoveAll(this);
	TTInterface->GetRaceHasFinished().RemoveAll(this);

	TTInterface->GetTimesTargetTrackedSignature().AddDynamic(this, &UTimeTrialPlayerComponent::UpdateLapTimes);
	TTInterface->GetNewRecordTime().AddDynamic(this, &UTimeTrialPlayerComponent::UpdateNewRecordTime);
	TTInterface->GetRaceHasStarted().AddDynamic(this, &UTimeTrialPlayerComponent::OnRaceStarted);
	TTInterface->GetRaceHasFinished().AddDynamic(this, &UTimeTrialPlayerComponent::RaceHasFinished);

	TTPC_LOGFMT(Verbose, "Bound to TimeTrialGameStateInterface events.");
}

void UTimeTrialPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind from interface
	if (CachedTimeTrialGameStateComponent)
	{
		if (ITimeTrialGameStateInterface* TTInterface = Cast<ITimeTrialGameStateInterface>(CachedTimeTrialGameStateComponent))
		{
			TTInterface->GetTimesTargetTrackedSignature().RemoveAll(this);
			TTInterface->GetNewRecordTime().RemoveAll(this);
			TTInterface->GetRaceHasStarted().RemoveAll(this);
			TTInterface->GetRaceHasFinished().RemoveAll(this);
		}
	}

	// Remove UI
	if (TimeTrialWidget)
	{
		TimeTrialWidget->RemoveFromParent();
		TimeTrialWidget = nullptr;
	}

	CachedTimeTrialGameStateComponent = nullptr;

	TTPC_LOGFMT(Display, "EndPlay cleanup complete.");

	Super::EndPlay(EndPlayReason);
}

void UTimeTrialPlayerComponent::UpdateLapTimes(AActor* TrackedActor, int32 Lap, int32 GateIndex, float Time)
{
	TTPC_LOGFMT(VeryVerbose, "UpdateLapTimes: Actor={Actor}, Lap={Lap}, Gate={Gate}, Time={Time}",
		("Actor", GetNameSafe(TrackedActor)),
		("Lap", Lap),
		("Gate", GateIndex),
		("Time", Time));

	if (!TimeTrialWidget)
	{
		return;
	}

	TimeTrialWidget->UpdateLapTimes(TrackedActor, Lap, GateIndex, Time);
}

void UTimeTrialPlayerComponent::UpdateNewRecordTime(AActor* TrackedActor, float Time)
{
	TTPC_LOGFMT(Display, "New record: Actor={Actor}, Time={Time}s",
		("Actor", GetNameSafe(TrackedActor)),
		("Time", Time));

	// Optional: route to widget later if you add UI for it.
}

void UTimeTrialPlayerComponent::OnRaceStarted()
{
	TTPC_LOGFMT(Display, "OnRaceStarted");

	if (bHasRaceStarted)
	{
		return;
	}

	bHasRaceStarted = true;

	if (!TimeTrialWidget)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		TimeTrialWidget->UpdateLapCount(1, World->GetTimeSeconds());
	}
}

void UTimeTrialPlayerComponent::RaceHasFinished()
{
	TTPC_LOGFMT(Display, "RaceHasFinished");

	if (TimeTrialWidget)
	{
		TimeTrialWidget->OnStopUpdateLaps.Broadcast();
	}
}
