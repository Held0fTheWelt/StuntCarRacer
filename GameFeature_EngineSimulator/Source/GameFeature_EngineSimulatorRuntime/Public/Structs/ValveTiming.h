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
 * File:        [ValveTiming.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/ValveType.h"
#include "ValveTiming.generated.h"

USTRUCT(BlueprintType)
struct FValveTiming
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    float OpenAngle = 0.0f; // Grad KW

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    float CloseAngle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    EValveType ValveType = EValveType::VT_INTAKE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    float MaxLift = 10.0f; // mm

    // Für variable Steuerzeiten (Offset/Bereich)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    float VariableTimingMin = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Valve")
    float VariableTimingMax = 0.0f;
};