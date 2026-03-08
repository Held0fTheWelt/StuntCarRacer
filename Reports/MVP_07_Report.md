# MVP-07 Report — Controlled Batch Test Matrix

## Test Configurations

| Test | Agent Count | ForcedForward | Expected First Failure Mode (Before Fixes) | Expected Behavior (After Fixes) |
|---|---|---|---|---|
| 1 — Single agent, forced-forward | 1 | ON | `Stuck` at t=2s (grace not protecting) | Drives forward; survives grace+diagnostic window; terminates on real obstacle or MaxSteps |
| 2 — Multi-agent, forced-forward | 8–10 | ON | `Collision` at t<2s (spawn overlap + trace contamination) | Agents staggered; no spawn overlap; forward motion established across batch |
| 3 — Single agent, normal policy | 1 | OFF | `Stuck` at t=2s OR `Collision` from initial random NEAT output | May terminate from genuine NEAT failure; no grace bypass |
| 4 — Multi-agent, normal policy | 8–10 | OFF | Synchronized `Stuck`/`Collision` burst at t=2s from multiple stacking bugs | Failures distributed over episode time; no synchronized burst at 2s |

## Fixes Applied (by MVP)

| Fix | MVP | Addressed Pattern |
|---|---|---|
| Spawn instrumentation (lateral offset + height log) | MVP-01 | Now observable; no change in behavior |
| Deterministic per-agent spline staggering (500 cm) | MVP-02 | Eliminates simultaneous spawn at distance=0 |
| Trace ignore includes all agent vehicles | MVP-03 | Eliminates false `Collision` from ray hitting neighboring cars |
| `Stuck` suppressed during grace and forced-forward windows | MVP-04 | Eliminates 2s premature `Stuck` at episode start |
| `AirborneLong` suppressed during grace window | MVP-04 | Eliminates premature airborne termination from spawn bounce |
| Config dump log per episode | MVP-06 | Makes Blueprint override drift detectable |
| Ground ray semantic verified consistent | MVP-05 | No mismatch; no fix needed |

## Predicted Test Outcomes (Code-Level)

### Test 1: Single agent, forced-forward ON

**Before fixes:**
- t=0: SpeedNorm=0 → stuck timer starts immediately (no grace protection in `CheckTerminalConditions`)
- t=2.0s: `Stuck` fires (grace window of 2.5s does not protect this path)
- Episode length: 2.0s regardless of forced-forward setting

**After fixes (MVP-04):**
- Stuck timer suppressed during grace (2.5s) and forced-forward window (2.0s)
- Combined suppress window: grace runs 0–2.5s; forced-forward runs 0–2.0s inside grace
- After grace ends at 2.5s: vehicle already moving forward (forced-forward window ran 0–2.0s at throttle=1.0)
- SpeedNorm should be well above 0.05 by t=2.5s → stuck timer never accumulates
- Expected outcome: agent drives beyond forced-forward window; terminates from real geometry or MaxSteps

**Supporting evidence from MVP-01:**
MVP-01 identified the root cause explicitly: "At episode start SpeedNorm = 0 < StuckSpeedNorm (0.05). The stuck timer starts immediately. If the vehicle does not reach SpeedNorm >= 0.05 within StuckTimeSeconds (2.0s), it terminates as Stuck. This occurs even during the 2.5s grace window and during the forced-forward diagnostic window."

MVP-04 confirms the fix: "`Stuck`: timer accumulates and fires only when neither grace nor forced-forward diagnostic window is active. Timer reset during suppressed windows."

### Test 2: Multi-agent, forced-forward ON

**Before fixes:**
- All N agents at `SpawnDistanceAlongTrackCm = 0` → physics overlap within first frame (MVP-02 finding)
- Rays from each agent hit neighboring car bodies → low RayForward value → `CollisionTerminalThreshold (0.05)` fires → `Collision` terminal (MVP-03 finding)
- t<0.5s: synchronized `Collision` burst across all agents before diagnostic window has any effect

**After fixes (MVP-02 + MVP-03 + MVP-04):**
- Agent 0 at 0 m, agent 1 at 5 m, ..., agent 9 at 45 m → no physical overlap (`AgentSpawnStaggerCm = 500 cm`)
- Ray traces ignore all other registered agent bodies → only static track geometry is visible to each agent
- Stuck and AirborneLong suppressed during grace window
- Expected outcome: agents distributed over 45 m of track; forced-forward drives each forward until real obstacle or grace expiry

**Supporting evidence from MVP-02:**
"With N agents, all N cluster at track distance 0 with only ±300 cm lateral scatter. On a track narrower than 600 cm, agents physically overlap at spawn → immediate collision."

**Supporting evidence from MVP-03:**
"In a 10-agent batch, the forward ray from Agent A could hit Agent B's body (a physics mesh directly in front). This would return a low normalized distance (e.g., 0.03) → triggers CollisionTerminalThreshold (0.05) → Agent A terminates as Collision even though no wall/track geometry was hit."

### Test 3: Single agent, normal policy (GenomeID=-1 or random NEAT)

