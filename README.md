# StuntCarRacer

A vehicle racing game built with **Unreal Engine 5.7** and C++. The project features physics-based vehicles powered by the Chaos Vehicles plugin, multiple game modes, procedural engine audio synthesis, and support for keyboard, gamepad, steering wheel, and touch input.

## Features

- **Physics-based vehicles** via Chaos Vehicles — sports car and off-road variants
- **Time Trial mode** — lap-based racing with sequenced track gates, lap timer, and best-lap tracking
- **Off-road mode** — freeform off-road driving
- **Procedural engine audio** — real-time synthesis of engine sound based on RPM and cylinder configuration
- **Machine learning AI** — NEAT and PPO-based racing agents trained via a hybrid Unreal + Python pipeline ([→ CarAI details](Plugins/GameFeatures/CarAI/README.md))
- **Multi-input support** — keyboard/gamepad, touch/mobile controls, and optional steering wheel
- **Auto-flip recovery** — the car automatically resets if it stays upside-down
- **Dual camera system** — front and rear spring-arm cameras with toggle

## Requirements

- Unreal Engine 5.7
- Visual Studio 2022 (with C++ game development workload)
- Windows 10/11 (Win64)
- Python 3.x + PyTorch (for AI training only — not required to run the game)

## Getting Started

1. Clone the repository.
2. Right-click `StuntCarRacer.uproject` and select **"Generate Visual Studio project files"**.
3. Open the generated `.sln` in Visual Studio 2022.
4. Build the `StuntCarRacerEditor` target (Development Editor | Win64).
5. Open the project in the Unreal Editor and load `Content/Map_LittleRamps.umap`.

## Project Structure

```
StuntCarRacer/
├── Source/StuntCarRacer/           # Main C++ game module
│   ├── SportsCar/                  # Sports car vehicle class
│   ├── OffroadCar/                 # Off-road vehicle class
│   ├── Variant_TimeTrial/          # Time Trial game mode & UI
│   └── Variant_OffRoad/            # Off-road game mode
├── Plugins/GameFeatures/CarAI/     # ML racing AI — NEAT + PPO (see CarAI/README.md)
├── CarStatistics/                  # Plugin: car statistics tracking (Game Feature)
├── GameFeature_EngineSimulator/    # Plugin: procedural engine audio (Game Feature)
├── Content/                        # Blueprints, maps, assets
└── Config/                         # Engine, game, input configuration
```

## Game Modes

| Mode | Class | Description |
|---|---|---|
| Base | `AStuntCarRacerGameMode` | Base mode, extend for custom variants |
| Time Trial | `ATimeTrialGameMode` | Lap racing through a sequence of track gates |
| Off-road | `AOffroadGameMode` | Open off-road driving |

## Plugins

### CarAI
A Game Feature plugin that provides machine learning-based racing agents. Supports two training modes — **NEAT** (neuro-evolution) and **PPO** (deep reinforcement learning) — using a hybrid Unreal + Python pipeline. The neural network and optimizer run entirely in C++; Python handles gradient computation and evolution. See [`Plugins/GameFeatures/CarAI/README.md`](Plugins/GameFeatures/CarAI/README.md) for full documentation.

### CarStatistics
A Game Feature plugin that tracks car performance statistics. Depends on the `Framework` and `Cars` engine plugins.

### GameFeature_EngineSimulator
Provides `UEngineSynthComponent`, a `USynthComponent` subclass that generates engine audio in real time. Engine characteristics (cylinder count, induction type, valve timing, hybrid drive, cooling system, etc.) are configured via `UEngineSimulatorSetupDataAsset` data assets.

## Input

Input actions are configured through Unreal's **Enhanced Input** system. The player controller supports:

- **Keyboard / Gamepad** — default mapping contexts
- **Steering Wheel** — optional additional mapping context (`bUseSteeringWheelControls`)
- **Touch / Mobile** — UMG-based on-screen controls, activated on mobile platforms or via `bForceTouchControls` in `DefaultGame.ini`

## Architecture Notes

All C++ base classes are `UCLASS(abstract)`. Concrete gameplay objects (vehicles, controllers, widgets) are configured as Blueprint subclasses in the Content browser. The C++ layer defines the logic; Blueprints provide asset references and visual implementations (especially for UI via `BlueprintImplementableEvent`).

## License

Copyright Epic Games, Inc. All Rights Reserved. (Template-derived code)
Custom code copyright the project author.
