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
 * File:        [InductionSystemConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/InductionType.h"
#include "Structs/TurboChargerConfig.h"
#include "InductionSystemConfig.generated.h"

USTRUCT(BlueprintType)
struct FInductionSystemConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Induction")
    EInductionType InductionType = EInductionType::IT_NATURALLY_ASPIRATED;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Induction")
    FTurboChargerConfig TurboCharger;

    // ... für Kompressor, Twincharger etc. weitere Felder ergänzbar!
};