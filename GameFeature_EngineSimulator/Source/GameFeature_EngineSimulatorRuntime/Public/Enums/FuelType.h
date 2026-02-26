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
 * File:        [FuelType.h]
 * Created:     [2025-06-18]
 * Description: [Short file/module purpose]
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "FuelType.generated.h"

UENUM(BlueprintType)
enum class EFuelType : uint8
{
    FT_NONE         UMETA(DisplayName = "None"),
    FT_PETROL       UMETA(DisplayName = "Petrol"),
    FT_DIESEL       UMETA(DisplayName = "Diesel"),
    FT_ETHANOL      UMETA(DisplayName = "Ethanol"),
    FT_LPG          UMETA(DisplayName = "LPG"),
    FT_HYBRID       UMETA(DisplayName = "Hybrid"),
    FT_ELECTRICT    UMETA(DisplayName = "Electric"),
    FT_CUSTOM       UMETA(DisplayName = "Custom"),
};