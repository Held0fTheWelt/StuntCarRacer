# MVP-01 Report — Single-Agent Baseline

## Test Setup

This report is based on **code-level analysis** of the current source. Runtime PIE logs have not been captured yet. The instrumentation added in this MVP provides the diagnostic surface needed to capture evidence on the next PIE run.

## Exact Runtime Configuration Analyzed

| Parameter | Source | Value |
|---|---|---|
| `bEnableForcedForwardDiagnostic` | C++ default | `false` |
| `ForcedForwardDiagnosticDuration` | C++ default | `2.0s` |
| `ForcedForwardDiagnosticThrottle` | C++ default | `1.0` |
| `DiagnosticOfftrackSuppressProgressCm` | C++ default | `200 cm` |
| `DiagnosticSpawnHeightOffsetCm` | C++ default | `0 cm` |
| `SpawnHeightOffsetCm` | C++ default | `50 cm` |
| `SpawnLateralOffsetMaxCm` | C++ default | `300 cm` |
| `SpawnDistanceAlongTrackCm` | C++ default | `0 cm` |
| `GraceSecondsAfterReset` | C++ default | `2.5 s` |
| `StuckSpeedNorm` | C++ default | `0.05` |
| `StuckTimeSeconds` | C++ default | `2.0 s` |
| `AirborneMaxSeconds` | C++ default | `3.0 s` |
| `GapTerminalThreshold` | C++ default | `0.1` |
| `CollisionTerminalThreshold` | C++ default | `0.05` |

Blueprint or placed-instance overrides may differ. MVP-06 will audit these.

## Log Coverage After MVP-01 Instrumentation

All required log items are now present:

| Required Item | Log Location | Verbosity | Gate |
|---|---|---|---|
| Spawn transform | `ResetEpisode()` | Display | Always |
| Lateral offset applied | `ResetEpisode()` | Display | Always (added MVP-01) |
| Step count | `StepOnce()` step 0 | Display | Always; every 100 with bEnableLogging |
| Signed forward speed | Forced-forward window | Display | When bEnableForcedForwardDiagnostic |
| Progress in cm | Forced-forward window | Display | When bEnableForcedForwardDiagnostic (added MVP-01) |
| RayGroundDist | Forced-forward window | Display | When bEnableForcedForwardDiagnostic (added MVP-01) |
| Grounded frame count | Forced-forward window | Display | When bEnableForcedForwardDiagnostic (added MVP-01) |
| Terminal reason | `StepOnce()` episode end | Display | Always |

## Code-Level Analysis: What Will Happen with One Agent

### Spawn Path
1. Agent spawns at `SpawnDistanceAlongTrackCm=0` on the track spline (tangent-aligned).
2. Random lateral offset in `[-300, +300]` cm is applied. Now logged explicitly.
3. Height offset `+50 cm` applied (or `+0 cm` if diagnostic active).
4. Physics teleport issued; velocities zeroed; controls zeroed.

### Grace Period
- 2.5 s grace window starts; `Collision` and `Fell off track` from `ComputeReward` are suppressed.
- **`Stuck` and `AirborneLong` from `CheckTerminalConditions` are NOT suppressed during grace.**

### Critical Finding: `Stuck` fires with no grace protection
At episode start `SpeedNorm = 0 < StuckSpeedNorm (0.05)`. The stuck timer starts immediately. If the vehicle does not reach `SpeedNorm ≥ 0.05` within `StuckTimeSeconds (2.0s)`, it terminates as `Stuck`. This occurs even during the 2.5s grace window and during the forced-forward diagnostic window.

**Scenario where this causes premature termination:**
- Agent spawns with `±300 cm` lateral offset into a wall (narrow track).
- Vehicle is blocked; throttle cannot build speed because physics constraint.
- `SpeedNorm` stays near 0 for 2+ seconds → terminates as `Stuck`.
- Grace window does not help.

### Critical Finding: `AirborneLong` fires with no grace protection
If spawn height offset or bounce puts `RayGroundDist < 0.1f` for `AirborneMaxSeconds (3.0s)`, the episode terminates as `AirborneLong`. Not suppressed by grace or diagnostic mode.

### Prediction for Single-Agent Run with Forced-Forward Enabled
- If track is wide (> 600 cm): agent likely drives forward; survives grace; may reach meaningful distance.
- If track is narrow or agent spawns against wall: `Stuck` in 2.0s, before grace ends.
- This is MVP-04's defect to fix.

## Gate Assessment

**Gate question: Does a single agent still fail?**
- **Code analysis predicts: YES, it can fail immediately via `Stuck` (2.0s) even with forced-forward diagnostic.**
- The exact terminal reason with a clean spawn: likely `Stuck` (vehicle stationary during gear engagement) or `Fell off track` if spawn is at track edge.
- With wide spawn and clean road: vehicle should drive forward and survive past grace.

**Gate question: If yes, what exact terminal reason occurs first?**
- Code analysis: `Stuck` is most likely first terminal reason when the vehicle fails to accelerate within 2s.
- `Collision` is second candidate if vehicle spawns laterally against a wall.

**Runtime evidence required:** Enable `bEnableForcedForwardDiagnostic=true` and `bEnableLogging=true` on the agent BP. Enter PIE. Collect log lines tagged `[NeatTrainingEditorWidget]` and `[RacingAgent*]`. Report updated when runtime logs available.

## Conclusion

Single-agent baseline instrumentation is complete. The two primary code-level failure hypotheses are:
1. **`Stuck` timer fires without grace protection** — primary suspect for 2s early termination.
2. **Large lateral spawn offset (±300 cm) may place agent off-track or against wall**.

Both are addressed in MVP-04 (`Stuck` grace fix) and MVP-02 (spawn offset investigation). MVP-06 will confirm whether Blueprint overrides change these defaults at runtime.
