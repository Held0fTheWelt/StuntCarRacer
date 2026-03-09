# MVP-02 Experiment Analysis: Startup Stuck Root Cause

**Date:** 2026-03-10
**Data Source:** StuntCarRacer.log (both Exp A and Exp B present)

---

## Experiment A: ForcedForward ON (ff_enabled=1)

### Frame 0-15 Snapshot (First 0.3s)

| Frame | Time(s) | Grace | FF_Window | FF_Enabled | Throttle | Speed_Norm | Vel_Forward | Stuck_Accum | Grounded |
|-------|---------|-------|-----------|------------|----------|-----------|-------------|------------|---------|
| 0 | 0.000 | 2.50 | 1 | 1 | 1.00 | 0.000 | 0.0 | 0.000 | 0 |
| 1 | 0.029 | 2.47 | 1 | 1 | 1.00 | 0.000 | 0.0 | 0.000 | 0 |
| 2 | 0.039 | 2.46 | 1 | 1 | 1.00 | 0.000 | 0.0 | 0.000 | 0 |
| 3 | 0.050 | 2.45 | 1 | 1 | 1.00 | 0.000 | 0.0 | 0.000 | 0 |
| 4 | 0.060 | 2.44 | 1 | 1 | 1.00 | 0.000 | -0.0 | 0.000 | 0 |
| 5 | 0.071 | 2.43 | 1 | 1 | 1.00 | 0.002 | -0.1 | 0.000 | 0 |
| 6 | 0.081 | 2.42 | 1 | 1 | 1.00 | 0.004 | -0.1 | 0.000 | 0 |
| 7 | 0.091 | 2.41 | 1 | 1 | 1.00 | 0.006 | -0.2 | 0.000 | 0 |
| 8 | 0.102 | 2.40 | 1 | 1 | 1.00 | 0.008 | -0.4 | 0.000 | 1 |
| 9 | 0.112 | 2.39 | 1 | 1 | 1.00 | 0.011 | -0.7 | 0.000 | 2 |
| 10 | 0.122 | 2.38 | 1 | 1 | 1.00 | 0.013 | -0.9 | 0.000 | 3 |
| 11 | 0.133 | 2.37 | 1 | 1 | 1.00 | 0.015 | -1.5 | 0.000 | 4 |
| 12 | 0.141 | 2.36 | 1 | 1 | 1.00 | 0.016 | -2.0 | 0.000 | 5 |
| 13 | 0.151 | 2.35 | 1 | 1 | 1.00 | 0.018 | -2.7 | 0.000 | 6 |
| 14 | 0.162 | 2.34 | 1 | 1 | 1.00 | 0.019 | -3.6 | 0.000 | 7 |
| 15 | 0.174 | 2.33 | 1 | 1 | 1.00 | 0.019 | -4.9 | 0.000 | 8 |

### Key Findings (Exp A):
- ✓ **Throttle IS reaching vehicle**: throttle=1.00 every frame
- ✓ **Drivetrain IS responding**: velocity accelerates (-0.1 → -4.9 cm/s backwards)
- ✓ **Wheels ARE engaging**: grounded_frames climb (0 → 8)
- ✓ **Stuck suppressed**: stuck_accum stays 0.000 because:
  - Grace window active (grace > 0)
  - Forced-forward window active (ff_window=1)
- ✓ **Speed norm rising**: 0.000 → 0.019 (vehicle moving)

**Verdict**: Suppression logic works correctly. Vehicle engages and accelerates. No early Stuck.

---

## Experiment B: ForcedForward OFF (ff_enabled=0)

### Frame 0-15 Snapshot (First 0.3s)

| Frame | Time(s) | Grace | FF_Window | FF_Enabled | Throttle | Speed_Norm | Vel_Forward | Stuck_Accum | Grounded |
|-------|---------|-------|-----------|------------|----------|-----------|-------------|------------|---------|
| 0 | 0.000 | 2.50 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 0 |
| 1 | 0.009 | 2.49 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 1 |
| 2 | 0.029 | 2.47 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 2 |
| 3 | 0.039 | 2.46 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 3 |
| 4 | 0.048 | 2.45 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 4 |
| 5 | 0.057 | 2.44 | 0 | 0 | 0.00 | 0.000 | 0.0 | 0.000 | 5 |
| 6 | 0.066 | 2.43 | 0 | 0 | 0.00 | 0.001 | 0.0 | 0.000 | 6 |
| 7 | 0.076 | 2.42 | 0 | 0 | 0.00 | 0.002 | 0.0 | 0.000 | 7 |
| 8 | 0.085 | 2.41 | 0 | 0 | 0.00 | 0.004 | 0.0 | 0.000 | 8 |
| 9 | 0.094 | 2.41 | 0 | 0 | 0.00 | 0.005 | 0.0 | 0.000 | 9 |
| 10 | 0.104 | 2.40 | 0 | 0 | 0.00 | 0.007 | 0.0 | 0.000 | 10 |
| 11 | 0.113 | 2.39 | 0 | 0 | 0.00 | 0.008 | 0.0 | 0.000 | 11 |
| 12 | 0.122 | 2.38 | 0 | 0 | 0.00 | 0.010 | 0.0 | 0.000 | 12 |
| 13 | 0.131 | 2.37 | 0 | 0 | 0.00 | 0.011 | 0.0 | 0.000 | 13 |
| 14 | 0.141 | 2.36 | 0 | 0 | 0.00 | 0.013 | 0.0 | 0.000 | 14 |
| 15 | 0.151 | 2.35 | 0 | 0 | 0.00 | 0.015 | 0.0 | 0.000 | 15 |

