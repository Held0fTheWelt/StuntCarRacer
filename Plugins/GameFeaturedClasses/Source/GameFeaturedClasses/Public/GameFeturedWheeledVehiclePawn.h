// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "GameFeturedWheeledVehiclePawn.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(Log_GameFeatured_WheeledVehiclePawn, Log, All);

/**
 * 
 */
UCLASS()
class GAMEFEATUREDCLASSES_API AGameFeturedWheeledVehiclePawn : public AWheeledVehiclePawn
{
	GENERATED_BODY()

protected:
	/** Initialization */
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	bool AddGameFeatureListener();

	bool RemoveGameFeatureListener();

	UPROPERTY(Transient)
	bool bGameFeaturesInitialized = false;
};
