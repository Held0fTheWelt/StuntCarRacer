// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/TimeTrialGameStateInterface.h"
#include "TimeTrialComponent.generated.h"


USTRUCT(BlueprintType)
struct FGateInformation
{
	GENERATED_BODY()

public:
	FGateInformation() = default;

	UPROPERTY(BlueprintReadOnly)
	int32 GateIndex = -1;
	UPROPERTY(BlueprintReadOnly)
	float TimeAtGateInSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FRoundInformation
{
	GENERATED_BODY()

public:
	FRoundInformation() = default;
	
	UPROPERTY(BlueprintReadOnly)
	float StartTimeInSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	float EndTimeInSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	TArray<FGateInformation> Gates;

};

USTRUCT(BlueprintType)
struct FTrackTimeInformation
{
	GENERATED_BODY()

public:
	FTrackTimeInformation() = default;

	UPROPERTY(BlueprintReadOnly)
	float BestTimeInSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	float LastTimeInSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly)
	int32 Round = 0;
	UPROPERTY(BlueprintReadOnly)
	TArray<FRoundInformation> Rounds;


};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TIMETRIALMODERUNTIME_API UTimeTrialComponent : public UActorComponent, public ITimeTrialGameStateInterface
{
	GENERATED_BODY()

public:	
	UTimeTrialComponent();


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDebug = false;

private:
	UFUNCTION()
	void OnTargetTracked(AActor* TrackedActor, int32 TargetIndex);

	void StartRound(int32 TimesIndex);

	void FinishRound(int32 TimesIndex, int32 FinishGateIndex);

private:
	UPROPERTY(Transient)
	int32 NumberOfGates = 0;
	UPROPERTY(Transient)
	int32 NumberOfRounds = 3;
	UPROPERTY(Transient)
	bool bRaceHasStarted = false;

	UPROPERTY(Transient)
	TArray<class AActor*> TrackedTargets;
	
	UPROPERTY(Transient)
	TArray<FTrackTimeInformation> TrackedTargetTimes;

	FTimesTargetTrackedSignature OnTimesTargetTracked;
	FNewRecordTime OnNewRecordTime;
	FRaceHasStarted OnRaceHasStarted;
	FRaceHasFinished OnRaceHasFinished;
public:
	/** Notify about the engine active state change */
	virtual FTimesTargetTrackedSignature& GetTimesTargetTrackedSignature() override
	{
		return OnTimesTargetTracked;
	};

	/** Notify about the second engine active state change */
	virtual FNewRecordTime& GetNewRecordTime() override
	{
		return OnNewRecordTime;
	};

	/** Notify that the race has started */
	virtual FRaceHasStarted& GetRaceHasStarted() override
	{
		return OnRaceHasStarted;
	};


	/** Notify that the race has started */
	virtual FRaceHasFinished& GetRaceHasFinished() override
	{
		return OnRaceHasFinished;
	};
};
