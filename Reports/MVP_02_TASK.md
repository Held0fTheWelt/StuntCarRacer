# MVP-02 Task: Prove Startup Stuck Root Cause

**Status:** READY FOR EXECUTION
**Prerequisite:** MVP-01 baseline locked (commit 2d28949)

## What to Do

### 1. Rebuild C++

Choose one:
- **Option A (Fast):** Live Coding
  - In Unreal Editor: `Ctrl+Alt+F11`
  - Wait for recompile to finish (watch Output Log)
  
- **Option B:** Full rebuild
  - Close Editor
  - Open `StuntCarRacer.sln` in Visual Studio
  - Build `StuntCarRacerEditor | Development | Win64`
  - Launch from Editor when done

### 2. Configure Test Scenario

In the test map (Content/Map_LittleRamps):
- **Spawn exactly ONE agent** (use a single Test Vehicle with BP_RacingAgentComponent)
- **Enable forced-forward diagnostic:**
  - Select BP_RacingAgentComponent on the vehicle
  - In Details panel, find `bEnableForcedForwardDiagnostic`
  - Check it (must be checked, not unchecked)
  - Compile (blue Compile button) and Save Blueprint
  
- **Set manager population to 1** (for simplicity)
  - In EUW_NeatTraining widget: Set PopulationSize=1

### 3. Start PIE Run

1. Open Output Log window (Windows → Developer Tools → Output Log)
2. Start training in EUW (click Start Training button)
3. Press Play (PIE)
4. Let the episode run for ~10-15 seconds (will terminate with Stuck)
5. Stop PIE
6. Scroll through Output Log and save to file

### 4. Capture Diagnostics

Search for these log patterns in Output Log:

**Startup diagnostics:**
```
STARTUP DIAG [frame=1..5]
```

Record:
- Velocity forward value
- Wheels on ground (X/Y)
- Stuck timer value
- Grace remaining time
- Diagnostic window remaining time

**Stuck timer accumulation:**
```
STUCK CHECK [frame=1..10]
```

Record:
- SpeedNorm value
- StuckAccum value
- Threshold value (StuckTimeSeconds)
- Whether reset occurred

**Action application:**
```
ACTION APPLIED [frame=1..10]
```

Record:
- Whether throttle > 0 was applied
- Frame number when first applied

**Stuck termination:**
```
STUCK TRIGGERED [frame=N t=X.XXs]
```

Record:
- Exact frame number
- Exact time
- Speed value at termination
- Stuck accumulator value

### 5. Build Evidence Spreadsheet

Create a table:

| Frame | Time(s) | Speed | Throttle Applied? | Wheels On Ground | Stuck Timer | Grace | Diag Window | Outcome |
|-------|---------|-------|------------------|------------------|-------------|-------|-------------|---------|
| 1     |  0.00   | 0.0   | Yes (1.0)        | 0/4              | 0.0         | 2.5s  | 2.0s        | -       |
| 2     |  0.12   | 0.1   | Yes (1.0)        | 0/4              | 0.12        | 2.38  | 1.88        | -       |
| ...   | ...     | ...   | ...              | ...              | ...         | ...   | ...         | ...     |
| N     |  2.0    | 0.05  | Yes (1.0)        | 2/4              | 0.0 (suppressed) | 0.5 | -0.0        | STUCK! |

## Success Criteria for MVP-02

Green gate requires identifying **at least one** of these:

1. **Vehicle not receiving throttle action** → ACTION APPLIED shows throttle=0.0 consistently
2. **Vehicle receiving throttle but wheels not engaging** → ACTION APPLIED shows throttle>0 but Wheels=0/4 and Speed=0
3. **Stuck timer not being reset/suppressed during grace** → STUCK CHECK shows accumulation continuing through grace window
4. **Stuck timer not being reset/suppressed during forced-forward** → STUCK CHECK shows accumulation continuing through diagnostic window
5. **Stuck threshold is too aggressive** → Stuck fires at 1.5s instead of 2.0s

## Next Step

Once you capture the logs, paste the relevant diagnostic section and note which hypothesis is proven/disproven.

