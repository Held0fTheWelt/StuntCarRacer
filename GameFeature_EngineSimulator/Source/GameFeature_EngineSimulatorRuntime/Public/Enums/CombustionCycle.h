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
 * File:        [CombustionCycle.h]
 * Created:     [2025-06-18]
 * Description: [Short file/module purpose]
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "CombustionCycle.generated.h"

UENUM(BlueprintType)
enum class ECombustionCycle : uint8
{
    CC_NONE         UMETA(DisplayName = "None"),
    CC_FOUR_STROKE  UMETA(DisplayName = "4-Stroke"),
    CC_TWO_STROKE   UMETA(DisplayName = "2-Stroke"),
    CC_ROTARY       UMETA(DisplayName = "Rotary"),
    CC_CUSTOM       UMETA(DisplayName = "Custom"),
};