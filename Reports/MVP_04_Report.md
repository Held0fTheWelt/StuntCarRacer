# MVP-04 Report — Terminal Condition Pipeline Audit

## Terminal Condition Matrix

| Terminal | Trigger | Grace-suppressed? | Diagnostic-suppressed? | Issue Before Fix |
|---|---|---|---|---|
| `Collision` | `MinRayDist < 0.05` in `ComputeReward()` | YES | NO | None |
| `Fell off track` | `RayGroundDist < 0.1` in `ComputeReward()` | YES | YES (progress + grounded frames) | None |
| `Stuck` | `SpeedNorm < 0.05` for 2s in `CheckTerminalConditions()` | **WAS NO** → **NOW YES** | **WAS NO** → **NOW YES** | Fires during 2.5s grace; fires during forced-forward window |
| `AirborneLong` | `RayGroundDist < 0.1` for 3s in `CheckTerminalConditions()` | **WAS NO** → **NOW YES** | N/A | Fires during grace if spawn bounce |
| `MaxSteps` | `StepCount >= 5000` | No (intentional) | No (intentional) | None |

## Mismatch Found

`Stuck` and `AirborneLong` were checked in `CheckTerminalConditions()` which had no awareness of:
- `EpisodeGraceTimeRemaining` — the post-reset stabilization window
- `bEnableForcedForwardDiagnostic` + `ForcedForwardDiagnosticDuration` — the diagnostic phase

This meant:
1. At t=0, `SpeedNorm = 0 < 0.05` → stuck timer starts.
2. Vehicle may not accelerate within 2s (due to spawn interference, gear lag, or lateral collision).
3. Episode terminates `Stuck` at t=2.0s, inside the 2.5s grace window.
4. Grace window was meaningless for this failure path.

## Fix Applied

`CheckTerminalConditions()` now:
- **`AirborneLong`**: timer accumulates and fires only when `EpisodeGraceTimeRemaining <= 0`. Timer reset during grace.
- **`Stuck`**: timer accumulates and fires only when neither grace nor forced-forward diagnostic window is active. Timer reset during suppressed windows.

`MaxSteps` is intentionally not suppressed — it is a hard session timeout.

## Proof

Config log at step 0 (Verbose level) shows all thresholds for offline verification. After fix:
- Stuck termination at t < 2.5s: eliminated.
- Stuck termination at t < ForcedForwardDiagnosticDuration: eliminated.
- Only genuine stuck behavior (vehicle stationary after full grace + diagnostic) triggers termination.

## Runtime Evidence Needed

Run with `bEnableLogging=true` + forced-forward enabled. Observe that no `Stuck` termination appears before `GraceSecondsAfterReset (2.5s)` elapses. If vehicle is truly stuck after grace ends, `Stuck` should still fire.
