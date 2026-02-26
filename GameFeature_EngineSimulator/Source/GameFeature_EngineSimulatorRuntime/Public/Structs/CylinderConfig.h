/*
 * Copyright (c) 2024 Yves Tanas
 * All Rights Reserved.
 *
 * This file is part of the Collections project.
 *
 * Unauthorized copying of this file, via any medium, is strictly prohibited.
 * Proprietary and confidential.
 *
 * This software may be used only as expressly authorized by the copyright holder.
 * Unless required by applicable law or agreed to in writing, software distributed
 * under this license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 *
 * For licensing inquiries, please contact: yves.tanas@example.com
 *
 * Contributors:
 *   Yves Tanas <yves.tanas@example.com>
 *
 * -------------------------------------------------------------------------------
 * File:        [CylinderConfig.h]
 * Created:     [2025-06-18]
 * Description: 
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Structs/ValveTiming.h"
#include "Structs/VariableValveControl.h"
#include "CylinderConfig.generated.h"

USTRUCT(BlueprintType)
struct FCylinderConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    float Bore = 86.0f; // mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    float Stroke = 86.0f; // mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    float ConnectingRodLength = 140.0f; // mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    float CompressionRatio = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    float FiringOffset = 0.0f; // Grad Kurbelwelle für Zündfolge

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    FVector RelativeLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    TArray<FValveTiming> ValveTimings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cylinder")
    FVariableValveControl VariableValveControl;
};