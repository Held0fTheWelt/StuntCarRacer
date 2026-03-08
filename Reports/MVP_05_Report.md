# MVP-05 Report — Ground Ray / Offtrack Semantics

## Semantic Contract (verified from source)

**`RayGroundDist` semantic:** `1.0 = ground very close (safe); 0.0 = no ground or ground far away (airborne/danger)`

**Computation (`BuildObservation()`, line 891–909):**
```
bHasGround → RayGroundDist = 1.0 - (GroundDistCm / GroundRayMaxDistanceCm)
!bHasGround → RayGroundDist = 0.0
```

**Ray parameters:** starts at `Vehicle->GetActorLocation()`, straight down, `GroundRayMaxDistanceCm = 500 cm` (default). Channel: `ECC_Visibility`.

The inversion is explicitly documented in source with a comment at line 889–890:
> `RayGroundDist semantics: downstream treats high = grounded/safe, low = offtrack/airborne/danger.`
> `Raw ray gives: small distance = ground close (on track), large distance = ground far (airborne). Invert so 1 = safe, 0 = danger.`

## Consumer Consistency Check

| Consumer | Condition | Interpretation | Consistent? |
|---|---|---|---|
| `ComputeReward()` GapWarning | `RayGroundDist < 0.3` | Low = danger | Yes |
| `ComputeReward()` GapTerminal | `RayGroundDist < 0.1` | Low = danger | Yes |
| `CheckTerminalConditions()` Airborne | `RayGroundDist < 0.1` | Low = airborne | Yes |
| `StepOnce()` GroundedFrameCount | `RayGroundDist >= 0.15` | High = grounded | Yes |
| Diagnostic suppress | `GroundedFrameCount < 30` | Low count = not settled | Yes |

All consumers interpret high = safe, low = danger. **No semantic mismatch.**

## On-Track False Positive Analysis

At normal on-track driving height (50–80 cm CoM above surface):
- `GroundDistCm ≈ 60 cm`
- `RayGroundDist = 1 - 60/500 = 0.88`
- `GapTerminalThreshold = 0.1` → fires only when vehicle is 450+ cm above nearest ground

**False positive risk: VERY LOW.** The threshold is intentionally conservative. Vehicle must be genuinely airborne (or over a very large gap) to trigger offtrack from ground ray.

**Potential edge case:** Track with large gap under bridge — ray passes through bridge floor if floor is thin or not tracked by `ECC_Visibility`. This is environment-specific and does not represent a semantic error.

## Evidence from Code

Verbose log confirmed present in `BuildObservation()` for steps 0–4 (when `bEnableLogging`), at lines 896–908:

- Hit path: `"Ground ray: hit=yes raw_cm=X norm_dist=Y RayGroundDist=Z (1=grounded)"`
- Miss path: `"Ground ray: hit=no RayGroundDist=0 (danger)"`

This provides runtime ground truth when `bEnableLogging=true`.

The `FRacingObservation` struct in `RacingAgentTypes.h` also documents the semantic inline:
> `"Ground ray (straight down): 1 = grounded/safe, 0 = no ground or far/danger. Used for gap/offtrack and airborne checks."`

## Gate Assessment

- Runtime meaning of `RayGroundDist`: **PROVEN from source** — 1=grounded/safe, 0=danger.
- All consumers interpret consistently: **YES**.
- On-track ground proximity causes false offtrack: **NO** — threshold requires 450 cm altitude.

## Classification

- Ground ray semantic mismatch: **DISPROVEN**. Contract is correct and consistent across all consumers.
- No code change required. This hypothesis is closed.
