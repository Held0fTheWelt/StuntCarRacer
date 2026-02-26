// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SpeedAndGearWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpeedUpdated, float, Speed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGearUpdated, int32, Gear);
/**
 * 
 */
UCLASS(Abstract, editinlinenew, BlueprintType, Blueprintable, meta = (DontUseGenericSpawnObject = "True", DisableNativeTick))
class CARSTATISTICSRUNTIME_API USpeedAndGearWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	FOnSpeedUpdated OnSpeedUpdated;
	FOnGearUpdated OnGearUpdated;

protected:
	virtual void NativePreConstruct() override;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Label_Gear;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Label_Speed;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	class UTextBlock* Label_Unit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bIsMPH = false;

	/** Implemented in Blueprint to display the new speed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle")
	void OnSpeedUpdate(float NewSpeed);

	/** Implemented in Blueprint to display the new gear */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle")
	void OnGearUpdate(int32 NewGear);

private:
	UPROPERTY()
	float CurrentSpeed = 0.0f;
	UPROPERTY()
	int32 CurrentGear = 0;

	UFUNCTION()
	void UpdateSpeed(float NewSpeed);
	UFUNCTION()
	void UpdateGear(int32 NewGear);
};
