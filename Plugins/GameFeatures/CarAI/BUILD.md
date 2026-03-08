# CarAI Plugin — Build and Source of Truth

## Source of truth

**The only trustworthy source of NEAT workflow behavior is the current source tree.**

- Do **not** rely on pre-built `Binaries/` or `Intermediate/` in the plugin or project. These may be stale.
- After pulling changes or modifying CarAI source, perform a clean or full rebuild so that editor and game use the current code.
- Runtime behavior must be explainable from the checked-in C++ and Blueprint assets. If behavior diverges, rebuild and verify; do not assume old binaries are correct.

## Clean rebuild path

1. Close the Unreal Editor.
2. (Optional but recommended for plugin changes) Delete the plugin build artifacts:
   - `Plugins/GameFeatures/CarAI/Intermediate/`
   - `Plugins/GameFeatures/CarAI/Binaries/`
3. Rebuild the project (e.g. from your IDE or `Build.bat` for your target, e.g. `StuntCarRacerEditor Win64 Development`).
4. Open the project in the editor. On load, the CarAI modules log a startup line (see below) so you can confirm the running build is from current source.

## EUW_NeatTraining asset

- The NEAT training control panel is the Editor Utility Widget **EUW_NeatTraining** (`Content/Editor/EUW_NeatTraining.uasset`).
- For correct behavior, this Blueprint **must** have its parent class set to **`NeatTrainingEditorWidget`** (the C++ class in `Source/CarAIEditor/`). If the asset was created from a different parent or the parent was changed, reparent it to `NeatTrainingEditorWidget` in the Blueprint editor.
- The C++ base implements workflow state, discovery, registration, and auto-start; the Blueprint may add UI only. Do not rely on disabled or empty Blueprint event graphs for core NEAT flow.

## What not to trust

- Stale plugin `.dll`/`.so` in `Binaries/`: rebuild the plugin.
- Old generated headers in `Intermediate/`: regenerated on build; delete folder for a clean UHT run.
- A copy of the widget or manager logic only in Blueprint: the source of truth for workflow is C++ (`NeatTrainingEditorWidget`, `UNEATTrainingManager`).

## Version identification at runtime

When the CarAI editor and runtime modules load, they log a single line to the Output Log so you can confirm the build:

- **CarAIRuntime:** `[CarAI] CarAIRuntime module loaded (plugin source build).`
- **CarAIEditor:** `[CarAI] CarAIEditor module loaded (plugin source build).`

If you do not see these after a rebuild, the new binaries may not be loaded (e.g. editor not restarted, or wrong build target).
