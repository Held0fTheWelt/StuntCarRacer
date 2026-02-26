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
 * File:        [CoolingSystemConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/CoolingType.h"
#include "CoolingSystemConfig.generated.h"

USTRUCT(BlueprintType)
struct FCoolingSystemConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cooling")
    ECoolingType CoolingType = ECoolingType::CT_COOLED_WATER;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cooling")
    float CoolantVolume = 5.0f; // Liter

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cooling")
    float RadiatorEfficiency = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cooling")
    float FanAirflow = 0.0f; // m³/min

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Cooling")
    float OilCoolerEfficiency = 0.0f; // für OilCooled

    // Optional: Thermostat, elektrische Wasserpumpe etc.
};