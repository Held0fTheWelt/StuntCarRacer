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
 * File:        [EngineConfig.h]
 * Created:     [2025-06-18]
 * Description:
 * -------------------------------------------------------------------------------
 */
#pragma once

#include "CoreMinimal.h"
#include "Enums/MotorType.h"
#include "Enums/FuelType.h"
#include "Enums/CombustionCycle.h"
#include "Structs/CylinderConfig.h"
#include "Structs/InductionSystemConfig.h"
#include "Structs/CoolingSystemConfig.h"
#include "Structs/HybridDriveConfig.h"
#include "Structs/EngineTuningConfig.h"
#include "EngineConfig.generated.h"

USTRUCT(BlueprintType)
struct FEngineConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    EMotorType MotorType = EMotorType::MT_INLINE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    EFuelType FuelType = EFuelType::FT_PETROL;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    ECombustionCycle CombustionCycle = ECombustionCycle::CC_FOUR_STROKE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    int32 NumCylinders = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    float RedlineRPM = 7000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    float IdleRPM = 800.0f;

    // Optionale Grunddaten für alle Zylinder (werden aufgeteilt oder individuell überschrieben)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    FCylinderConfig DefaultCylinder;

    // Individuelle Konfiguration für jeden Zylinder (falls leer, wird DefaultCylinder verwendet)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    TArray<FCylinderConfig> Cylinders;

    // Schwungrad/Trägheit (kg*m²)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    float Inertia = 0.15f;

    // Wirkungsgrad (gesamt)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim")
    float Efficiency = 0.3f;

    // Weitere Parameter (z.B. für Turbo, Supercharger) können später hinzugefügt werden!
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Engine")
    FInductionSystemConfig InductionSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Engine")
    FCoolingSystemConfig CoolingSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Engine")
    FHybridDriveConfig HybridDrive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EngineSim|Engine")
    FEngineTuningConfig TuningConfig;
};