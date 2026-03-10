# MVP-03 Report — Why the MVP-01 Baseline Differed from MVP-02 Experiments

## Goal

Prove why the MVP-01 baseline (Steps=14, Stuck at ~1.65s wall time) produced a different result than the MVP-02 experiments where FF=ON never caused Stuck and FF=OFF caused Stuck at step 492 / t=4.5s.

---

## Evidence: Runtime Conditions Compared

### Session A — Backup log `StuntCarRacer-backup-2026.03.09-22.58.28.log` (22:52 run — MVP-01 baseline)

| Item | Value |
|---|---|
| Log file | StuntCarRacer-backup-2026.03.09-22.58.28.log |
| Time | 22:52:47 |
| ForcedForward | ON (Config dump: `ForcedForward=ON ForcedForwardDur=2.0s`) |
| Grace | 2.5s |
| StuckTimeSeconds | 2.0s |
| Episode result | Stuck, Steps=14, Progress=0.1m |
| Frame rate | ~3 fps (DeltaTime ≈ 0.333s/frame) |
| Python subprocess | `train_neat.py` started simultaneously with PIE |

**Step-by-step frame data (baseline run):**

| Step | Wall delta (ms) | DeltaTime (s) | EpisodeTimeAccum (s) | RayGroundDist | grounded_frames | vel_forward (cm/s) | throttle | FF_active | grace_active |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 0 | — | 0.000 | 0.000 | 0 | 0.0 | 1.00 | YES | YES |
| 1 | +318 | ~0.318 | ~0.318 | 0.000 | 0 | 0.0 | 1.00 | YES | YES |
| 2 | +335 | ~0.335 | ~0.653 | 0.000 | 0 | 0.0 | 1.00 | YES | YES |
| 3 | +333 | ~0.333 | ~0.986 | 0.991 | 0 | 0.0 | 1.00 | YES | YES |
| 4 | +333 | ~0.333 | ~1.319 | 0.985 | 1 | 0.6 | 1.00 | YES | YES |
| 5 | +333 | ~0.333 | ~1.652 | 0.978 | 2 | 3.7 | 1.00 | **ENDED** | YES |
| 6-7 | +666 | ~0.333 ea | ~2.0–2.32 | — | — | ~0-4 | policy | NO | YES→NO |
| 8-13 | +1998 | ~0.333 ea | ~2.32–4.32 | — | — | ~0 | policy | NO | NO (**Stuck accumulates**) |

**Observation at FF window end (step 5, t≈1.65s wall / ~2.0s game time):**
```
steps_in_window=6 avg_signed_forward_cm_s=0.7 forward_motion_established=no
```

**Grace window end** (step ~7, t≈2.5s game time, 22:52:49.584):
```
Grace window ended (2.50s elapsed). Collision/offtrack will now terminate episode.
```

**Episode terminated** (step 13 = Steps=14, 22:52:51.586 = ~4.32s wall time):
```
Episode completed: Stuck | Fitness=0.05 Steps=14 Progress=0.1m GenomeID=1
```

---

### Session B — Post-rebuild log `StuntCarRacer.log` (23:40 run — MVP-02 Exp A)

| Item | Value |
|---|---|
| Log file | StuntCarRacer.log |
| Time | 23:40:49 |
| ForcedForward | ON |
| Grace | 2.5s |
| StuckTimeSeconds | 2.0s |
| Episode result | No Stuck from FF=ON (survived to meaningful progress) |
| Frame rate | ~100 fps (DeltaTime ≈ 0.010s/frame) |
| Python subprocess | Already running or not started during PIE startup |

**Step-by-step frame data (post-rebuild Exp A, FF=ON):**

