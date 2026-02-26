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
 * File:        [TurboChargerConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/ValveType.h"
#include "TurboChargerConfig.generated.h"

USTRUCT(BlueprintType)
struct FTurboChargerConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    float MaxBoostPressure = 1.0f; // bar (Überdruck)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    float SpoolTime = 0.5f; // Zeit zum Hochdrehen in Sekunden

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    float WastegatePressure = 0.8f; // bar

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    float IntercoolerEfficiency = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Turbo")
    float CompressorEfficiency = 0.7f;

    // Für TwinTurbo, Kompressor oder E-Turbo weitere Parameter ergänzbar!
};