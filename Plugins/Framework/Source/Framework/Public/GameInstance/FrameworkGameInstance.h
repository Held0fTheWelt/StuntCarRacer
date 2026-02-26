// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FrameworkGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class FRAMEWORK_API UFrameworkGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	virtual void Init() override;
	virtual void Shutdown() override;

	//void FVector GetRespawnLocationOffset(FVector& OutOffset) const { OutOffset = RespawnOffset; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn")
	FVector RespawnOffset = FVector(0.f, 0.f, 200.f);
};
