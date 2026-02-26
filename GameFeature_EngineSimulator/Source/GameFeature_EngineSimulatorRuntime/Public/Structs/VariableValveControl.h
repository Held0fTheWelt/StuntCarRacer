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
 * File:        [VariableValveControl.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/VariableValveType.h"
#include "VariableValveControl.generated.h"

USTRUCT(BlueprintType)
struct FVariableValveControl
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|VVT")
    EVariableValveType VVTType = EVariableValveType::VVT_NONE;

    // z.B. bei VVT: Grad, wie stark Steuerzeiten verschiebbar sind
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|VVT")
    float TimingRange = 0.0f; // Grad Kurbelwelle

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|VVT")
    float LiftRange = 0.0f; // mm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|VVT")
    bool bIntakeControlled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|VVT")
    bool bExhaustControlled = false;
};