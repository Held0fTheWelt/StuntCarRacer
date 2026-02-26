// Yves Tanas 2025 All Rights Reserved.

/**
 * This file is part of the "AsyncSplineBuilder" plugin.
 *
 * Use of this software is governed by the Fab Standard End User License Agreement
 * (EULA) applicable to this product, available at:
 * https://www.fab.com/eula
 *
 * Except as expressly permitted by the Fab Standard EULA, any reproduction,
 * distribution, modification, or use of this software, in whole or in part,
 * is strictly prohibited.
 *
 * This software is provided on an "AS IS" basis, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied, including but not
 * limited to warranties of merchantability, fitness for a particular purpose,
 * and non-infringement.
 *
 * ---------------------------------------------------------------------------
 * File:        [AsyncSplineBuilder.cpp]
 * Description: Module implementation and startup/shutdown logic for the
 *              AsyncSplineBuilder plugin.
 * ---------------------------------------------------------------------------
 */

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FAsyncSplineBuilderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
