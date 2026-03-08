// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarAIRuntimeModule.h"
#include "CarAIRuntimeLogging.h"

DEFINE_LOG_CATEGORY(LogCarAIAgent);

#define LOCTEXT_NAMESPACE "FCarAIRuntimeModule"

void FCarAIRuntimeModule::StartupModule()
{
	// Identify running build so we can confirm behavior comes from current source (see BUILD.md).
	UE_LOG(LogCarAIAgent, Display, TEXT("[CarAI] CarAIRuntime module loaded (plugin source build)."));
}

void FCarAIRuntimeModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCarAIRuntimeModule, CarAIRuntime)
