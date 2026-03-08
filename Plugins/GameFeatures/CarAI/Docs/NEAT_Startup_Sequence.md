# NEAT Startup Sequence (GameFeature-Added Agents)

This document describes the **intended startup sequence** when `URacingAgentComponent` instances are added by a GameFeature at runtime (e.g. only after PIE starts). The order is enforced so that agents never step with `GenomeID < 0` or without an assigned policy backend.

## Order of operations

1. **Training armed (before PIE)**  
   User opens the NEAT Training widget, initializes the manager, and clicks **Arm Training** (or **Start When Ready**). No agents exist yet.

2. **PIE detected**  
   Editor module receives `PostPIEStarted`. If a widget is armed, it captures the PIE world and moves to “waiting for runtime agents”.

3. **Waiting for runtime agents**  
   The widget polls the PIE world periodically for `URacingAgentComponent` instances. Status shows “Waiting for runtime agents (GameFeature)…”. If none appear within the timeout, the tool fails clearly.

4. **Agents discovered and registered (idle)**  
   When at least one agent is found, the widget registers them with `UNEATTrainingManager::RegisterAgent()`. Registration is **passive**: agents are stored and delegates bound; **no activation, no reset, no stepping**. Each agent’s `GenomeID` is set to `-1` and policy backend is cleared. Log: “Registered agent (idle; evaluation will start when genomes are loaded and assigned).”

5. **Python/evolution triggered**  
   The widget calls `StartTraining()`. The manager triggers Python to export generation 0 (or the next generation). Log: “Python/evolution triggered (generation N, manifest=…).”

6. **Generation genomes loaded**  
   When Python finishes, the manager loads the generation genome list and individual genome files. Log: “Loaded N genomes for generation N (all validated…); ready for batch evaluation.”

7. **Genomes assigned to active batch**  
   `AssignGenomesToAgents()` sets each active-batch agent’s `GenomeID` and `SetPolicyBackend()`. Log: “Genome assigned: NEAT runtime backend set on agent (agent_index=…, genome_id=…).”

8. **Evaluation activated**  
   `StartEpisodeEvaluation()` runs only after assignment. A hard gate checks that every active-batch agent has `GenomeID >= 0` and `HasNEATPolicyBackend()`. If any fail, evaluation is aborted with an error. Then agents that are not yet active receive `Initialize()` (Activate + tick + ResetEpisode). Log: “Evaluation activated for N agent(s) (genomes assigned; stepping begins).”

9. **Stepping**  
   Only after step 8 do agents run `StepOnce()` each tick. The component also refuses to step if `GenomeID >= 0` and no policy backend is set (safety net).

## Failure guards (logs/errors)

- **No runtime agents within timeout:** Widget logs error and moves to “Training failed”; status explains timeout.
- **Training started without genomes loaded:** The design does not start evaluation until after genomes are loaded and assigned; `StartEpisodeEvaluation()` is only called after `LoadGenerationGenomes()` and `AssignGenomesToAgents()`.
- **Active batch agent missing backend / GenomeID < 0:** `StartEpisodeEvaluation()` logs “GUARD: …” and aborts; no agent is activated for that batch.
- **Agent entering evaluation with GenomeID < 0:** If an agent were ever to tick in NEAT mode without a backend, `TickComponent` skips `StepOnce()` and logs once per episode.

## Summary

- **Registering an agent does not start an episode.**
- **Agents do not step in NEAT evaluation mode before a genome/backend is assigned.**
- **First episode start happens only after generation genomes are loaded and assigned to the active batch.**
