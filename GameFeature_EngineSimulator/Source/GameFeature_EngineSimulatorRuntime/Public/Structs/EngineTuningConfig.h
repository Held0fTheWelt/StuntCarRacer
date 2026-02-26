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
 * File:        [EngineTuningConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "EngineTuningConfig.generated.h"

USTRUCT(BlueprintType)
struct FEngineTuningConfig
{
    GENERATED_BODY()

    // Direkte Einspritzung, High-Octane Fuel, Sportnockenwelle etc.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Tuning")
    bool bDirectInjection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Tuning")
    bool bHighOctaneFuel = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Tuning")
    bool bPerformanceCams = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Tuning")
    bool bSportsExhaust = false;

    // Add-Ons: Water/Methanol Injection, Nitrous, … später ergänzbar!
};