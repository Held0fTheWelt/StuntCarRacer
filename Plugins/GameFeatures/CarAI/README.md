# CarAI — Machine Learning Racing Agent

A Game Feature plugin for StuntCarRacer that trains AI-controlled vehicles using **NEAT** (neuro-evolution) and **PPO** (deep reinforcement learning). The system is a hybrid between Unreal Engine (simulation, inference) and Python (training, evolution).

---

## How It Works

Unreal Engine acts as the **simulator**: it runs physics, raycasts, and reward calculation, and runs the neural network at inference time. Python acts as the **learning brain**: it receives collected experience data and returns improved model weights. The two sides communicate exclusively via files on disk — no sockets, no gRPC.

```
Unreal Engine                       Python
─────────────────────               ──────────────────────────
RacingAgentComponent                train_pytorch.py  (PPO)
  → run episodes                    train_neat.py     (NEAT)
  → build observations              find_best_model.py
  → compute rewards
         │
         ▼  export  (JSON / NPZ)
         ────────────────────────►  training / evolution
         ◄────────────────────────  model weights (JSON)
         ▼  import
  agent drives with improved policy
         │
         └── loop continues automatically
```

---

## Observation Space (15 values)

Every tick the agent receives a flat vector as input to its neural network:

| # | Field | Description |
|---|---|---|
| 1 | `SpeedNorm` | Forward speed, normalized to `SpeedNormCmPerSec` |
| 2 | `YawRateNorm` | Yaw angular velocity, normalized |
| 3 | `PitchRateNorm` | Pitch angular velocity, normalized |
| 4 | `RollRateNorm` | Roll angular velocity, normalized |
| 5 | `RayForward` | Adaptive ray — straight ahead |
| 6 | `RayLeft` | Adaptive ray — 90° left |
| 7 | `RayRight` | Adaptive ray — 90° right |
| 8 | `RayLeft45` | Adaptive ray — 45° left-forward |
| 9 | `RayRight45` | Adaptive ray — 45° right-forward |
| 10 | `RayForwardUp` | Fixed ray — forward +30° pitch (detects overhangs / loops) |
| 11 | `RayForwardDown` | Fixed ray — forward −30° pitch (detects ramps / drop-offs) |
| 12 | `RayGroundDist` | Fixed ray — straight down (detects gaps in the track) |
| 13–15 | `GravityX/Y/Z` | World gravity in vehicle-local space (IMU — detects being upside-down) |

All ray values are normalized to `[0, 1]` where `1.0` means no hit (free space) and lower values mean an obstacle is nearby.

### Adaptive Rays

The 5 horizontal rays automatically adjust their vertical pitch angle each frame:

| Situation | Reaction |
|---|---|
| No hit (ray shoots into empty space) | Pitch down; accelerates after multiple consecutive misses |
| Hit too close (< 70 % of max range) | Pitch up |
| Hit too far (> 70 % of max range) | Pitch down |
| Hit at target distance | No change |

This allows the sensor to function correctly on 3D geometry (ramps, loops, banked corners) without any manual per-track configuration. Pitch is clamped to `[−45°, +45°]`.

---

## Action Space (3 values)

| Field | Range | Description |
|---|---|---|
| `Steer` | `[−1, 1]` | Steering input |
| `Throttle` | `[0, 1]` | Throttle input |
| `Brake` | `[0, 1]` | Brake input |

---

## Reward Function

The reward is computed in `ComputeReward` on every step and consists of several weighted components:

### Phase 1 — Distance (always active)
- `+W_Distance` for each centimeter traveled forward
- `+W_Survival` per step (constant survival bonus)

### Phase 2 — Speed (activates after 50 m traveled)
- `+W_Speed` reward when speed is near `SpeedTargetNorm` (default 70 % of max speed)

