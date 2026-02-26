# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

StuntCarRacer is an **Unreal Engine 5.7** vehicle racing game written in C++. It uses the **Chaos Vehicles** physics plugin and **Enhanced Input** system. The project targets Win64.

## Building

This is a standard UE5 project. There is no separate build script — use UnrealBuildTool or Visual Studio:

- **Generate project files:** Right-click `StuntCarRacer.uproject` → "Generate Visual Studio project files"
- **Build (Editor):** Open the `.sln` in Visual Studio and build `StuntCarRacerEditor` (Development Editor | Win64)
- **Build via UAT:** `UnrealBuildTool.exe StuntCarRacerEditor Win64 Development -Project="E:\StuntCarRacer\StuntCarRacer.uproject"`

There are no unit tests in this project.

## Architecture

### Modules

The project has two runtime C++ modules (declared in `StuntCarRacer.uproject`):

- **StuntCarRacer** (`Source/StuntCarRacer/`) — main game module
- **MyProject3** — secondary module (no visible source in repo; depends on AIModule)

### Plugins

Local plugins live at the project root alongside the `.uproject`:

| Plugin | Path | Purpose |
|---|---|---|
| `CarStatistics` | `CarStatistics/` | Game Feature plugin; tracks car stats; depends on `Framework` and `Cars` plugins |
| `GameFeature_EngineSimulator` | `GameFeature_EngineSimulator/` | Game Feature plugin; procedural engine audio synthesis |

Additionally, several engine/marketplace plugins are enabled: `ChaosVehiclesPlugin`, `LearningAgents`, `CarAI`, `StateTree`, `GameplayStateTree`, `CarController`, `CarCamera`, `FlipSystem`, `TimeTrialMode`.

### Core Class Hierarchy

**Vehicle Pawn (abstract base):**
`AStuntCarRacerPawn` (`Source/StuntCarRacer/StuntCarRacerPawn.h`) — extends `AWheeledVehiclePawn`. Handles input binding (steering, throttle, brake, handbrake, look, camera toggle, reset), dual front/back camera management, and a periodic flip-detection timer that auto-resets the car if upside-down.

**Vehicle subclasses (both abstract, configured in Blueprints):**
- `AStuntCarRacerSportsCar` (`Source/StuntCarRacer/SportsCar/`) — sports car variant
- `AStuntCarRacerOffroadCar` (`Source/StuntCarRacer/OffroadCar/`) — off-road variant with explicit tire mesh components

**Game Modes (all abstract, extended in Blueprints):**
- `AStuntCarRacerGameMode` — base game mode
- `ATimeTrialGameMode` (`Variant_TimeTrial/`) — adds finish-line gate, lap counting
- `AOffroadGameMode` (`Variant_OffRoad/`) — simple offroad mode

**Player Controllers:**
- `AStuntCarRacerPlayerController` — base; manages Enhanced Input contexts (default + mobile-excluded + optional steering wheel), spawns `UStuntCarRacerUI` (speed/gear HUD), handles pawn respawn on destruction
- `ATimeTrialPlayerController` — extends base; additionally manages `UTimeTrialUI` (lap/time tracking), the target `ATimeTrialTrackGate` chain, and race start sequencing

**UI Widgets (abstract, implemented in Blueprints):**
- `UStuntCarRacerUI` — vehicle HUD; C++ calls `UpdateSpeed`/`UpdateGear`, Blueprint implements `OnSpeedUpdate`/`OnGearUpdate`
- `UTimeTrialUI` — time trial overlay; tracks lap, best time, current lap time; spawns `UTimeTrialStartUI` for countdown; fires `OnRaceStart` delegate
- `UTimeTrialStartUI` — countdown widget

**Track Gating:**
`ATimeTrialTrackGate` — box-collision actors chained via `NextMarker` pointer; `bIsFinishLine = true` increments lap counter when passed.

### EngineSimulator Plugin

`GameFeature_EngineSimulator/Source/GameFeature_EngineSimulatorRuntime/`

- `UEngineSynthComponent` — extends `USynthComponent`; generates engine sound procedurally in `OnGenerateAudio` based on `CurrentRPM`, cylinder firing offsets, and pre-loaded impulse `.wav` samples (via `dr_wav`)
- `UEngineSimulatorSetupDataAsset` — `UPrimaryDataAsset` that wraps `FEngineConfig` (cylinder count, induction, cooling, hybrid drive, valve timing, etc.)
- Rich enum/struct set in `Public/Enums/` and `Public/Structs/` covering engine topology

### Key Patterns

- All C++ base classes are `UCLASS(abstract)` — concrete gameplay objects are Blueprints
- Input is bound through **Enhanced Input** (`UInputAction` / `UInputMappingContext`); touch controls are conditionally added via `bForceTouchControls` config
- Vehicle pawn inputs are also exposed as `BlueprintCallable` `Do*` functions for mobile UI binding
- `UStuntCarRacerUI` uses the C++ → Blueprint event pattern: C++ calls native update functions, which fire `BlueprintImplementableEvent` for actual display logic
- The `CarStatistics` plugin depends on `Framework` and `Cars` engine plugins; it is a `GameFeature` with `BuiltInInitialFeatureState: Active`
