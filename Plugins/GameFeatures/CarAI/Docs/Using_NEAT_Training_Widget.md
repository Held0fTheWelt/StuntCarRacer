# Using the NEAT Training Editor Widget

## Overview

`EUW_NeatTraining` is an Editor Utility Widget (a tab-based editor tool) that provides
a control surface for NEAT training. It is backed by `UNeatTrainingEditorWidget` in the
`CarAIEditor` module. All training logic lives in `UNEATTrainingManager`; the widget just
configures and controls it.

## How to Open

**Via menu:** `Window > Car AI > NEAT Training`

The menu entry is registered by `FCarAIEditor::StartupModule()` and opens the widget
via `EditorUtilitySubsystem::SpawnAndRegisterTab()`.

**Alternative:** Right-click `Content/Editor/EUW_NeatTraining` in the Content Browser
and choose **Run Editor Utility Widget**.

## Asset Setup (one-time, after compiling the C++ class)

`EUW_NeatTraining` must be reparented to `UNeatTrainingEditorWidget`:

1. Build the project so `UNeatTrainingEditorWidget` is available.
2. In the Content Browser open `Plugins/CarAI/Content/Editor/EUW_NeatTraining`.
3. In the Blueprint editor go to **File > Reparent Blueprint**.
4. Select `NeatTrainingEditorWidget`.
5. Compile and save.
6. Add controls matching the C++ properties and bind buttons to the `CallInEditor` functions.

If the asset was already created as a normal Widget Blueprint (not an Editor Utility Widget),
delete it and create a new **Editor Utility Widget** (right-click in Content Browser >
Editor Utilities > Editor Utility Widget), then reparent as above.

## Workflow

```
Initialize Manager
    ↓ (creates UNEATTrainingManager, owned by the widget)
Set Config (FreshStart, PopulationSize, NumGenerations, MaxEpisodeDuration, Python)
    ↓
Enter PIE (Play In Editor) — agents must be alive in the level
    ↓
Register Agents
    ↓ (finds all URacingAgentComponent in the PIE world)
Start Training
    ↓ (applies config, calls UNEATTrainingManager::StartTraining)
    ↓ Python runs in background → genomes evaluated → fitness exported → repeat
Stop Training (optional early stop)
    ↓
Refresh Status (updates CurrentGeneration, bTrainingInProgress display)
```

## Config Properties

| Property | Default | Notes |
|---|---|---|
| `bFreshStart` | false | If true, Python discards checkpoint and best genome on next run |
| `PythonExecutable` | "python" | Full path if Python is not on PATH (e.g. `C:/Python311/python.exe`) |
| `PopulationSize` | 50 | Must match `pop_size` in `neat_config.txt` |
| `NumGenerations` | 50 | Generations to train in this session |
| `MaxEpisodeDuration` | 120 s | Timeout before fitness is assigned by timeout |

## Status Properties (Blueprint-readable)

| Property | Meaning |
|---|---|
| `bManagerReady` | True after InitializeManager succeeds |
| `RegisteredAgentCount` | Number of agents found by RegisterAgents |
| `CurrentGeneration` | Updated by RefreshStatus from the manager |
| `bTrainingInProgress` | True while the manager is in Evaluating state |
| `LastStatusMessage` | Human-readable summary of the last action result |

## Failure Modes

| Situation | Log / Status message |
|---|---|
| `StartTraining` with no manager | `ERROR: Initialize manager before starting training` |
| `StartTraining` with no agents | `ERROR: No agents registered; call Register Agents first` |
| `StartTraining` with empty Python path | `ERROR: Python executable is not set` |
| `RegisterAgents` with no world | `ERROR: No valid world; enter PIE or open a level` |
| `RegisterAgents` finds no agents | `WARNING: No agents found` |
| Menu open but asset is wrong type | `EUW_NeatTraining not found` error in Output Log |

## Output Log Tags

All widget logs use `[NeatTrainingEditorWidget]` prefix. All manager logs use
`[NEATTrainingManager]`. All Python output is streamed with `[Python]` prefix.

## Source Files

| File | Purpose |
|---|---|
| `Source/CarAIEditor/Public/UI/NeatTrainingEditorWidget.h` | C++ base class |
| `Source/CarAIEditor/Private/UI/NeatTrainingEditorWidget.cpp` | Implementation |
| `Source/CarAIEditor/Private/CarAIEditor.cpp` | Menu registration (`OpenNeatTraining`) |
| `Content/Editor/EUW_NeatTraining.uasset` | Blueprint UI asset |
