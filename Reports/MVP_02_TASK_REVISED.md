# MVP-02 Revised: Startup Stuck Root Cause — Two Explicit Experiments

**Status:** EVIDENCE-GATHERING PHASE ONLY
**Prerequisite:** Code rebuilt with enhanced diagnostics (commit fb255d8)
**Objective:** Prove root cause, NOT fix

---

## Why Two Experiments?

Early Stuck termination could come from five possible root causes:
1. **Suppression logic not actually active at runtime** (grace/diagnostic windows missing)
2. **Input not reaching vehicle** (throttle action lost in path)
3. **Drivetrain/gear not engaging** (throttle applied but engine doesn't respond)
4. **Wheel-ground contact issue** (wheels not touching ground, physics startup)
5. **Blueprint/runtime override drift** (property mismatch between declared and actual)

**Experiment A (ForcedForward ON)** and **Experiment B (ForcedForward OFF)** will isolate whether the problem is:
- Suppression logic (compare timing of Stuck across both)
- Vehicle physics/drivetrain (same vehicle, different suppression should show difference)
- Blueprint override (verify Blueprint state matches runtime logs)

---

## Setup (Identical for Both Experiments)

### 1. Rebuild C++ with Enhanced Diagnostics

```
Visual Studio: Build > Build StuntCarRacerEditor (Development, Win64)
OR
Unreal Editor: Ctrl+Alt+F11 (Live Coding)
```

Wait for compile to complete. Check Output Log for "Compile Successful".

### 2. Test Scene Configuration

Use **Content/Map_LittleRamps** with:
- **Single agent only** (one Test Vehicle with BP_RacingAgentComponent attached)
- **Population size = 1** (in EUW_NeatTraining widget Details)
- **Same spawn location** (no randomization; use default PlayerStart)
- **No other agents or obstacles**

### 3. Verify Blueprint State (Before Each Experiment)

Select the vehicle's BP_RacingAgentComponent in the editor:
- Expand Details panel
- Find `bEnableForcedForwardDiagnostic` property
- **Do NOT change it between Experiments** — only compare the two fixed states

---

## Experiment A: ForcedForward ON

### Prerequisites
- Blueprint: `bEnableForcedForwardDiagnostic = true`
- **Do not change any other settings**
- Do not recompile Blueprint (just use as-is)

### Run Procedure

1. Open **Output Log** (Windows → Developer Tools → Output Log)
2. In EUW_NeatTraining widget: Click **Start Training**
3. Click **Play** (PIE start)
4. **Wait for episode to terminate** (will end with Stuck, typically at t~2s)
5. **Stop PIE** (click Stop button)
6. Copy all logs from Output Log to a file: **Reports/Exp_A_ForcedForward_ON.log**

### What to Capture

Search the log for these patterns and note **all occurrences**:

```
STARTUP_FRAME [N]
ACTION_FRAME [N]
STUCK_ACCUM_START [N]
STUCK_ACCUM [N]
STUCK_SUPPRESSED [N]
STUCK_TRIGGERED [N]
```

Record in a spreadsheet:

| Frame | Time | Grace | FF_Window | FF_Enabled | Throttle | Speed_Norm | Vel_Forward | Stuck_Accum | Grounded | Note |
|-------|------|-------|-----------|------------|----------|-----------|-------------|------------|---------|------|
| 1 | 0.000 | 2.500 | 1 | 1 | ? | ? | ? | 0.000 | ? | Start |
| 2 | 0.012 | 2.488 | 1 | 1 | ? | ? | ? | 0.012 | ? | Accum? |
| ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... |
| N | 2.0 | 0.0 | 0 | 1 | 1.0 | 0.05 | 0 | X.XX | Y | **STUCK!** |

### Success for Exp A

Capture at least one complete episode run (from Start to Stuck termination).

---

## Experiment B: ForcedForward OFF

### Prerequisites
- Blueprint: `bEnableForcedForwardDiagnostic = false`
- **All other settings identical to Experiment A**
- Same vehicle, same agent, same spawn location
- **Do not change any other settings**

### Run Procedure

**Identical to Experiment A:**

1. Open **Output Log**
2. In EUW: Click **Start Training**
3. Click **Play**
4. **Wait for episode to terminate** (will terminate, may be Stuck or may be different reason)
5. **Stop PIE**
6. Copy logs to file: **Reports/Exp_B_ForcedForward_OFF.log**

### Success for Exp B

Capture complete episode run.

---

## Evidence Analysis (After Both Experiments)

**Do not analyze yet.** Just run both experiments and save the logs.

Once you have both log files, compare them on these dimensions:

### Question 1: Is Suppression Logic Active?

**Evidence from logs:**
- Exp A: Does `STUCK_SUPPRESSED` appear when `grace > 0` or `ff_window = 1`?
- Exp B: When does `STUCK_ACCUM_START` appear? (should be immediate, no suppression)

**Finding:** If Suppression works, Exp A should show no Stuck until grace expires.

### Question 2: Is Throttle Reaching the Vehicle?

**Evidence from logs:**
- Do both experiments show `ACTION_FRAME` with `throttle=1.0` in frame 1-5?
- If yes: input path works
- If no: input not reaching vehicle

### Question 3: Is Drivetrain Responding?

**Evidence from logs:**
- `Vel_Forward` progression frame 1-20:
  - Exp A: Should show acceleration (vel increasing) if throttle works
  - Exp B: May show no acceleration if no throttle input
- `Speed_Norm` progression: Should rise if vehicle accelerating

### Question 4: Is Wheel-Ground Contact the Issue?

**Evidence from logs:**
- `Grounded_Frames` value: should increase over time if wheels stay on ground
- Compare Exp A vs Exp B: same?

### Question 5: Is Blueprint State Matching Runtime?

**Evidence from logs:**
- `FF_Enabled` column should match your Blueprint setting:
  - Exp A: all `1` (enabled)
  - Exp B: all `0` (disabled)
- If mismatch: Blueprint override not taking effect

---

## Deliverables

1. **Reports/Exp_A_ForcedForward_ON.log** (raw log file)
2. **Reports/Exp_B_ForcedForward_OFF.log** (raw log file)
3. **Reports/Exp_A_B_Comparison_Frame_Table.md** (frame-by-frame comparison spreadsheet)

---

## Next Steps

After running both experiments, provide:
- Both log files
- Frame table
- One-sentence hypothesis for each of the 5 root causes

No fixes. Just evidence.

