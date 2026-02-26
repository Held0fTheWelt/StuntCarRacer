// Fill out your copyright notice in the Description page of Project Settings.

#include "Controller/FrameworkAIController.h"

#include "Interfaces/TrainNNInterface.h"

#include "GameFramework/Pawn.h"
#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_FrameworkAIController, Log, All);

#define FAC_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_FrameworkAIController, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

UActorComponent* AFrameworkAIController::FindTrainNNComponent(APawn* InPawn) const
{
	if (!InPawn)
	{
		return nullptr;
	}

	TArray<UActorComponent*> TrainNNComponents = InPawn->GetComponentsByInterface(UTrainNNInterface::StaticClass());
	if (TrainNNComponents.IsEmpty())
	{
		return nullptr;
	}

	// If there are multiple, prefer the first but log at Verbose for clarity
	if (TrainNNComponents.Num() > 1)
	{
		FAC_LOGFMT(Verbose, "Multiple TrainNNInterface components found on Pawn={Pawn}. Using first: {Component}",
			("Pawn", GetNameSafe(InPawn)),
			("Component", GetNameSafe(TrainNNComponents[0])));
	}

	return TrainNNComponents[0];
}

void AFrameworkAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		FAC_LOGFMT(Warning, "OnPossess called with null pawn.");
		return;
	}

	// Optional: if you want this only on server / authority
	// if (!HasAuthority()) { return; }

	UActorComponent* TrainComp = FindTrainNNComponent(InPawn);
	if (!TrainComp)
	{
		FAC_LOGFMT(Verbose, "Pawn={Pawn} has no TrainNNInterface component. Nothing to reset.",
			("Pawn", GetNameSafe(InPawn)));
		return;
	}

	// Safe interface call
	ITrainNNInterface::Execute_ResetEpisode(TrainComp);

	FAC_LOGFMT(Log, "ResetEpisode executed. Pawn={Pawn}, Component={Component}",
		("Pawn", GetNameSafe(InPawn)),
		("Component", GetNameSafe(TrainComp)));
}
