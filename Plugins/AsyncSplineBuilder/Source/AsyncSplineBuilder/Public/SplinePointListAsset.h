// Yves Tanas 2025 All Rights Reserved.

/**
 * This file is part of the "AsyncSplineBuilder" plugin.
 *
 * Use of this software is governed by the Fab Standard End User License Agreement
 * (EULA) applicable to this product, available at:
 * https://www.fab.com/eula
 *
 * Except as expressly permitted by the Fab Standard EULA, any reproduction,
 * distribution, modification, or use of this software, in whole or in part,
 * is strictly prohibited.
 *
 * This software is provided on an "AS IS" basis, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied, including but not
 * limited to warranties of merchantability, fitness for a particular purpose,
 * and non-infringement.
 *
 * -------------------------------------------------------------------------------
 * File:        [SplinePointListAsset.h]
 * Description: Simple Data Asset to store a set of Spline Points
 * -------------------------------------------------------------------------------
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/SplineComponent.h"
#include "SplinePointListAsset.generated.h"

 /**
  * USplinePointListAsset
  *
  * Stores a named list of FSplinePoint entries.
  * Used to serialize / reuse spline shapes between actors/levels.
  */
UCLASS(BlueprintType)
class ASYNCSPLINEBUILDER_API USplinePointListAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Name of the spline list (organizational). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FName SplineListName;

	/** Raw spline points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FSplinePoint> PointList;
};