**Before fixes:**
- Random initial NEAT genome may output full brake or backward steer → SpeedNorm ≈ 0 → stuck timer fires at t=2.0s
- Episode duration consistently 2.0s for any bad initial policy
- Grace window provides no protection for this failure path

**After fixes (MVP-04):**
- Stuck timer does not start accumulating until grace window (2.5s) has elapsed
- NEAT genome has the full grace window to produce non-zero forward motion
- Bad genomes that produce zero speed throughout will still fail, but only after 2.5s minimum (grace end) plus the stuck accumulation time (2.0s) = at earliest t=4.5s
- Expected outcome: episodes last at minimum 2.5s (grace duration) regardless of policy quality; genuinely bad policies terminate at ≥4.5s rather than ≤2.0s

### Test 4: Multi-agent, normal policy

**Before fixes:**
- Simultaneous spawn overlap (all agents at distance=0) → immediate physics collision within first frame
- Trace contamination → false `Collision` from neighboring car body in forward ray
- Stuck grace missing → all agents accumulate stuck timer from t=0 and terminate at t≈2.0s
- Three bugs compound: result is a synchronized burst of `Stuck` and `Collision` terminals clustered in the interval t<2.5s

**After fixes (MVP-02 + MVP-03 + MVP-04):**
- Staggered spawn eliminates physics overlap at t=0
- Clean ray traces eliminate false collision terminals from neighboring bodies
- Grace-protected stuck timer prevents premature 2.0s Stuck termination
- Bad population: agents now fail at different times (after grace, from genuine stuck or real collision) and for different reasons distributed across their individual episode histories
- Synchronized failure burst at t<2.5s: eliminated by construction

## Runtime Validation Procedure

To generate runtime evidence, run PIE with these steps:

1. Open `Content/Map_LittleRamps.umap` or `Plugins/Tracks/Content/Maps/Map_LittleRamp.umap`.
2. Place 1 agent with `BP_CarPawn` + `URacingAgentComponent`:
   - `bEnableLogging = true`
   - `bEnableForcedForwardDiagnostic = true`
   - `bDrawRayDebug = true`
3. Enter PIE. Open Output Log.
4. Search for `Config dump:` — verify runtime values match C++ defaults from MVP-06 table. Any difference indicates a Blueprint override.
5. Search for `FORCED-FORWARD DIAGNOSTIC ACTIVE` — verify progress in cm and RayGroundDist appear each step during the 0–2.0s window.
6. Search for `Episode completed:` — record terminal reason and elapsed time.
7. Verify `Stuck` does NOT appear before t=2.5s (grace duration).
8. Verify `AirborneLong` does NOT appear before t=2.5s (grace duration).

For **Test 2 / Test 4 (multi-agent):** Place 8–10 agents in the level. Enter PIE. Search for `[NEATTrainingManager] Agent` lines to confirm distinct `SpawnDistanceCm` values per agent (expected: 0, 500, 1000, 1500, ... cm). With `bDrawRayDebug=true`, verify ignored actor count in ray trace logs equals own car + (N-1) other agents.

## Metrics (to fill in from runtime)

| Metric | Before Fixes (code prediction) | After Fixes (target) | Actual (fill from PIE) |
|---|---|---|---|
| Single agent min episode duration | ~2.0s (stuck timer fires without grace) | ≥2.5s (grace suppresses stuck timer) | TBD |
| Multi-agent sync failure at t<2s | All agents terminate in synchronized burst | 0 agents terminate at t<2.5s from grace-bypassed Stuck | TBD |
| False `Collision` from trace contamination | High (1 false terminal per neighboring car in forward ray) | 0 (all agent bodies in ignore list) | TBD |
| Agents with distinct spawn positions | 0/N (all at SplineDistance=0) | N/N (staggered 500 cm apart) | TBD |
| AirborneLong from spawn bounce | Possible within 3s grace window | Eliminated (timer reset during grace) | TBD |

## Gate Assessment

**Code-level gate (current):**

The earlier pathological patterns are reduced by construction:

- **Spawn overlap** (MVP-02): eliminated — `AgentSpawnStaggerCm = 500 cm` ensures no two agents share a spawn position.
- **Trace contamination** (MVP-03): eliminated — `BuildTraceIgnoreParams()` includes all registered agent vehicles in every ray's ignore list.
- **Stuck grace bypass** (MVP-04): eliminated — `CheckTerminalConditions()` now checks `EpisodeGraceTimeRemaining` and `bEnableForcedForwardDiagnostic` before accumulating the stuck timer.
- **AirborneLong grace bypass** (MVP-04): eliminated — airborne timer resets during grace window.
- **Config drift invisibility** (MVP-06): resolved — config dump log fires on every episode reset.
- **Ground ray semantic** (MVP-05): confirmed correct — no mismatch, no fix required.

Remaining failures after fixes are classified as **genuine gameplay failures**: bad NEAT genome producing zero speed after grace expires → stuck after 4.5s minimum; real track geometry collision; out-of-bounds; MaxSteps. These are the intended training signal.

The synchronized early failure burst (all agents terminating at t≈2s) is eliminated by design.

**Runtime evidence required** to fully close the gate: PIE run per procedure above. Metrics table to be updated with observed values.