### Penalties (always active)
| Source | Condition | Penalty |
|---|---|---|
| Action smoothness | Large change in steering between steps | `W_ActionSmooth` (negative) |
| Collision warning | Forward ray hits below 15 % of max distance | `CollisionWarningPenalty` |
| Collision terminal | Forward ray hits below 5 % | `CollisionTerminalPenalty` + episode ends |
| Gap warning | Ground ray below 30 % of max distance | `GapWarningPenalty` |
| Gap terminal | Ground ray below 10 % | `GapTerminalPenalty` + episode ends |
| Airborne | Every second in the air | `W_Airborne` |
| Long airborne | Airborne > `AirborneMaxSeconds` | `TerminalPenalty_AirborneLong` + episode ends |
| Stuck | Speed below 5 % of max for > 2 s | `TerminalPenalty_Stuck` + episode ends |

---

## Neural Network (`USimpleNeuralNetwork`)

A fully self-contained MLP implemented in C++ — no external inference library is required.

- **Architecture**: configurable hidden layers (default: 2 × 128 neurons, ReLU activation)
- **Policy head**: outputs the mean for each of the 3 action dimensions
- **Value head**: outputs a single scalar value estimate (used by PPO)
- **Learnable log-std**: per-action exploration noise, learned during training
- **Optimizer**: Adam with full M/V moment state stored per weight (survives save/load)
- **Serialization**: binary checkpoint files + JSON import/export for Python interop

---

## Training Mode 1 — PPO (Proximal Policy Optimization)

The recommended mode for continuous improvement.

### Auto-Training Cycle

1. **Collect** — agents run episodes in Play mode, filling a `UExperienceBuffer` (rollout buffer)
2. **Export** — after `AutoTrainAfterNRollouts` rollouts (default: 30), `UPyTorchExporter` writes the buffer to disk as JSON / NPZ
3. **Train** — `UPythonTrainingExecutor` launches `train_pytorch.py` as a subprocess (configurable epochs, default 10)
4. **Find best** — `find_best_model.py` identifies the checkpoint epoch with the highest reward
5. **Export weights** — `export_model_for_unreal.py` converts the PyTorch checkpoint to a JSON weight file
6. **Import** — `PyTorchImporter` loads the JSON and writes the weights into the live `USimpleNeuralNetwork`
7. **Continue** — agents keep running with the updated policy; loop repeats indefinitely

### Quick Setup

In the `RacingTrainingEditorWidget` (Editor Utility Widget `EUW_AICarCurriculum`):

```
bEnableAutoTraining     = true
bExportOnly             = true        // skip in-engine PPO update, let Python handle it
AutoTrainAfterNRollouts = 30
PythonExecutablePath    = "python"    // or full path if not in PATH
AutoLoadModelEpoch      = 0           // 0 = automatically pick best epoch
```

**Start order (important):**
1. Spawn Agents (button in widget — **before** pressing Play)
2. Press Play
3. Initialize Training (button — **after** Play has started)
4. Start Training (button)

### Key Hyperparameters (`FPPOHyperparameters`)

| Parameter | Default | Description |
|---|---|---|
| `LearningRate` | `0.0003` | Adam learning rate |
| `Gamma` | `0.99` | Discount factor |
| `Lambda` | `0.95` | GAE lambda |
| `ClipRange` | `0.2` | PPO clip epsilon |
| `EntropyCoef` | `0.01` | Entropy bonus for exploration |
| `RolloutSteps` | `2048` | Steps collected before each update |
| `BatchSize` | `64` | Mini-batch size for gradient updates |
| `NumEpochs` | `4` | Gradient update passes per rollout |

---

## Training Mode 2 — NEAT (NeuroEvolution of Augmenting Topologies)

An evolutionary alternative managed by `UNEATTrainingManager` (Editor module).

### Cycle

1. Python generates a population of genomes → saves as JSON to `Saved/Training/NEAT/`
2. UE loads the genomes and assigns one to each registered `URacingAgentComponent`
3. All agents run their episode; fitness is computed as:
   ```
   NEATFitness = DistanceMeters + SpeedBonus   (SpeedBonus active after 50 m)
   ```
   Episodes shorter than 2 seconds are penalized by 50 %.
