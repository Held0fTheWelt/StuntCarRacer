# MVP-06 Report — Runtime Config Drift Audit

## Source Defaults vs Runtime

The following table documents C++ defaults for all properties relevant to training stability. Blueprint defaults or placed-instance overrides may differ — the config dump log (added in MVP-06) prints effective runtime values at each episode reset.

| Property | C++ Default | Location | Notes |
|---|---|---|---|
| `GraceSecondsAfterReset` | `2.5` | `FRacingRewardConfig` | Post-reset collision/offtrack suppress |
| `bEnableForcedForwardDiagnostic` | `false` | `URacingAgentComponent` | Must be enabled in BP for diagnosis |
| `ForcedForwardDiagnosticDuration` | `2.0` s | `URacingAgentComponent` | Forced throttle window |
| `ForcedForwardDiagnosticThrottle` | `1.0` | `URacingAgentComponent` | Full throttle during diagnostic |
| `DiagnosticOfftrackSuppressProgressCm` | `200` cm | `URacingAgentComponent` | Min progress before offtrack can fire (diagnostic) |
| `DiagnosticSpawnHeightOffsetCm` | `0` cm | `URacingAgentComponent` | Height override during diagnostic |
| `SpawnHeightOffsetCm` | `50` cm | `URacingAgentComponent` | Normal height offset |
| `SpawnLateralOffsetMaxCm` | `300` cm | `URacingAgentComponent` | Max lateral jitter (±300 cm) |
| `SpawnDistanceAlongTrackCm` | `0` cm | `URacingAgentComponent` | Now set per-agent by manager (MVP-02 fix) |
| `StuckSpeedNorm` | `0.05` | `FRacingRewardConfig` | ~22.5 cm/s at SpeedNormCmPerSec=4500 |
| `StuckTimeSeconds` | `2.0` s | `FRacingRewardConfig` | Now grace-protected (MVP-04 fix) |
| `AirborneMaxSeconds` | `3.0` s | `FRacingRewardConfig` | Now grace-protected (MVP-04 fix) |
| `GapTerminalThreshold` | `0.1` | `FRacingRewardConfig` | RayGroundDist < 0.1 = offtrack |
| `CollisionTerminalThreshold` | `0.05` | `FRacingRewardConfig` | MinRayDist < 0.05 = collision |
| `MaxEpisodeSteps` | `5000` | `FRacingRewardConfig` | Hard step limit |
| `AgentSpawnStaggerCm` | `500` cm | `UNEATTrainingManager` | Per-agent spline stagger (MVP-02 addition) |

## Override Winner

The effective value priority chain (highest wins):
1. Placed-instance property override in level editor
2. Blueprint class default
3. C++ UPROPERTY default

**Risk:** Blueprint class defaults are not visible from C++ source alone. The config dump log added in MVP-06 prints effective values at runtime, making overrides detectable in Output Log.

## Actions Taken

- Added config dump log in `ResetEpisode()` at Display level. Fires once per episode reset. Log tag: `[AgentID] Config dump:`.
- No C++ defaults changed (correctness is verified by reading values, not by assuming defaults match).

## Gate Assessment

**Effective runtime values logged:** YES — config dump fires on every episode reset.

**Discrepancy between source and runtime:** UNDETECTABLE without a runtime log; the config dump added here makes it detectable. Blueprint overrides must be compared against the config dump log in PIE.

**Actions to take in PIE:** Run any agent. Search Output Log for `Config dump:` line. Compare to C++ defaults table above. Any difference indicates Blueprint override.
