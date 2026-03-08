# Final Root Cause and Fix Report

## Original Symptoms (from Task.md)

- Agents may be spawned at the same location and rotation.
- Early termination reasons include `Collision`, `Fell off track`, and `Stuck`.
- Some logs show valid forward motion during forced-forward diagnostic.
- Some logs show near-synchronous failure patterns across multiple agents.
- There may be disagreement between source code expectations and runtime behavior.
- There may be stale Blueprint or asset overrides.
- There may be trace contamination from other agent vehicles.
- `Stuck` may be firing during the same window where collision/offtrack is intentionally suppressed.

---

## Root Cause Classification

### PRIMARY CAUSES

#### 1. `Stuck` fires without grace period protection (MVP-04)

**Symptom:** Episodes terminate with `Stuck` at t=2.0s even with 2.5s grace window and forced-forward diagnostic active.

**Root cause:** `CheckTerminalConditions()` accumulated the stuck timer unconditionally from episode start. `SpeedNorm = 0` at t=0 → timer starts immediately → `Stuck` fires at t=2.0s. The grace window in `ComputeReward()` protects only `Collision` and `Fell off track`. `CheckTerminalConditions()` had no awareness of `EpisodeGraceTimeRemaining`.

**Fix (MVP-04):** `Stuck` timer now suppressed during `EpisodeGraceTimeRemaining > 0` AND during `bEnableForcedForwardDiagnostic && EpisodeTimeAccum < ForcedForwardDiagnosticDuration`. Timer reset during suppressed windows.

**Evidence:** Code analysis proves the unprotected path. After fix: `Stuck` cannot fire before t=2.5s (grace) under any configuration.

---

#### 2. All agents spawn at same spline distance (MVP-02)

**Symptom:** Near-synchronous collision/stuck burst at episode start in multi-agent runs.

**Root cause:** `SpawnDistanceAlongTrackCm = 0.f` by default. The `NEATTrainingManager` never set this property before calling `Initialize()`. All N agents spawned at spline distance 0 with only random lateral jitter (±300 cm). On tracks narrower than 600 cm, agents physically overlapped → immediate physics collision → `Stuck` or `Collision` at t<0.5s.

**Fix (MVP-02):** `UNEATTrainingManager` now sets `Agent->SpawnDistanceAlongTrackCm = i * AgentSpawnStaggerCm` (default 500 cm per agent) before authorizing each agent. N=10 agents → 0 m, 5 m, 10 m, ..., 45 m along track. Added `AgentSpawnStaggerCm` property (tunable via Details panel).

**Evidence:** `SpawnDistanceAlongTrackCm` default is 0 in both C++ and manager source. Manager assignment loop read and confirmed — no distance assignment before fix.

---

#### 3. Sensor traces saw neighboring agent car bodies (MVP-03)

**Symptom:** False `Collision` termination in multi-agent runs — agents near each other triggered collision threshold from rays hitting adjacent cars.

**Root cause:** All trace calls (`TraceAdaptiveRay`, `TraceFixedRay`, ground ray) used `FCollisionQueryParams(tag, false, GetVehicleActor())` — ignoring only the own vehicle. Other agent vehicles were visible to traces. In a 10-agent batch at close spawn, a neighboring car body directly in a ray's path returned `HitDistNorm < 0.05` → `CollisionTerminalThreshold` → `Collision` termination.

**Fix (MVP-03):** Added `BuildTraceIgnoreParams()` helper. Uses `URacingAgentRegistrySubsystem::GetAllAgents()` to collect all registered agent vehicles and add them to the ignore list. All nine trace call sites updated.

**Evidence:** `FCollisionQueryParams` constructor with single actor confirmed in all trace sites before fix.

---

### CONTRIBUTING CAUSES

#### 4. `AirborneLong` fires without grace protection (MVP-04)

**Symptom:** If spawn height offset or physics bounce lifts vehicle above `RayGroundDist < 0.1f` for 3+ seconds, `AirborneLong` terminates episode during or just after grace window.

