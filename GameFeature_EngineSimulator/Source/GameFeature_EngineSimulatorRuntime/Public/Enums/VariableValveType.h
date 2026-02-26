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
 * File:        [VariableVavleType.h]
 * Created:     [2025-06-12]
 * Description: [Short file/module purpose]
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "VariableValveType.generated.h"

UENUM(BlueprintType)
enum class EVariableValveType : uint8
{
    VVT_NONE        UMETA(DisplayName = "None"),
    VVT_VVT         UMETA(DisplayName = "VVT (Variable Valve Timing)"),
    VVT_VVTL        UMETA(DisplayName = "VVTL (Variable Valve Timing and Lift)"),
    VVT_VTEC        UMETA(DisplayName = "VTEC (Honda Variable Valve Timing and Lift Electronic Control)"),
    VVT_VANOS       UMETA(DisplayName = "VANOS (BMW VVT)"),
    VVT_CUSTOM      UMETA(DisplayName = "Custom"),
};