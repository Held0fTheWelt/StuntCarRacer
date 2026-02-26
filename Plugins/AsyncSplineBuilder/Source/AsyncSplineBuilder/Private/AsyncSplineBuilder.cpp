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
 * 
 Separate Trace-Settings nur für GroundWalls (damit SnapMeshes z.B. “Landscape-only” bleiben kann, GroundWalls aber “WorldStatic” trifft).
 UV-Scaling nach realer Wall-Höhe (damit Material nicht stretcht, wenn Drop/Cliff groß wird).
 * 
 */

#include "AsyncSplineBuilder.h"

#define LOCTEXT_NAMESPACE "FAsyncSplineBuilderModule"

void FAsyncSplineBuilderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FAsyncSplineBuilderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAsyncSplineBuilderModule, AsyncSplineBuilder)