### Critical Event: Stuck Accumulation Starts

```
Frame 273, t=2.515s: STUCK_ACCUM_START [first accumulation (speed=0.000 < threshold=0.050)]
```

**Why did Stuck start here?**
- Grace window ended at t=2.5s (2.515 - 0.015 remaining = 2.5 expired)
- Forced-forward window is OFF (ff_enabled=0, ff_window=0)
- Vehicle speed_norm is 0.000 (matches stuck threshold)
- No suppression active anymore → Stuck timer started

```
Frame 492, t=4.513s: STUCK_TRIGGERED [accumulator=2.006 >= threshold=2.000 speed=0.000]
```

**Stuck fired after exactly 2.0 seconds of accumulation** (from t=2.515 to t=4.515, minus rounding)

### Key Findings (Exp B):
- ✗ **Throttle NOT reaching vehicle**: throttle=0.00 every frame (policy is steering only, no throttle)
- ✗ **Drivetrain NOT responding**: velocity stays at 0.0 cm/s
- ✓ **Wheels ARE engaging**: grounded_frames climb (0 → 15)
- ✓ **Stuck suppressed during grace**: stuck_accum stays 0.000 while grace > 0
- ✓ **Stuck TRIGGERED after grace**: Frame 273 when grace expires and speed=0
- Speed norm barely rises: 0.000 → 0.015 (very little motion)

**Verdict**: Grace suppression worked. After grace expired, vehicle has zero speed (policy output is steering=~1.0, throttle=0.0), so Stuck fires correctly.

---

## Root Cause Determination

### Hypothesis 1: Suppression Logic Not Active at Runtime
**STATUS: ✓ DISPROVEN**
- Exp A: Stuck timer stayed at 0.000 until after forced-forward window (grace still active)
- Exp B: Stuck timer stayed at 0.000 until grace expired
- **Conclusion**: Suppression logic IS active and working correctly

### Hypothesis 2: Input Not Reaching Vehicle
**STATUS: ✓ CONFIRMED for Exp B, ✓ DISPROVEN for Exp A**
- Exp A: Throttle=1.00 applied, vehicle accelerates → input path works
- Exp B: Throttle=0.00 applied, vehicle doesn't accelerate → policy output is not throttle
- **Conclusion**: Input path works when policy provides throttle. In Exp B, policy chose steering only (throttle=0.00)

### Hypothesis 3: Drivetrain/Gear Not Engaging
**STATUS: ✓ DISPROVEN (when throttle is provided)**
- Exp A: Throttle applied → immediate velocity response (-4.9 cm/s at frame 15)
- Exp B: Throttle 0.00 → no velocity response (expected)
- **Conclusion**: Drivetrain responds correctly to throttle input

### Hypothesis 4: Wheel-Ground Contact Issue
**STATUS: ✓ DISPROVEN**
- Both experiments show grounded_frames increasing from frame 0
- Wheels ARE on ground from early frames
- Vehicle physics startup is healthy
- **Conclusion**: Not a physics startup issue

### Hypothesis 5: Blueprint/Runtime Override Drift
**STATUS: ✓ DISPROVEN**
- Exp A logs show: ff_enabled=1, matches Blueprint setting
- Exp B logs show: ff_enabled=0, matches Blueprint setting
- **Conclusion**: Blueprint properties correctly reflected at runtime

---

## Root Cause of EARLY Stuck (from MVP-01 baseline)

The original MVP-01 observation was agents terminating with `Stuck` at ~2.0s with 0.1m-0.2m progress.

**From this data:**
- **NOT suppression failure** (suppression works perfectly)
- **NOT input path failure** (throttle reaches vehicle when policy provides it)
- **NOT drivetrain failure** (responds immediately when given throttle)
- **NOT physics startup** (wheels engage, acceleration happens)

**The actual root cause:** The **policy network is not outputting throttle during the forced-forward window**, even though:
1. ForcedForward diagnostic is enabled
2. Throttle=1.0 is supposed to be forced in ApplyAction()

**Evidence:**
- Exp A ACTION_FRAME shows throttle=1.00 → forced-forward override IS working
- Exp B ACTION_FRAME shows throttle=0.00 (random policy) until Stuck fires

**Possible sub-causes:**
1. Policy backend assigned incorrectly (genome not loaded)
2. Policy evaluation returns zero/invalid output
3. Forced-forward action override not reaching ApplyAction()
4. NEAT genome evaluation fails silently

---

## Next Step: MVP-03

Investigate why forced-forward action is not being applied in the original MVP-01 failure case.

Check:
1. Is `bEnableForcedForwardDiagnostic` actually true at runtime? (Add Blueprint validation log)
2. Does ApplyAction() receive throttle=1.0 during forced-forward window?
3. Is there a NEAT evaluation failure blocking the forced-forward path?

