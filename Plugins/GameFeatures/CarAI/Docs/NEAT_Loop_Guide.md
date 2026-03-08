# CarAI NEAT Loop — Developer Guide

## End-to-End Loop

```
Python (train_neat.py)
  → exports generation_{N}_genomes.json + genome_{id}.json + best_genome.json
  → Unreal: NEATTrainingManager.LoadGenerationGenomes()
  → Unreal: AssignGenomesToAgents() (batch mode when pop > agents)
  → Unreal: StartEpisodeEvaluation() (resets active batch agents only)
  → Agents run episodes; RacingAgentComponent fires OnEpisodeDone with FEpisodeStats
  → NEATTrainingManager.OnAgentEpisodeDone() attributes fitness by AgentInstanceID
  → After all genomes evaluated: ExportFitnessValues() → generation_{N}.json
  → TriggerPythonEvolution() → Python resumes, loads fitness, reproduces, exports N+1
  → Repeat until NumGenerations reached
  → Training complete: FinalizeAndLoadBestGenome() reads last fitness file, copies best
    genome_{id}.json to best_genome.json, then calls LoadBestGenome() for inference
```

## Training Mode (Fresh vs Resume)

Set `bFreshStart` on `UNEATTrainingManager` before calling `StartTraining()`:

- **`bFreshStart = false` (default — Resume):** Python loads the latest checkpoint
  (`neat_checkpoint_latest.pkl`) if it exists, then continues evolution from the last
  exported generation. Use to continue a run across sessions.
- **`bFreshStart = true` (Fresh):** Python deletes the checkpoint and `best_genome.json`
  before running, guaranteeing a clean slate. Use to start a new experiment.

The chosen mode is written into `neat_contract.json` as `training_mode = "fresh" | "resume"`
and logged at startup: `[NEATTrainingManager] TrainingMode: FRESH | RESUME`.

## Expected Artifacts (Saved/Training/)

| File | Written by | Consumed by |
|---|---|---|
| `neat_contract.json` | Unreal (NEATTrainingManager) | Python (train_neat.py --manifest) |
| `NEAT/generation_{N}_genomes.json` | Python | Unreal (LoadGenerationGenomes) |
| `NEAT/genome_{id}.json` | Python | Unreal (LoadGenerationGenomes) |
| `NEAT/best_genome.json` | Unreal (FinalizeAndLoadBestGenome) | Unreal (LoadBestGenome) |
| `NEAT/neat_checkpoint_latest.pkl` | Python | Python (resume) |
| `Fitness/generation_{N}.json` | Unreal (ExportFitnessValues) | Python (FitnessLoader) |
| `Logs/python_training_*.log` | Python (via batch script) | Developer / Unreal log |

Note: Python (resume path) also exports `best_genome.json` as a secondary guard, but
`FinalizeAndLoadBestGenome()` is the authoritative Unreal-side step that always runs at
training completion. It reads the just-written fitness file, picks the best genome_id by
fitness, copies `genome_{id}.json` to `best_genome.json`, then calls `LoadBestGenome()`.

## Manual Verification Procedure

1. Set `bFreshStart = true` on the manager if starting a new experiment.
2. Start training (EUW_NeatTraining editor widget → Start).
3. Check Unreal log for `[NEATTrainingManager] TrainingMode:` — verify fresh or resume.
4. Check Unreal log for `[NEAT contract]` block — verify all 7 fields (paths + sizes + training_mode).
5. After first Python run: verify `Saved/Training/NEAT/generation_0_genomes.json` and `genome_*.json` exist.
6. After first evaluation: verify `Saved/Training/Fitness/generation_0.json` exists with `"genomes": [{...}]`.
7. After second Python run: verify `generation_1_genomes.json` exists (and optionally `best_genome.json`).
8. At training completion: check `[NEATTrainingManager] Best genome artifact finalized:` — confirm generation, genome_id, fitness, path.
9. Check `[NEATTrainingManager] Best genome loaded:` appears immediately after finalization.
10. Check `[NEATTrainingManager] Fitness attributed:` lines — every genome_id should appear once per generation.

### Verifying Fresh Mode
- Start with existing `neat_checkpoint_latest.pkl` in `Saved/Training/NEAT/`.
- Set `bFreshStart = true`, start training.
- Python log should say `[NEAT] Fresh mode: deleted checkpoint ...`.
- First Python run should say `[NEAT] Fresh start: no checkpoint at ...` and export generation_0.

### Verifying Resume Mode
- Ensure `neat_checkpoint_latest.pkl` exists.
- Set `bFreshStart = false` (default), start training.
- Python log should say `[NEAT] Resumed run: checkpoint found at ...`.

## Common Failure Signatures

| Symptom | Likely Cause |
|---|---|
| `VALIDATION FAILED: Genome list not found: generation_N_genomes.json` | Python failed or wrong generation number |
| `VALIDATION FAILED: Observation size mismatch` | Python contract `num_inputs` != 15 |
| `ERROR: Missing fitness export for generation N` | Unreal didn't write fitness before calling Python |
| `Episode-done event rejected: agent_id=X not found in active batch` | Inactive agent fired (should be silent after isolation fix) |
| `FinalizeAndLoadBestGenome: fitness file not found for generation N` | ExportFitnessValues failed silently — check earlier log for write errors |
| `FinalizeAndLoadBestGenome: genome file not found: genome_{id}.json` | Python genome files were deleted/moved before training completed |
| `Python evolution failed!` | Check `Saved/Training/Logs/python_training_*.log` |
| `Missing backend / failed runtime assignment` | `UNEATGraphEvaluator::CreateFromGenome` returned nullptr — check activation functions in genome JSON |
| Fitness stuck at 0 for all genomes | `OnAgentEpisodeDone` not being called — verify agents are registered and `Initialize()` was called |
| `ERROR: Unknown training_mode 'X'` | Contract manifest has unexpected value; only "fresh" and "resume" are valid |

## Generation Numbering

- Python fresh start: exports `generation_0_genomes.json`, saves checkpoint with `last_exported=0`.
- Unreal evaluates generation 0, exports `Fitness/generation_0.json`, increments `CurrentGeneration` to 1.
- Python resume: reads checkpoint (`last_exported=0`), loads `Fitness/generation_0.json`, reproduces, exports `generation_1_genomes.json`, saves checkpoint with `last_exported=1`.
- Repeat. Numbers are always aligned.

## Batch Mode

When `PopulationSize > Agents.Num()`, genomes are evaluated in waves:
- Batch 0: agents 0..N-1 evaluate genomes 0..N-1
- Batch 1: agents 0..N-1 evaluate genomes N..2N-1
- ...

Agents outside the active batch are `ForceEpisodeDone()`-d and cannot fire episode events.
All batches must complete before fitness is exported. No genome is silently skipped.
