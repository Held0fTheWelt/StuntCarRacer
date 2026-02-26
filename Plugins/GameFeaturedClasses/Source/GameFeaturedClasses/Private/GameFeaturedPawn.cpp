// Fill out your copyright notice in the Description page of Project Settings.


#include "GameFeaturedPawn.h"

#include "Components/GameFrameworkComponentManager.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY(Log_GameFeatured_VehiclePawn);

void AGameFeaturedPawn::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	UGameFrameworkComponentManager::AddGameFrameworkComponentReceiver(this);
}

void AGameFeaturedPawn::BeginPlay()
{
	Super::BeginPlay();

	// Add the game feature listener if it was not added yet
	AddGameFeatureListener();
}

void AGameFeaturedPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Remove the game feature listener if it was added
	RemoveGameFeatureListener();
}

//FName AGameFeaturedGameStateBase::GetDefinitionAssetName_Implementation() const
//{
//	return GetFName();
//}

#pragma region GameFeatures
bool AGameFeaturedPawn::AddGameFeatureListener()
{

	UE_LOGFMT(Log_GameFeatured_VehiclePawn, VeryVerbose, "{Time}: {Line} {Class}: Attempting to add game feature listener to GameFeturedWheeledVehiclePawn: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));

	if (bGameFeaturesInitialized)
	{
		// Log that the game feature listener is not initialized
		UE_LOGFMT(Log_GameFeatured_VehiclePawn, VeryVerbose, "{Time}: {Line} {Class}: Game feature listener is already initialized for GameFeturedWheeledVehiclePawn: {Name}",
			("Class", GET_CLASSNAME_WITH_FUNCTION),
			("Line", GET_LINE_NUMBER),
			("Time", GET_CURRENT_TIME),
			("Name", GetFName().ToString()));
		return false;
	}

	bGameFeaturesInitialized = true;


	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(this, UGameFrameworkComponentManager::NAME_GameActorReady);
	// Log addition of the game feature listener
	UE_LOGFMT(Log_GameFeatured_VehiclePawn, Verbose, "{Time}: {Line} {Class}: Added game feature listener to GameFeturedWheeledVehiclePawn: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));
	return false;
}
bool AGameFeaturedPawn::RemoveGameFeatureListener()
{
	UE_LOGFMT(Log_GameFeatured_VehiclePawn, VeryVerbose, "{Time}: {Line} {Class}: Attempting to remove game feature listener from GameFeturedWheeledVehiclePawn: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));

	if (!bGameFeaturesInitialized)
	{
		// Log that the game feature listener is not initialized
		UE_LOGFMT(Log_GameFeatured_VehiclePawn, VeryVerbose, "{Time}: {Line} {Class}: Game feature listener is not initialized for GameFeturedWheeledVehiclePawn: {Name}",
			("Class", GET_CLASSNAME_WITH_FUNCTION),
			("Line", GET_LINE_NUMBER),
			("Time", GET_CURRENT_TIME),
			("Name", GetFName().ToString()));
		return false;
	}

	bGameFeaturesInitialized = false;

	UGameFrameworkComponentManager::RemoveGameFrameworkComponentReceiver(this);
	// Log removal of the game feature listener
	UE_LOGFMT(Log_GameFeatured_VehiclePawn, Verbose, "{Time}: {Line} {Class}: Removed game feature listener from GameFeturedWheeledVehiclePawn: {Name}",
		("Class", GET_CLASSNAME_WITH_FUNCTION),
		("Line", GET_LINE_NUMBER),
		("Time", GET_CURRENT_TIME),
		("Name", GetFName().ToString()));
	return true;
}