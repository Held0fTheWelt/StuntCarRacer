#pragma once

#include "CoreMinimal.h"

/**
 * Dedicated log categories for the CarAI runtime module.
 *
 * Usage:
 *   UE_LOG(LogCarAIAgent,   Display, TEXT("...")); // major agent lifecycle transitions
 *   UE_LOG(LogCarAIAgent,   Verbose, TEXT("...")); // step-level state changes
 *   UE_LOG(LogCarAIAgent,   VeryVerbose, TEXT("...")); // per-frame / per-poll detail
 *
 * To enable verbose output at runtime, add to DefaultEngine.ini:
 *   [Core.Log]
 *   LogCarAIAgent=Verbose
 *   LogCarAITraining=Verbose
 *
 * Or via console command during PIE:
 *   Log LogCarAIAgent Verbose
 *   Log LogCarAITraining Verbose
 */

/** Per-agent runtime lifecycle: attach, idle, authorization, stepping, episode. */
DECLARE_LOG_CATEGORY_EXTERN(LogCarAIAgent, Log, All);

/** Training workflow: discovery, registration, genome assignment, evaluation authorization. */
DECLARE_LOG_CATEGORY_EXTERN(LogCarAITraining, Log, All);
