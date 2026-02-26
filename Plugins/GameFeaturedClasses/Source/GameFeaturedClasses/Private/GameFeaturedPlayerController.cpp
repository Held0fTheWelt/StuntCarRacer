// Fill out your copyright notice in the Description page of Project Settings.


#include "GameFeaturedPlayerController.h"


#include "Components/GameFrameworkComponentManager.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(Log_GameFeatured_PlayerController);

void AGameFeaturedPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void AGameFeaturedPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add the game feature listener if it was not added yet
	AddGameFeatureListener();
}

void AGameFeaturedPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Remove the game feature listener if it was added
	RemoveGameFeatureListener();
}

#pragma region GameFeatures
bool AGameFeaturedPlayerController::AddGameFeatureListener()
{
	if (bGameFeaturesInitialized)
	{
		// Log that the game feature listener is not initialized
		UE_LOGFMT(Log_GameFeatured_PlayerController, VeryVerbose, "{Time}: {Line} {Class}: Game feature listener is already initialized for GameFeaturedGameStateBase: {Name}",
			("Class", GET_CLASSNAME_WITH_FUNCTION),
			("Line", GET_LINE_NUMBER),
			("Time", GET_CURRENT_TIME),
			("Name", GetFName().ToString()));
		return false;
	}

	bGameFeaturesInitialized = true;


	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
	// Log addition of the game feature listener
	UE_LOGFMT(Log_GameFeatured_PlayerController, Verbose, "{Time}: {Line} {Class}: Added game feature listener to GameFeaturedGameStateBase: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));
	return false;
}
bool AGameFeaturedPlayerController::RemoveGameFeatureListener()
{
	if (!bGameFeaturesInitialized)
	{
		// Log that the game feature listener is not initialized
		UE_LOGFMT(Log_GameFeatured_PlayerController, VeryVerbose, "{Time}: {Line} {Class}: Game feature listener is not initialized for GameFeaturedGameStateBase: {Name}",
			("Class", GET_CLASSNAME_WITH_FUNCTION),
			("Line", GET_LINE_NUMBER),
			("Time", GET_CURRENT_TIME),
			("Name", GetFName().ToString()));
		return false;
	}

	bGameFeaturesInitialized = false;

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	// Log removal of the game feature listener
	UE_LOGFMT(Log_GameFeatured_PlayerController, Verbose, "{Time}: {Line} {Class}: Removed game feature listener from GameFeaturedGameStateBase: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));
	return true;
}