4. UE exports the `GenomeID → Fitness` map to `Saved/Training/Fitness/`
5. `UPythonTrainingExecutor` runs `train_neat.py` — Python performs selection, mutation, crossover, and speciation
6. New generation genomes are saved → back to step 2

**States**: `Idle → Evaluating → WaitingForPython → Evaluating → …`

---

## Curriculum System

The curriculum system segments the track into typed sections so agents can be spawned at specific challenging locations (e.g., to practice corners specifically).

`URacingCurriculumBuilder` analyzes a `UTrackFrameProviderComponent` (spline) and produces a `URacingCurriculumDataAsset` with segments tagged as:

| Tag | Detection |
|---|---|
| `Corner` | Curvature above threshold |
| `Uphill` / `Downhill` | Z-component of spline tangent |
| `RampApproach` | Lookahead: significant height gain ahead |
| `OnRamp` | Steep tangent Z |

Each segment stores a `SuggestedSpeedNorm` and `MaxSteerHint` that can guide reward shaping or heuristic policies.

The data asset can be built from within the Editor using the `EUW_AICarCurriculum` widget.

---

## Plugin Structure

```
CarAI/
├── Source/
│   ├── CarAIRuntime/           # Runs in-game (inference, data collection, export/import)
│   │   ├── Components/         # URacingAgentComponent
│   │   ├── NN/                 # USimpleNeuralNetwork, UExperienceBuffer
│   │   ├── Export/             # UPyTorchExporter
│   │   ├── Import/             # PyTorchImporter
│   │   ├── Data/               # URacingCurriculumDataAsset
│   │   ├── Builder/            # URacingCurriculumBuilder
│   │   ├── Debug/              # URacingCurriculumDebugActor
│   │   └── Types/              # All structs and enums
│   └── CarAIEditor/            # Editor-only (training management)
│       ├── Manager/            # UNEATTrainingManager, UPythonTrainingExecutor
│       └── Editor/             # UCurriculumEditorWidget, UCurriculumSpawner
└── Content/
    ├── BP_RacingAgentComponent.uasset
    ├── BP_TrackFrameProviderComponent.uasset
    ├── DA_RC_LittleRamp.uasset         # Pre-built curriculum for Map_LittleRamps
    ├── Editor/
    │   ├── EUW_AICarCurriculum.uasset  # Main training editor widget
    │   └── EUW_NeatTraining.uasset     # NEAT training editor widget
    └── Python/                         # Training scripts (see below)
```

---

## Python Scripts (`Content/Python/`)

| Script | Purpose |
|---|---|
| `train_pytorch.py` | PPO training on exported rollout data |
| `train_neat.py` | NEAT evolution loop |
| `export_model_for_unreal.py` | Convert PyTorch checkpoint → JSON for import |
| `find_best_model.py` | Analyze all checkpoints, return best epoch by reward |
| `find_and_export_best_model.py` | Combined find + export in one step |
| `suggest_agent_parameters.py` | Suggest reward/observation hyperparameters for a given track and car |
| `carai_analysis_cli.py` | Analyze collected rollout data (reward curves, action distributions) |
| `neat_analysis_cli.py` | Analyze NEAT training progress |
| `benchmark_cpu_gpu.py` | Compare CPU vs. GPU training performance |

### Python Dependencies

```bash
pip install torch numpy
```

---

## Troubleshooting

**Auto-Training is not triggered**
- Check `bEnableAutoTraining = true` and `AutoTrainAfterNRollouts > 0`
- Check the Output Log for export/training messages

**Python training fails**
- Verify: `python --version` and `python -c "import torch"`
- Check `PythonExecutablePath` in the training config (use full path if `python` is not in `PATH`)
- Check `Saved/Training/` for exported JSON files (confirms UE export worked)

**Agents do not improve (stay stuck / drive in circles)**
- Increase `W_Distance` and `W_Survival` to prioritize forward progress
- Reduce `W_Speed` early in training
- Check that `SpawnLateralOffsetMaxCm` is not too large (agents spawn off the track)
- Use `bDrawRayDebug = true` on the agent to verify sensors are hitting geometry
