# MVP-01 Report: Lock Current Baseline

**Date:** 2026-03-10
**Status:** BASELINE LOCKED
**Evidence Source:** StuntCarRacer.log (2026.03.09-23:51:22)

---

## Current Codebase State

### 1. Spawn/Reset Logic (RacingAgentComponent::ResetEpisode)

**File:** `Plugins/GameFeatures/CarAI/Source/CarAIRuntime/Private/Components/RacingAgentComponent.cpp`

- **SpawnDistanceAlongTrackCm**: Default 0 cm. Manager sets per-agent via `Agent->SpawnDistanceAlongTrackCm = i * AgentSpawnStaggerCm` (default 500 cm).
- **SpawnLateralOffsetMaxCm**: 0 cm (no lateral jitter)
- **SpawnHeightOffsetCm**: 50 cm
- **ForcedForwardDiagnostic**: Blueprint-controlled; applies Steer=0, Throttle=1.0, Brake=0 for 2.0s with offtrack suppression.

**Config dump from log:**
```
Grace=2.5s StuckSpeedNorm=0.050 StuckSec=2.0 AirborneSec=5.0 
GapTerminal=0.100 CollisionTerminal=0.050 SpawnLateralMax=0cm 
SpawnHeight=50cm SpawnDistAlongTrack=0cm (Agent 1), 500cm (Agent 2)
ForcedForward=ON (early), OFF (later) ForcedForwardDur=2.0s 
DiagSuppressCm=200 MaxSteps=5000
```

### 2. Forced-Forward Activation Logic

**File:** Line ~692

```cpp
const bool bInForcedForwardWindow = bEnableForcedForwardDiagnostic && (EpisodeTimeAccum < ForcedForwardDiagnosticDuration);
```

- Applied as forced action: Steer=0, Throttle=1.0, Brake=0
- Lasts 2.0 seconds
- Toggleable via Blueprint property

### 3. Stuck Evaluation Logic (CheckTerminalConditions)

**File:** Line 1315-1336

```cpp
const bool bStuckSuppressed = (EpisodeGraceTimeRemaining > 0.f) ||
    (bEnableForcedForwardDiagnostic && EpisodeTimeAccum < ForcedForwardDiagnosticDuration);
if (!bStuckSuppressed)
{
    if (Obs.SpeedNorm < RewardCfg.StuckSpeedNorm)
    {
        StuckTimeAccum += DeltaTime;
        if (StuckTimeAccum >= RewardCfg.StuckTimeSeconds)
        {
            OutReason = TEXT("Stuck");
            return true;
        }
    }
    else
    {
        StuckTimeAccum = 0.f;
    }
}
else
{
    StuckTimeAccum = 0.f; // Reset during suppressed window
}
```

- Suppressed during grace (2.5s) AND forced-forward diagnostic (2.0s)
- Triggered if SpeedNorm < 0.050 for 2.0+ seconds
- Timer reset during suppression

### 4. Offtrack / Collision / Grounded Logic

- **Collision**: Ray hit < 0.05 normalized distance (with grace suppression)
- **Offtrack**: Ground ray RayGroundDist < 0.1, with grace + diagnostic suppression
- **Grounded**: Tracked via ground ray in real-time

### 5. Generation Export/Import

**File:** NEATTrainingManager.cpp

- **LoadGenerationGenomes()**: Loads from `generation_N_genomes.json` + individual `genome_*.json`
- **ExportFitnessValues()**: Writes `generation_N.json` only after ALL genomes evaluated
- **OnPythonEvolutionComplete()**: Reads `training_state.json` for generation sync
- **Cross-PIE resume**: If Agents.Num()==0, sets `bPendingGenomesReady=true`, waits for next PIE

---

## Observed Failure Patterns

### Pattern 1: Early Stuck Termination (t~2s, progress~0.1m)

```
22:52:51:585 - Stuck | Fitness=0.05 Steps=14 Progress=0.1m GenomeID=1
22:52:51:586 - Stuck | Fitness=0.17 Steps=14 Progress=0.2m GenomeID=2
```

**Analysis:**
- Both terminate at ~2 seconds (14 steps × ~0.12s ≈ 1.68s)
- Minimal progress (0.1-0.2m)
- Grace window is 2.5s (should protect)
- Forced-forward diagnostic 2.0s suppresses Stuck (should also protect)

**Hypothesis:** Vehicle fails to engage throttle/drivetrain despite forced-forward action being applied.

### Pattern 2: Collision After Meaningful Progress (32m)

```
22:54:26:826 - Collision | Fitness=11.97 Steps=350 Progress=32.0m GenomeID=2
```

**Analysis:**
- Agent traveled 32 meters over 350 steps (~42 seconds)
- Real motion occurred
- Likely genuine collision with track geometry

### Pattern 3: Offtrack After Significant Progress (39m)

```
22:54:27:065 - Fell off track | Fitness=18.64 Steps=??? Progress=38.6m GenomeID=1
Diagnostic: RayGroundDist=0.000 | ground_ray_hit=no ground_dist_cm=500 | lateral_cm=1111 | wheels_grounded=1/4
```

**Analysis:**
- Agent traveled ~39 meters
- Ground ray shows no ground (danger state)
- Lateral 1111 cm (off spline)
- Genuine offtrack termination (likely ramp/gap encounter)

### Pattern 4: Training Generation Drift

```
22:54:42:755 - generation=0, exported_fitness_file=generation_1.json
22:54:43:266 - [Python] ERROR: Missing fitness export for generation 2
```

**Analysis:**
- Manager reports generation=0 but references generation_1.json
- Python looks for generation_2.json (off by 2)
- Generation numbering misalignment between UE and Python

---

## Change from Old Baseline

**Old (Task.md context):**
- All agents at spline distance 0 → immediate overlap collision
- Trace contamination from agent vehicles → false collision

**Now (Current):**
- Spawn stagger working (0cm, 500cm verified in logs)
- Traces ignore all agents (no contamination logs)
- **New issue**: Early Stuck despite suppression (vehicle engagement problem)
- **New issue**: Generation drift (training orchestration problem)

---

## MVP-01 Gate Status

✓ **Gate Passed**: Baseline established.

**Fixed Issues Confirmed:**
1. Multi-agent spawn stagger applied (500cm between agents)
2. Trace ignore active (no cross-agent contamination)
3. Grace + diagnostic window suppression in code

**New Issues Identified for MVP-02+:**
1. Early Stuck at ~2s with 0.1m progress despite suppression
2. Generation numbering drift (UE vs Python)
3. Silent Blueprint override (ForcedForward can toggle without rebuild)

---

## Next: MVP-02

Add startup diagnostics to identify why vehicles fail to engage motion in first 2 seconds.

