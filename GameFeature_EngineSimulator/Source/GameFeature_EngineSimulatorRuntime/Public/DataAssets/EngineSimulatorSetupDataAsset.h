// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataAssets/PreLoadingDataAsset.h"
#include "Structs/EngineConfig.h"
#include "EngineSimulatorSetupDataAsset.generated.h"

/**
 * 
 */
UCLASS()
class GAMEFEATURE_ENGINESIMULATORRUNTIME_API UEngineSimulatorSetupDataAsset : public UPreLoadingDataAsset
{
	GENERATED_BODY()


public:
	UEngineSimulatorSetupDataAsset()
	{
		AssetTypeName = GetAssetType_Implementation();
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engine Simulator Setup")
	FEngineConfig EngineConfig;

	/** Returns the unique asset type for this data asset class */
	virtual const FPrimaryAssetType GetAssetType_Implementation() const override
	{
		return AssetType;
	}

private:
	/** Static asset type used for asset registry/manager integration */
	static const FPrimaryAssetType AssetType;
};
