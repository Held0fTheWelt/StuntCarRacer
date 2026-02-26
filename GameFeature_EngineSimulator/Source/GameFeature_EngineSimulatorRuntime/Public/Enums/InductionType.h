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
 * File:        [InductionType.h]
 * Created:     [2025-06-12]
 * Description: [Short file/module purpose]
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "InductionType.generated.h"

UENUM(BlueprintType)
enum class EInductionType : uint8
{
    IT_NONE                 UMETA(DisplayName = "None"),
    IT_NATURALLY_ASPIRATED  UMETA(DisplayName = "Naturally Aspirated"),
    IT_CHARGED_TURBO        UMETA(DisplayName = "Turbocharged"),
    IT_CHARGED_SUPER        UMETA(DisplayName = "Supercharged"),
    IT_CHARGED_TWIN         UMETA(DisplayName = "Twincharged"),
    IT_BOOST_ELECTRIC       UMETA(DisplayName = "Electric Boost"),
    IT_CUSTOM               UMETA(DisplayName = "Custom"),
};