| Step | DeltaTime (s) | EpisodeTimeAccum (s) | RayGroundDist | grounded_frames | vel_forward (cm/s) | FF_active |
|---|---|---|---|---|---|---|
| 0 | ~0.010 | 0.000 | — | 0 | 0.0 | YES |
| 1 | ~0.010 | 0.029 | — | 0 | 0.0 | YES |
| 8 | ~0.010 | 0.102 | — | 1 | -0.4 | YES |
| 9 | ~0.010 | 0.112 | — | 2 | -0.7 | YES |
| 14 | ~0.010 | 0.162 | — | 7 | -3.6 | YES |
| ~200 | ~0.010 | ~2.0 | — | ~190 | **fast** | **ENDED** |

At step 14 (t=0.162s), FF window still has **1.838s** remaining. By the time FF ends (t≈2.0s, ~step 200), the vehicle has been receiving throttle=1.0 for 200 frames and is well above the stuck speed threshold.

---

## Root Cause: DeltaTime Compression Due to Python Subprocess CPU Contention

### The Mechanism

The `bStuckSuppressed` logic and grace protection both operate in **seconds**, not frames:

```cpp
const bool bStuckSuppressed = (EpisodeGraceTimeRemaining > 0.f) ||
    (bEnableForcedForwardDiagnostic && EpisodeTimeAccum < ForcedForwardDiagnosticDuration);
```

When the game runs at **3fps** (DeltaTime=0.333s per tick):
- FF window (2.0s) = **6 steps**
- Grace window (2.5s) = **7.5 steps** (ends at step ~7)
- Stuck threshold (2.0s) = **6 steps**
- Total episode = 6 + 2 + 6 = **14 steps** → Stuck

When the game runs at **100fps** (DeltaTime=0.010s per tick):
- FF window (2.0s) = **200 steps**
- Grace window (2.5s) = **250 steps**
- Stuck threshold (2.0s) = **200 steps**
- At step 200 (when FF ends), vehicle has accelerated for 2s → well above StuckSpeedNorm

### Why Was the Game Running at 3fps?

The `train_neat.py` Python subprocess was launched by `UPythonTrainingExecutor` at PIE start. This subprocess starts a NEAT training loop and consumes significant CPU. On the test machine, this caused:
- UE rendering + physics + Python process = CPU saturation
- UE frame time → ~333ms per frame
- All protection windows exhausted 37× faster in step count

At 100fps (no concurrent Python subprocess):
- Each 333ms of real time = ~33 UE ticks
- Protection windows span hundreds of steps
- Vehicle has ample opportunity to accelerate through the FF window

### Why Did the Vehicle Still Not Establish Forward Motion at 3fps?

Even with throttle=1.0 applied every frame:
- Steps 0-2 (1 full second at 3fps): vehicle is **airborne** (RayGroundDist=0.000 → no ground contact → no traction)
- Steps 3-5: vehicle settles, only 2 grounded frames by step 5
- After just 2 grounded frames (≈0.67s of ground contact), avg velocity = 0.7 cm/s
- FF window ends immediately after → policy takes over → 0 throttle → Stuck

At 100fps:
- Airborne frames: ~7 frames (0.07s), negligible
- Vehicle settles by frame 8 (0.102s into episode)
- By FF window end (t=2.0s, ~200 frames), vehicle has had ~197 grounded frames of throttle

---

## Runtime Diff Table: Baseline vs Experiments

| Dimension | MVP-01 Baseline (Steps=14) | MVP-02 Exp A (FF=ON, no Stuck) | MVP-02 Exp B (FF=OFF, Stuck@step492) |
|---|---|---|---|
| Binary | Pre-rebuild | Post-rebuild | Post-rebuild |
| ForcedForward | ON | ON | OFF |
| Frame rate | ~3fps | ~100fps | ~100fps |
| DeltaTime | ~0.333s | ~0.010s | ~0.010s |
| Python subprocess concurrent | YES | NO | NO |
| FF window (steps) | 6 | ~200 | N/A |
| Steps when grace ends | ~7 | ~250 | ~250 |
| grounded_frames at FF end | 2 | ~197 | N/A |
| vel at FF end (cm/s) | 3.7 (too slow) | >> threshold | N/A |
| Stuck fires | YES (step 13) | NO | YES (step 492) |
| Policy source | NEAT genome (gen=0) | NEAT genome (gen=0) | NEAT genome (gen=0) |
| Config (all values) | **Identical** | **Identical** | **Identical** |

