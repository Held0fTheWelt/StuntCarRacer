# CarAI — Structured Logging and Verbosity

## Log categories

| Category | Scope | Typical use |
|----------|--------|-------------|
| `LogCarAIEditor` | Editor module (widget, PIE hooks, menu) | Workflow state, registry polling, readiness, auto-registration, auto-start |
| `LogCarAITraining` | NEAT training manager | Contract manifest, genome load, batch assignment, fitness export, Python trigger, best-genome |
| `LogCarAIAgent` | Runtime (RacingAgentComponent, registry) | BeginPlay/EndPlay, registry register/unregister, agent state, authorization, step skip, episode |

## Log levels

- **Error / Warning**: Broken states, validation failures, timeouts, missing data.
- **Display**: Major state changes, contract written, batch start/done, readiness passed, training started.
- **Log**: Per-poll summary, registration summary (when verbosity ≥ 2 in widget).
- **Verbose**: Per-agent skip reasons, duplicate registration, auth skip, step count.
- **VeryVerbose**: Optional extra detail (e.g. per-component skip reasons when WorkflowLogVerbosity ≥ 3).

## Widget verbosity

The NEAT Training editor widget has **WorkflowLogVerbosity** (0–3):

- **0**: Only errors/warnings.
- **1**: + Display (state transitions, readiness, registration summary).
- **2**: + Log (per-poll registry count, poll summary).
- **3**: + Verbose (per-component skip reasons if any fallback path is used).

Adjust in the Details panel when the widget is open, or leave at 2 for normal diagnosis.

## Console commands (in-editor)

To increase verbosity at runtime (Output Log / console):

```
Log LogCarAIEditor Verbose
Log LogCarAITraining Verbose
Log LogCarAIAgent Verbose
```

To reduce noise:

```
Log LogCarAIEditor Warning
Log LogCarAITraining Warning
Log LogCarAIAgent Warning
```

## Proving the flow from logs

You should be able to confirm from logs alone:

1. **Source build**: `[CarAI] CarAIRuntime module loaded (plugin source build).` and same for CarAIEditor.
2. **Registry fill**: `[RacingAgentRegistry] Agent registered. Total=N` when PIE agents BeginPlay.
3. **Manager registration**: `[NEATTrainingManager] Agent registered ... Total registered: N` and `[NeatTrainingEditorWidget] Registration summary: ...`.
4. **Batch / authorization**: `[NEATTrainingManager] Genome assigned and authorization granted` and `[CarAIAgent] Evaluation authorization GRANTED`.
5. **First real step**: `[CarAIAgent] First step this episode. RuntimeState=... GenomeID=N` with GenomeID >= 0.
