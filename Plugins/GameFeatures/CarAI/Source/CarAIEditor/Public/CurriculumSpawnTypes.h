#pragma once

#include "CoreMinimal.h"
#include "CurriculumSpawnTypes.generated.h"

USTRUCT(BlueprintType)
struct FCurriculumSpawnPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FTransform Transform;

	UPROPERTY(VisibleAnywhere)
	float Score01 = 0.f;

	UPROPERTY(VisibleAnywhere)
	bool bValid = false;
};
