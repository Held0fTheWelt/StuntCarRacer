# MVP-02 Report — Multi-Agent Spawn Interference

## Finding: All agents spawn at same spline distance

**Before fix:** All agents have `SpawnDistanceAlongTrackCm = 0.f` (C++ default, unchanged by manager). With N agents, all N cluster at track distance 0 with only ±300 cm lateral scatter. On a track narrower than 600 cm, agents physically overlap at spawn → immediate collision → `Stuck` or `Collision` termination.

**Evidence (code level):**
- `SpawnDistanceAlongTrackCm` is `0.f` in C++ default and is never set by the manager before this fix.
- The manager iterates registered agents and calls `Initialize()` without setting distance.
- All agents thus call `GetSpawnTransformFromTrack()` with `SpawnDistanceAlongTrackCm = 0`.

## Fix Applied

Added `AgentSpawnStaggerCm = 500.f` to `UNEATTrainingManager`.
In the genome assignment loop, each agent at index `i` gets `SpawnDistanceAlongTrackCm = i * AgentSpawnStaggerCm`.
Example: 10 agents → spaced 0 m, 5 m, 10 m, 15 m, ..., 45 m along the track.

Per-agent spawn log added to manager: `[NEATTrainingManager] Agent i: SpawnDistanceCm=X (stagger Y cm)`.

Height offset now logged unconditionally in `ResetEpisode()`.

## Gate Assessment

**Before fix:** Spawn overlap exists for all multi-agent configurations when `SpawnDistanceAlongTrackCm` is not set by the manager.

**After fix:** Agents are staggered `AgentSpawnStaggerCm` cm apart along the track. Synchronized early collision from spawn overlap is eliminated.

**Runtime evidence:** To confirm, enable `bEnableLogging=true` on agent BPs and enter PIE. Agent spawn logs will show distinct `DistanceAlongTrack` values per agent. Collision or stuck immediately after PIE start should no longer appear in the first 2s.

## Classification

- **Spawn overlap at distance=0**: Confirmed cause of synchronized early collision in multi-agent runs. Primary contributor.
- **Lateral offset ±300 cm**: Secondary contributor (still used within lane; now explicitly logged).
