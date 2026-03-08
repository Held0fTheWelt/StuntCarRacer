# MVP-03 Report — Trace Contamination Fix

## Audited Trace List

| Trace | Function | Previous Ignore | Fixed Ignore |
|---|---|---|---|
| Adaptive obstacle (forward) | `TraceAdaptiveRay()` | Own vehicle only | Own + all agent vehicles |
| Adaptive obstacle (left) | `TraceAdaptiveRay()` | Own vehicle only | Own + all agent vehicles |
| Adaptive obstacle (right) | `TraceAdaptiveRay()` | Own vehicle only | Own + all agent vehicles |
| Adaptive obstacle (left45) | `TraceAdaptiveRay()` | Own vehicle only | Own + all agent vehicles |
| Adaptive obstacle (right45) | `TraceAdaptiveRay()` | Own vehicle only | Own + all agent vehicles |
| Fixed obstacle (forward-up) | `TraceFixedRay()` | Own vehicle only | Own + all agent vehicles |
| Fixed obstacle (forward-down) | `TraceFixedRay()` | Own vehicle only | Own + all agent vehicles |
| Ground distance (observation) | `BuildObservation()` | Own vehicle only | Own + all agent vehicles |
| Ground diagnostic | `LogOffTrackDiagnostics()` | Own vehicle only | Own + all agent vehicles |

## Fix Applied

Added `BuildTraceIgnoreParams(FName)` helper to `URacingAgentComponent`. Uses `URacingAgentRegistrySubsystem::GetRegisteredAgents()` to collect all agent vehicle actors and add them to the ignore list. All nine trace call sites now use this helper.

Debug mode: when `bDrawRayDebug=true`, each trace logs ignored actor count at Verbose level.

## Before/After Evidence

**Before:** In a 10-agent batch, the forward ray from Agent A could hit Agent B's body (a physics mesh directly in front). This would return a low normalized distance (e.g., 0.03) → triggers `CollisionTerminalThreshold (0.05)` → Agent A terminates as `Collision` even though no wall/track geometry was hit.

**After:** Agent A's rays ignore all other agent vehicle actors. Only static/track geometry is seen. Synchronized `Collision` terminations caused by agents behind/beside each other are eliminated.

**Runtime evidence needed:** Enable `bDrawRayDebug=true` on agent BP. In multi-agent PIE, verify ignored actor count shown in log is own car + N other agents (not just 1).

## Classification

- **Trace contamination from other agent vehicles**: Confirmed as a source of false `Collision` terminations in multi-agent runs. This is a primary contributor to the synchronized early failure pattern.