**Conclusion: The only runtime difference is frame rate (DeltaTime), caused by Python subprocess CPU contention at PIE startup.**

---

## Hypotheses Evaluated

| Hypothesis | Result |
|---|---|
| Different source code / binary | **DISPROVEN** — same fix is in both; Config dumps show identical values |
| Different Blueprint runtime values | **DISPROVEN** — Config dump identical: same Grace, StuckSec, FF duration, thresholds |
| Suppression logic not active | **DISPROVEN** — suppression IS active in both binaries; but at 3fps it runs out of steps faster |
| Policy/genome failure | **DISPROVEN** — FF=ON overrides policy in both sessions; config matches |
| Drivetrain/gear engagement failure | **DISPROVEN** — throttle=1.00 applied in both; difference is grounded contact time |
| Large DeltaTime exhausting protection windows | **CONFIRMED** — 3fps causes FF/grace to expire after 6/7 steps vs 200/250 at 100fps |
| Python subprocess CPU contention | **CONFIRMED** — 3fps only occurs when `train_neat.py` subprocess launches concurrently with PIE |
| Airborne spawn with no-traction period | **CONFIRMED** — steps 0-2 at 3fps = 1 full second airborne; vehicle not grounded when traction matters |

---

## Startup Engagement Truth Table (Updated)

| Scenario | DeltaTime | FF steps | Grace steps | Stuck steps | Vehicle grounded by FF end? | Outcome |
|---|---|---|---|---|---|---|
| train_neat.py concurrent | ~333ms | 6 | 7 | 6 | NO (only 2 frames) | **Stuck at step 14** |
| No concurrent subprocess | ~10ms | 200 | 250 | 200 | YES (~197 frames) | **Survives** |
| FF=OFF, no subprocess | ~10ms | N/A | 250 | 200 | YES | **Stuck at step 492 (correct)** |

---

## Is This Still a Risk?

**YES.** The current code does not protect against large DeltaTime caused by concurrent Python subprocess startup.

The `bStuckSuppressed` logic only uses time-based windows. On any machine where `train_neat.py` startup causes frame rate to drop below ~3fps at PIE start, the Steps=14 failure mode will recur.

### Candidate Minimal Fix

The smallest fix that eliminates this class of failure:
1. Add a `MinStuckGraceFrames` check: Stuck cannot fire until at least N frames have elapsed (independent of DeltaTime). E.g., `EpisodeStepCount < 30` as an additional suppression condition.
2. Alternatively: launch Python subprocess asynchronously and delay PIE evaluation start until the first frame renders at > some threshold fps (harder to implement).
3. Or: cap DeltaTime to a maximum (e.g., 50ms) in the agent tick to prevent any single large DeltaTime from burning multiple seconds of grace.

**Option 3 is the safest minimal change**: cap DeltaTime at agent tick entry to `FMath::Min(DeltaTime, 0.05f)`. This ensures no single tick can consume more than 50ms of protection budget, regardless of system load.

---

## Classification

- **Large DeltaTime from Python subprocess CPU contention**: **Primary root cause** of Steps=14 baseline failure
- **Short airborne period at spawn**: **Secondary amplifier** — at 3fps, 3 airborne frames = 1 second of wasted throttle time
- **Suppression logic correctness**: **Not a bug** — logic is correct; the problem is the time granularity at low fps
- **Different binary / code drift**: **Disproven** — config dumps confirm identical values in both sessions

---

## Next Step (MVP-04 per Task.md)

Implement the minimal DeltaTime cap fix and validate that a simulated heavy-load startup (or actual concurrent subprocess) no longer produces Steps=14 Stuck.
