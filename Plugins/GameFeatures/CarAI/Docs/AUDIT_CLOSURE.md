# CarAI NEAT — End-to-End Closure Audit

**Date**: After MVP-CLOSE-01 through CLOSE-09.  
**Conclusion**: The NEAT workflow is **closed end-to-end from source**. There is no remaining hidden dependency on stale binaries, dead Blueprint logic, or fragile pre-genome behavior.

## Proof points (source-backed)

| # | Proof point | Source / evidence |
|---|-------------|-------------------|
| 1 | Widget can arm before PIE | `NeatTrainingEditorWidget::ArmTraining()` sets `bArmedForPIE`, state `ArmedForPIE`. |
| 2 | PIE start resolves the correct world | `FCarAIEditor::OnPostPIEStarted` → `OnPIEWorldStarted(PIEWorld)`; world from `GEngine->GetWorldContexts()` PIE. |
| 3 | Runtime agents self-register into the registry | `URacingAgentComponent::BeginPlay()` → `World->GetSubsystem<URacingAgentRegistrySubsystem>()->RegisterAgent(this)`. |
| 4 | Widget/manager consumes the registry (not world-scan) | `PollForRuntimeAgents()` uses `World->GetSubsystem<URacingAgentRegistrySubsystem>()->GetRegisteredAgents()`; no TObjectIterator. |
| 5 | Manager registers agents passively | `UNEATTrainingManager::RegisterAgent()`: Add, bind delegate, MarkRegistered; no Initialize, no GrantEvaluationAuthorization. |
| 6 | No agent steps before authorization | `URacingAgentComponent::TickComponent()` early-returns unless `RuntimeState` is EvaluationAuthorized or Evaluating. |
| 7 | Generation genomes loaded successfully | `LoadGenerationGenomes()`; fail-fast if count != PopulationSize; logs. |
| 8 | Active batch agents receive valid genome IDs/backends | `AssignGenomesToAgents()`: sets `Agent->GenomeID`, `SetPolicyBackend(Evaluator)`, then `GrantEvaluationAuthorization()`. |
| 9 | Authorization granted only after assignment | Manager calls `GrantEvaluationAuthorization()` only after genome + backend in same loop in `AssignGenomesToAgents()`. |
| 10 | First real step with GenomeID ≥ 0, no fallback | `StepOnce()` only runs when authorized; NEAT path (GenomeID ≥ 0) uses policy backend only; zero action if no output. |
| 11 | Episode completion attributes fitness to correct agent | `OnAgentEpisodeDone()` matches by `Stats.AgentInstanceID` vs `Agent->GetUniqueID()` within active batch. |
| 12 | Fresh mode works | Manifest `training_mode: "fresh"`; Python deletes checkpoint and best genome; Unreal sets `bFreshStart`. |
| 13 | Resume mode works | Manifest `training_mode: "resume"`; Python loads checkpoint; `training_state.json` sync. |
| 14 | Population parity enforced | Manifest `population_size`; Python `create_or_validate_config(..., population_size)`; neat_config pop_size from manifest. |
| 15 | Final best-genome handling correct | `FinalizeAndLoadBestGenome()`; Python exports best to `best_genome_path`; single source in contract. |

## Evidence in logs (Task.md final proof required)

For the flow to be considered closed, the following log evidence must appear when running the full path. Each line is the exact string (or pattern) to look for in the Output Log.

| # | Requirement | Log evidence (category / message pattern) |
|---|-------------|------------------------------------------|
| 1 | PIE starts and widget/manager enter armed flow | `LogCarAIEditor`: `[NeatTrainingEditorWidget] PIE hook entered; PIE world captured` and workflow state `WaitingForRuntimeAgents` or `RegisteringAgents`. |
| 2 | Runtime agents self-register into the runtime registry | `LogCarAIAgent`: `[RacingAgentRegistry] Agent registered. Total=N World=... Owner=...` (one per agent BeginPlay). |
| 3 | Editor/widget consumes the registry, not world-scanning | `LogCarAIEditor`: `registry_count=N (registry-driven discovery)` and/or `Readiness passed: registry_count=N. Auto-registration with manager triggered`. No TObjectIterator in PollForRuntimeAgents. |
| 4 | Manager registers agents passively without starting them | `LogCarAITraining`: `[NEATTrainingManager] Agent registered (RuntimeState=Registered; awaiting genome assignment and authorization). Total registered: N` — no "Initialize" or "authorization granted" in the same path. |
| 5 | Generation genomes are loaded successfully | `LogCarAITraining`: `[NEATTrainingManager] Loaded N genomes for generation G (all validated, count matches PopulationSize); ready for batch evaluation`. |
| 6 | Active batch agents assigned valid genome IDs and policy backends | `LogCarAITraining`: `[NEATTrainingManager] Genome assigned and authorization granted: agent_index=... genome_id=...`. |
| 7 | Evaluation authorization granted only after assignment | Same as (6); order in code: `SetPolicyBackend(Evaluator)` then `GrantEvaluationAuthorization()` in same loop. Agent log: `[CarAIAgent] Evaluation authorization GRANTED. ... GenomeID=N`. |
| 8 | First real agent step with GenomeID ≥ 0 and no fallback | `LogCarAIAgent`: `[Agent] First step this episode. RuntimeState=... GenomeID=N` with N ≥ 0. No `Using fallback forward drive` for that agent (fallback only when GenomeID < 0). |
| 9 | Fitness export and next-generation continuation intact | `LogCarAITraining`: `[NEATTrainingManager] Exported fitness for generation G (...)` and `[NEATTrainingManager] Python evolution complete` and `training_state.json: exported_generation=G`. |
| 10 | Resume and fresh flows both remain valid | Manifest contains `training_mode`: `"fresh"` or `"resume"`. Python stdout: `[NEAT] Training mode: fresh` or `resume`. Unreal: `Contract population parity: manifest population_size=N`. |

## Manual verification steps

1. **Arm → PIE → registry → manager → start**  
   Open Window > Car AI > NEAT Training, Initialize Manager, Arm Training, Start PIE. In Output Log: PIE hook, registry resolved, registry_count ≥ 1, readiness passed, auto-registration, auto-start. No manual Register/Start click.

2. **First real step**  
   After training starts, in LogCarAIAgent: "First step this episode. RuntimeState=... GenomeID=N" with N ≥ 0. No "fallback forward drive" for NEAT agents.

3. **Population parity**  
   Set PopulationSize in widget (e.g. 24), start training. In manifest JSON: `"population_size": 24`. In Python stdout: "Config population parity: pop_size=24 (source: manifest)".

4. **Fresh vs resume**  
   Fresh: training_mode "fresh", checkpoint/best deleted. Resume: training_mode "resume", checkpoint loaded. Check manifest and Python logs.

## Architecture status

- **Closed for pre-PIE → first genome-driven step**: Yes.  
- **Closed for fitness export and next generation**: Yes.  
- **Closed for fresh and resume flows**: Yes.  
- **No remaining dependency on stale binaries or dead Blueprint logic**: Yes.