**Root cause:** Same as Stuck — `CheckTerminalConditions()` accumulated airborne timer without grace awareness.

**Fix (MVP-04):** Airborne timer now suppressed during grace window. Timer reset during grace so it starts fresh after grace ends.

**Severity:** Contributing cause (3s airborne timeout is longer than grace, so less commonly triggered than Stuck's 2s timeout).

---

#### 5. Spawn lateral offset not logged (MVP-01, MVP-02)

**Symptom:** Spawn debugging was incomplete — lateral offset applied was computed but not logged.

**Fix (MVP-01):** Added unconditional lateral offset log in `ResetEpisode()`. Added height offset log unconditionally.

**Severity:** Observability gap, not a functional bug.

---

### SECONDARY ISSUES

#### 6. Config dump missing (MVP-06)

**Issue:** No runtime log of effective config values. Blueprint overrides to C++ defaults could cause silent behavior changes.

**Fix (MVP-06):** Added config dump log at Display level in `ResetEpisode()`. Fires once per episode. Covers all 14 relevant config properties.

---

### DISPROVEN

#### 7. Ground ray semantic mismatch (MVP-05)

**Hypothesis:** `RayGroundDist` might be interpreted inconsistently across consumers.

**Finding:** Semantic is `1 = ground close/safe, 0 = no ground/danger`. All five consumers (`ComputeReward` gap warning/terminal, `CheckTerminalConditions` airborne, `StepOnce` grounded frame count, diagnostic suppress) consistently use high=safe, low=danger. No mismatch.

#### 8. On-track false offtrack termination from ground ray (MVP-05)

**Hypothesis:** Ground ray might trigger offtrack while vehicle is on track.

**Finding:** At normal vehicle height (50–80 cm CoM), `RayGroundDist ≈ 0.88`. `GapTerminalThreshold = 0.1` requires >450 cm altitude to trigger. No false positive under normal on-track conditions.

---

## Symptom-to-Fix Mapping

| Original Symptom | Root Cause | Fix |
|---|---|---|
| Agents spawned at same location | All use `SpawnDistanceAlongTrackCm=0` | MVP-02: manager sets per-agent stagger |
| Near-synchronous failure burst | Spawn overlap + trace contamination + Stuck no grace | MVP-02 + MVP-03 + MVP-04 |
| `Collision` termination (multi-agent) | Ray hitting neighboring agent body | MVP-03: BuildTraceIgnoreParams |
| `Stuck` termination at t~2s | Grace window not protecting CheckTerminalConditions | MVP-04: grace + diagnostic suppression |
| `Stuck` during forced-forward diagnostic | Same as above | MVP-04: forced-forward window suppression |
| `Fell off track` at spawn | No grace in some paths (now has grace+diagnostic suppress) | Already had grace; diagnostic suppress pre-existing |
| Blueprint/config ambiguity | No runtime config log | MVP-06: per-episode config dump |
| Trace semantic unclear for other agents | Only own vehicle ignored | MVP-03: all agent vehicles ignored |

## What Remains Open

1. **Runtime evidence**: All fixes are code-level. PIE logs have not been captured. MVP-07 documents the runtime validation procedure.
2. **Blueprint override audit**: Config dump log now exists. On first PIE run, search for `Config dump:` in Output Log and compare to C++ defaults table in MVP-06 report.
3. **Track narrowness**: If track width < `AgentSpawnStaggerCm` lateral scatter, agents may still overlap. This is environment-specific. Reduce `SpawnLateralOffsetMaxCm` or increase `AgentSpawnStaggerCm` if needed.

## Repo State

All diagnostic logs added in MVP-01 through MVP-06 are retained — they are useful for ongoing NEAT training diagnosis. The forced-forward diagnostic is still `bEnableForcedForwardDiagnostic = false` by default; enable in BP when diagnosing vehicle drive issues.

Commits: MVP-01 through MVP-07 applied to `master`. Code is clean.
