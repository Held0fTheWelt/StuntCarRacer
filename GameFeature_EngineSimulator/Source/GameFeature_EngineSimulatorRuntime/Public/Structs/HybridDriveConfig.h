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
 * File:        [HybridDriveConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/HybridType.h"
#include "HybridDriveConfig.generated.h"

USTRUCT(BlueprintType)
struct FHybridDriveConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Hybrid")
    EHybridType HybridType = EHybridType::HT_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Hybrid")
    float ElectricPower = 0.0f; // kW

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Hybrid")
    float BatteryCapacity = 0.0f; // kWh

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Hybrid")
    float RegenEfficiency = 0.6f;
};