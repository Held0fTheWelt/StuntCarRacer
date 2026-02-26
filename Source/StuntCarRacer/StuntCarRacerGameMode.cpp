// Copyright Epic Games, Inc. All Rights Reserved.

#include "StuntCarRacerGameMode.h"
#include "StuntCarRacerPlayerController.h"

AStuntCarRacerGameMode::AStuntCarRacerGameMode()
{
	PlayerControllerClass = AStuntCarRacerPlayerController::StaticClass();
}
