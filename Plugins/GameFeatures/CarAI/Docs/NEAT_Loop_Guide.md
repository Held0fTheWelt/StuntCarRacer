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
  → Training complete: LoadBestGenome() assigns best genome to agents for inference
```

## Expected Artifacts (Saved/Training/)

| File | Written by | Consumed by |
|---|---|---|
| `neat_contract.json` | Unreal (NEATTrainingManager) | Python (train_neat.py --manifest) |
| `NEAT/generation_{N}_genomes.json` | Python | Unreal (LoadGenerationGenomes) |
| `NEAT/genome_{id}.json` | Python | Unreal (LoadGenerationGenomes) |
| `NEAT/best_genome.json` | Python (resume path only) | Unreal (LoadBestGenome) |
| `NEAT/neat_checkpoint_latest.pkl` | Python | Python (resume) |
| `Fitness/generation_{N}.json` | Unreal (ExportFitnessValues) | Python (FitnessLoader) |
| `Logs/python_training_*.log` | Python (via batch script) | Developer / Unreal log |

## Manual Verification Procedure

1. Start training (EUW_NeatTraining editor widget → Start).
2. Check Unreal log for `[NEAT contract]` block — verify all 6 fields (paths + sizes).
3. After first Python run: verify `Saved/Training/NEAT/generation_0_genomes.json` and `genome_*.json` exist.
4. After first evaluation: verify `Saved/Training/Fitness/generation_0.json` exists with `"genomes": [{...}]`.
5. After second Python run: verify `generation_1_genomes.json` + `best_genome.json` exist.
6. Check Unreal log for `[NEATTrainingManager] Best genome loaded:` after training completes.
7. Check `[NEATTrainingManager] Fitness attributed:` lines — every genome_id should appear once per generation.

## Common Failure Signatures

| Symptom | Likely Cause |
|---|---|
| `VALIDATION FAILED: Genome list not found: generation_N_genomes.json` | Python failed or wrong generation number |
| `VALIDATION FAILED: Observation size mismatch` | Python contract `num_inputs` != 15 |
| `ERROR: Missing fitness export for generation N` | Unreal didn't write fitness before calling Python |
| `Episode-done event rejected: agent_id=X not found in active batch` | Inactive agent fired (should be silent after isolation fix) |
| `Best genome could not be loaded after training` | Python did not export `best_genome.json` (only written in resume path) |
| `Python evolution failed!` | Check `Saved/Training/Logs/python_training_*.log` |
| `Missing backend / failed runtime assignment` | `UNEATGraphEvaluator::CreateFromGenome` returned nullptr — check activation functions in genome JSON |
| Fitness stuck at 0 for all genomes | `OnAgentEpisodeDone` not being called — verify agents are registered and `Initialize()` was called |

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
