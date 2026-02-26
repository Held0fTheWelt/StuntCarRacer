// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FrameworkSaveGame.generated.h"

USTRUCT(BlueprintType)
struct FTrackTimeRecord
{
	GENERATED_BODY()

public:
	FTrackTimeRecord() = default;
	
	UPROPERTY(BlueprintReadOnly)
	FName PlayerName;
	UPROPERTY(BlueprintReadOnly)
	float BestTimeInSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	TArray<float> GateTimesInSeconds;

	// Unused yet, but could be useful for future features
	UPROPERTY(BlueprintReadOnly)
	float RecordDateTime = 0.0f;
};

USTRUCT(BlueprintType)
struct FTrackSaveInformation
{
	GENERATED_BODY()

public:
	FTrackSaveInformation() = default;
	
	UPROPERTY(BlueprintReadOnly)
	FName TrackName;

	UPROPERTY(BlueprintReadOnly)
	TArray<FTrackTimeRecord> TrackRecords;


};

/**
 * 
 */
UCLASS()
class FRAMEWORK_API UFrameworkSaveGame : public USaveGame
{
	GENERATED_BODY()
	
};
