# NEAT Training Loop – End-to-End Verification

This document describes a **lightweight verification flow** to confirm the Unreal ↔ Python NEAT loop is working and that failures are visible (no silent fallbacks).

## Prerequisites

- Unreal: NEATTrainingManager configured (fitness dir, genome dir, checkpoint dir, best genome path).
- Python: `train_neat.py` run via manifest from Unreal (no manual fallback paths).
- Contract: `observation_size` and `action_size` match between Unreal and NEAT config.

## Verification Steps

### 1. Unreal logs – contract and status

- **On StartTraining:** Look for:
  - `[NEATTrainingManager] NEAT contract (resolved):` with `FitnessDir`, `GenomeDir`, `CheckpointDir`, `BestGenomePath`, `ObservationSize`, `ActionSize`.
  - `[NEATTrainingManager] NEAT status [Start]: generation=0, active_genomes=0, completed_genomes=0, exported_fitness_file=(none), loaded_best_genome=(none)`.
- **After Python returns (each gen):** Look for:
  - `[NEATTrainingManager] NEAT status [AfterPythonLoad]: generation=N, active_genomes=M, ...` with `M > 0`.
- **After fitness export:** Look for:
  - `[NEATTrainingManager] Exported fitness for generation N (...) -> <path>`.
  - `[NEATTrainingManager] NEAT status [AfterExport]: ... exported_fitness_file=<path>`.
- **If you load best genome:** Look for:
  - `[NEATTrainingManager] Best genome file loaded: ...` and `NEAT status [LoadBestGenome]: ... loaded_best_genome=<path>`.

**Failure check:** Any `VALIDATION FAILED` in the log means the loop stopped intentionally (missing file, observation/action mismatch, unsupported activation, or failed fitness write). No silent skip.

### 2. Python logs – directories and export

- **On each run:** Look for:
  - `[NEAT contract] Resolved directories from manifest:` with `fitness_dir`, `genome_dir`, `checkpoint_dir`, `best_genome_path`, `observation_size`, `action_size`.
- **Fresh start:** Look for:
  - `[NEAT] Fresh start: no checkpoint found.`
  - `Exported N genomes for Unreal generation 0 -> generation_0_genomes.json`
  - `[NEAT] Exported generation file: <path>`
  - `[NEAT] Number of genomes exported: N`
- **Resume:** Look for:
  - `[NEAT] Resumed run: checkpoint file used: <path>`
  - `[NEAT] Resumed checkpoint: last exported Unreal generation = N`
  - After evolution: `[NEAT] Best genome summary: genome_id=X fitness=Y`
  - `[NEAT] Exported generation file: <path>`
  - `[NEAT] Number of genomes exported: N`

**Failure check:** If fitness for the last generation is missing, Python exits with `ERROR: Missing fitness export ...` and does not evolve with zero fitness.

### 3. Validation guards (no silent fallback)

| Check | Where | Behaviour |
|-------|--------|-----------|
| Observation size mismatch | Unreal: LoadGenerationGenomes, LoadBestGenome | Log `VALIDATION FAILED`, return false / stop training. |
| Action size mismatch | Unreal: LoadGenerationGenomes, LoadBestGenome | Same. |
| Missing genome list/file | Unreal: LoadGenerationGenomes | Log `VALIDATION FAILED: Genome list not found` or `Missing genome file`, return false. |
| Unsupported activation | Unreal: NEATGenomeImporter | LoadFromFile fails; LoadGenerationGenomes returns false and logs. |
| Incomplete generation evaluation | Unreal: TickEvaluation before export | Log error, call StopTraining(), do not export. |
| Fitness export write failure | Unreal: ExportFitnessValues | Log `VALIDATION FAILED: Failed to write fitness file`; do not clear map. |
| Fitness file missing on resume | Python: resume path | Exit with error; no zero-fitness evolution. |

### 4. Quick smoke test (one generation)

1. Start NEAT training in Unreal (one or more agents, e.g. 2 generations for a short test).
2. Confirm Unreal logs: `NEAT status [Start]`, then Python runs.
3. Confirm Python logs: resolved directories, then either “Fresh start” with export of `generation_0_genomes.json` or “Resumed checkpoint” with “Best genome summary” and export of next generation.
4. Back in Unreal: `NEAT status [AfterPythonLoad]` with `active_genomes > 0`, then after evaluation `Exported fitness for generation N -> <path>` and `NEAT status [AfterExport]` with `exported_fitness_file=<path>`.
5. If you run a second generation: Python should log “Resumed checkpoint” and load the fitness file just exported; no “Missing fitness export” error.

If any step fails, the logs (Unreal or Python) should show a clear **VALIDATION FAILED** or **ERROR** and the process should stop or exit without silently continuing.

## Manual QA checklist

- [ ] Unreal contract and NEAT status lines appear at start and after export / after Python load.
- [ ] Python resolved directories and export lines appear; genome count matches population.
- [ ] No “VALIDATION FAILED” or “ERROR: Missing fitness export” during a normal run.
- [ ] Intentionally break something (e.g. wrong observation size in contract, or delete a genome file): confirm failure is logged and training stops or Python exits.
- [ ] Load best genome: Unreal logs “Best genome file loaded” and “loaded_best_genome=<path>” in status.
