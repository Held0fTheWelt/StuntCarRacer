#!/usr/bin/env python3
"""
NEAT Training for Racing AI
=============================

Uses NEAT-Python to evolve neural networks for racing car control.
Unreal is the single source of truth: run with --manifest <path> to neat_contract.json
written by NEATTrainingManager. All paths and sizes come from the manifest; no hardcoded
NEAT directories or fallbacks.

Workflow:
1. Load contract from Unreal (fitness_dir, genome_dir, checkpoint_dir, best_genome_path, observation_size, action_size)
2. If checkpoint exists: resume population and last_exported_generation; load fitness for that generation; evolve once; export next generation.
3. If no checkpoint: fresh start; create population; export generation_0; save checkpoint.
4. Unreal loads the exported generation file, evaluates, writes fitness, then calls Python again. Generation numbering is sequential (0, 1, 2, ...) and aligned with Unreal's expected filenames.

File naming (must match Unreal): generation_{N}.json (fitness), generation_{N}_genomes.json (list), genome_{id}.json, best_genome.json. Checkpoint: neat_checkpoint_latest.pkl in checkpoint_dir.

Requirements:
    pip install neat-python

Usage (from Unreal):
    python train_neat.py --manifest <path_to_neat_contract.json>
"""

import argparse
import ast
import copyreg
from datetime import datetime
import itertools
import json
import sys
import neat
import pickle
from pathlib import Path
from typing import Dict, List, Tuple, Optional

# ============================================================================
# Contract: all values from Unreal manifest only (no hardcoded NEAT paths)
# ============================================================================

CONFIG_FILE = "neat_config.txt"
REQUIRED_CONTRACT_KEYS = ("fitness_dir", "genome_dir", "checkpoint_dir", "best_genome_path", "observation_size", "action_size", "population_size")

# Canonical NEAT observation size and layout (must match Unreal FRacingObservation::NEAT_OBSERVATION_SIZE and BuildVectorForNEAT order).
# LIDAR and other optional sensors are not part of the NEAT contract.
NEAT_OBSERVATION_SIZE = 15
NEAT_OBSERVATION_LAYOUT = (
    "SpeedNorm,YawRateNorm,PitchRateNorm,RollRateNorm,"
    "RayForward,RayLeft,RayRight,RayLeft45,RayRight45,RayForwardUp,RayForwardDown,RayGroundDist,"
    "GravityX,GravityY,GravityZ"
)
MIN_TRAINING_POPULATION_SIZE = 3


def _reduce_itertools_count(counter: itertools.count) -> tuple:
    """
    Python 3.14 no longer pickles itertools.count by default, but neat-python
    stores count objects in its reproduction/species state. Preserve the current
    counter position so Population checkpoints remain portable.
    """
    repr_text = repr(counter)
    if not repr_text.startswith("count(") or not repr_text.endswith(")"):
        raise TypeError(f"Unsupported itertools.count repr: {repr_text}")
    args_text = repr_text[len("count("):-1]
    args = tuple(ast.literal_eval(part.strip()) for part in args_text.split(",") if part.strip())
    return itertools.count, args


copyreg.pickle(type(itertools.count()), _reduce_itertools_count)

# ============================================================================
# NEAT Config Template
# ============================================================================

def _config_template(obs_size: int, action_size: int, pop_size: int) -> str:
    return """
[NEAT]
fitness_criterion     = max
fitness_threshold     = 1000.0
no_fitness_termination = False
pop_size              = {pop_size}
reset_on_extinction   = False

[DefaultGenome]
# node activation options
activation_default      = tanh
activation_mutate_rate  = 0.1
activation_options      = sigmoid tanh relu

# node aggregation options
aggregation_default     = sum
aggregation_mutate_rate = 0.0
aggregation_options     = sum

# node bias options
bias_init_type          = gaussian
bias_init_mean          = 0.0
bias_init_stdev         = 1.0
bias_max_value          = 30.0
bias_min_value          = -30.0
bias_mutate_power       = 0.5
bias_mutate_rate        = 0.7
bias_replace_rate       = 0.1

# genome compatibility options
compatibility_disjoint_coefficient = 1.0
compatibility_weight_coefficient   = 0.5

# connection add/remove rates
conn_add_prob           = 0.5
conn_delete_prob        = 0.5

# connection enable options
enabled_default         = True
enabled_mutate_rate     = 0.01
enabled_rate_to_true_add  = 0.0
enabled_rate_to_false_add = 0.0

feed_forward            = True
initial_connection      = full

# node add/remove rates
node_add_prob           = 0.2
node_delete_prob        = 0.2

# structural mutation options (required by neat-python >= 0.92)
single_structural_mutation = false
structural_mutation_surer  = default

# network parameters
num_hidden              = 0
num_inputs              = {obs_size}
num_outputs             = {action_size}

# node response options
response_init_type      = gaussian
response_init_mean      = 1.0
response_init_stdev     = 0.0
response_max_value      = 30.0
response_min_value      = -30.0
response_mutate_power   = 0.0
response_mutate_rate    = 0.0
response_replace_rate   = 0.0

# connection weight options
weight_init_type        = gaussian
weight_init_mean        = 0.0
weight_init_stdev       = 1.0
weight_max_value        = 30.0
weight_min_value        = -30.0
weight_mutate_power     = 0.5
weight_mutate_rate      = 0.8
weight_replace_rate     = 0.1

[DefaultSpeciesSet]
compatibility_threshold = 3.0

[DefaultStagnation]
species_fitness_func = max
max_stagnation       = 15
species_elitism      = 2

[DefaultReproduction]
elitism            = 2
survival_threshold = 0.2
min_species_size   = 1
""".format(obs_size=obs_size, action_size=action_size, pop_size=pop_size)

# ============================================================================
# Checkpoint (deterministic filename; one file per contract dir; enables resume)
# ============================================================================

CHECKPOINT_FILENAME = "neat_checkpoint_latest.pkl"


def get_checkpoint_path(checkpoint_dir: Path) -> Path:
    """Return the canonical checkpoint file path. Does not check existence."""
    return checkpoint_dir / CHECKPOINT_FILENAME


def find_checkpoint(checkpoint_dir: Path) -> Optional[Path]:
    """Return path to the checkpoint file if it exists, else None. Logs path checked."""
    path = get_checkpoint_path(checkpoint_dir)
    if path.is_file():
        return path
    return None


def load_checkpoint(
    checkpoint_path: Path,
    expected_inputs: int,
    expected_outputs: int,
    expected_population_size: int,
) -> Tuple[neat.Population, neat.Config, int]:
    """
    Load (population, config, last_exported_unreal_generation).
    Validates checkpoint format and that config matches contract observation/action size.
    Raises on failure.
    """
    with open(checkpoint_path, "rb") as f:
        data = pickle.load(f)
    if len(data) != 3:
        raise ValueError(f"Checkpoint has wrong format: expected 3 items, got {len(data)}")
    population, config, last_exported = data[0], data[1], int(data[2])
    if config.genome_config.num_inputs != expected_inputs or config.genome_config.num_outputs != expected_outputs:
        raise ValueError(
            f"Checkpoint config mismatch: checkpoint has num_inputs={config.genome_config.num_inputs} "
            f"num_outputs={config.genome_config.num_outputs}, contract expects {expected_inputs} / {expected_outputs}"
        )
    population_count = len(population.population)
    if population_count != expected_population_size:
        raise ValueError(
            f"Checkpoint population mismatch: checkpoint has {population_count} genomes, "
            f"contract expects population_size={expected_population_size}"
        )
    return population, config, last_exported


def save_checkpoint(
    checkpoint_path: Path,
    population: neat.Population,
    config: neat.Config,
    last_exported_unreal_gen: int,
) -> None:
    """Save checkpoint so the next run can resume from this generation."""
    checkpoint_path.parent.mkdir(parents=True, exist_ok=True)
    with open(checkpoint_path, "wb") as f:
        pickle.dump((population, config, last_exported_unreal_gen), f)


def quarantine_checkpoint(checkpoint_path: Path) -> Path:
    """Move an unusable checkpoint aside without deleting it, then return the new path."""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    target = checkpoint_path.with_name(
        f"{checkpoint_path.stem}.invalid_{timestamp}{checkpoint_path.suffix}"
    )
    counter = 1
    while target.exists():
        target = checkpoint_path.with_name(
            f"{checkpoint_path.stem}.invalid_{timestamp}_{counter}{checkpoint_path.suffix}"
        )
        counter += 1
    checkpoint_path.replace(target)
    return target


def export_initial_population_for_unreal(
    config: neat.Config,
    genome_dir_path: Path,
    checkpoint_path: Path,
) -> None:
    """Create a fresh population, export generation 0, and save the matching checkpoint."""
    population = neat.Population(config)
    population.add_reporter(neat.StdOutReporter(True))
    export_population_for_unreal(population, config, genome_dir_path, 0)
    list_path_0 = genome_dir_path / "generation_0_genomes.json"
    print(f"[NEAT] Exported generation file: {list_path_0} (generation 0 for Unreal to evaluate)")
    print(f"[NEAT] Number of genomes exported: {len(population.population)}")
    save_checkpoint(checkpoint_path, population, config, 0)
    print(f"[NEAT] Checkpoint saved: {checkpoint_path} (last_exported_generation=0)")
    write_training_state(genome_dir_path, 0)


def reexport_population_for_unreal(
    population: neat.Population,
    config: neat.Config,
    genome_dir_path: Path,
    generation: int,
    reason: str,
) -> None:
    """Export an already-checkpointed generation again without evolving it."""
    print(f"[NEAT] Re-exporting generation {generation}: {reason}")
    export_population_for_unreal(population, config, genome_dir_path, generation)
    list_path = genome_dir_path / f"generation_{generation}_genomes.json"
    print(f"[NEAT] Re-exported generation file: {list_path} (generation {generation} for Unreal to evaluate)")
    print(f"[NEAT] Number of genomes exported: {len(population.population)}")
    write_training_state(genome_dir_path, generation)


# ============================================================================
# Fitness Loader
# ============================================================================

class FitnessLoader:
    """Loads fitness values from Unreal JSON exports."""
    
    def __init__(self, fitness_dir: str):
        self.fitness_dir = Path(fitness_dir)
        
    def load_for_generation(self, generation: int) -> Dict[int, float]:
        """Load fitness for a specific Unreal generation. Returns genome_id -> fitness."""
        path = self.fitness_dir / f"generation_{generation}.json"
        if not path.is_file():
            return {}
        with open(path, "r") as f:
            data = json.load(f)
        fitness_map = {}
        for genome_data in data.get("genomes", []):
            fitness_map[genome_data["genome_id"]] = genome_data["fitness"]
        return fitness_map

    def load_latest_generation(self) -> Dict[int, float]:
        """
        Load fitness values for latest generation.
        
        Expected JSON format:
        {
            "generation": 0,
            "genomes": [
                {"genome_id": 0, "fitness": 45.2},
                {"genome_id": 1, "fitness": 32.1},
                ...
            ]
        }
        
        Returns:
            Dict mapping genome_id -> fitness
        """
        fitness_files = sorted(self.fitness_dir.glob("generation_*.json"))
        
        if not fitness_files:
            print(f"[NEAT] No fitness files found in {self.fitness_dir}")
            return {}
        
        latest_file = fitness_files[-1]
        print(f"[NEAT] Loading fitness from: {latest_file.name}")
        
        with open(latest_file, 'r') as f:
            data = json.load(f)
        
        fitness_map = {}
        for genome_data in data.get("genomes", []):
            gid = genome_data["genome_id"]
            fitness = genome_data["fitness"]
            fitness_map[gid] = fitness
        
        print(f"[NEAT] Loaded {len(fitness_map)} fitness values")
        return fitness_map


def validate_fitness_map_for_population(
    fitness_map: Dict[int, float],
    population_genome_ids,
    generation: int,
) -> None:
    """Raise ValueError if Unreal's fitness file does not exactly match the evaluated population."""
    expected_ids = {int(gid) for gid in population_genome_ids}
    actual_ids = {int(gid) for gid in fitness_map.keys()}
    missing = sorted(expected_ids - actual_ids)
    extra = sorted(actual_ids - expected_ids)
    if missing or extra:
        details = []
        if missing:
            details.append(f"missing genome IDs: {missing}")
        if extra:
            details.append(f"unexpected genome IDs: {extra}")
        raise ValueError(
            f"Fitness file for generation {generation} does not match checkpoint population "
            f"(expected {len(expected_ids)} IDs, got {len(actual_ids)}): " + "; ".join(details)
        )


# ============================================================================
# Genome Exporter
# ============================================================================

class GenomeExporter:
    """Exports NEAT genome to JSON for Unreal."""
    
    @staticmethod
    def export_genome(genome, genome_id: int, generation: int, fitness: float,
                      output_path: str, config: neat.Config, verbose: bool = True):
        """
        Export genome to JSON format compatible with Unreal.
        
        JSON Format:
        {
            "genome_id": 0,
            "generation": 5,
            "fitness": 123.45,
            "num_inputs": 10,
            "num_outputs": 3,
            "nodes": [
                {"id": 0, "activation": "tanh", "bias": 0.5, "response": 1.0},
                ...
            ],
            "connections": [
                {"in_node": 0, "out_node": 10, "weight": 1.23, "enabled": true},
                ...
            ]
        }
        """
        data = {
            "genome_id": genome_id,
            "generation": generation,
            "fitness": fitness,
            "num_inputs": config.genome_config.num_inputs,
            "num_outputs": config.genome_config.num_outputs,
            "nodes": [],
            "connections": []
        }

        # Input pins in NEAT have negative keys (-1 .. -num_inputs). Unreal expects every
        # connection endpoint to exist in the nodes array, so add synthetic input nodes.
        for input_key in config.genome_config.input_keys:
            data["nodes"].append({
                "id": input_key,
                "activation": "tanh",
                "bias": 0.0,
                "response": 1.0
            })

        # Export real nodes (output + hidden)
        for node_id, node in genome.nodes.items():
            data["nodes"].append({
                "id": node_id,
                "activation": node.activation,
                "bias": node.bias,
                "response": node.response
            })
        
        # Export connections
        for conn_key, conn in genome.connections.items():
            in_node, out_node = conn_key
            data["connections"].append({
                "in_node": in_node,
                "out_node": out_node,
                "weight": conn.weight,
                "enabled": conn.enabled
            })
        
        # Write to file
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        if verbose:
            print(f"[NEAT] Exported genome {genome_id} to: {output_file.name}")


TRAINING_STATE_FILENAME = "training_state.json"


def get_training_state_path(genome_dir: Path) -> Path:
    """
    Return the canonical path for training_state.json.
    Canonical location: genome_dir / training_state.json.
    All writers, readers, and deleters must use this function — do not construct
    the path inline from any other directory variable (e.g. checkpoint_dir).
    Unreal reads from Contract.GenomeDir + TrainingStateFileName; this must match.
    """
    return genome_dir / TRAINING_STATE_FILENAME


def write_training_state(genome_dir: Path, exported_generation: int) -> None:
    """
    Write training_state.json to the canonical location (genome_dir).
    Unreal reads exported_generation from this file to synchronize CurrentGeneration
    before calling LoadGenerationGenomes(). This is the canonical source of truth
    for which generation Python just exported.
    """
    state_file = get_training_state_path(genome_dir)
    with open(state_file, "w") as f:
        json.dump({"exported_generation": exported_generation}, f, indent=2)
    print(f"[NEAT] Training state written: {state_file} (exported_generation={exported_generation})")


def export_population_for_unreal(
    population: neat.Population,
    config: neat.Config,
    output_dir: Path,
    unreal_generation: int,
) -> None:
    """
    Export current population as generation_{unreal_generation}_genomes.json and genome_{id}.json.
    Deterministic naming for Unreal.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    genomes = list(population.population.items())  # (genome_id, genome)

    list_file = output_dir / f"generation_{unreal_generation}_genomes.json"
    genome_list = [{"genome_id": gid, "generation": unreal_generation} for gid, _ in genomes]
    with open(list_file, "w") as f:
        json.dump({"generation": unreal_generation, "population_size": len(genome_list), "genomes": genome_list}, f, indent=2)
    print(f"[NEAT] Exported {len(genomes)} genomes for Unreal generation {unreal_generation} -> {list_file.name}")

    for genome_id, genome in genomes:
        genome_file = output_dir / f"genome_{genome_id}.json"
        GenomeExporter.export_genome(genome, genome_id, unreal_generation, getattr(genome, "fitness", 0.0), str(genome_file), config, verbose=False)


# ============================================================================
# Main
# ============================================================================

def load_contract(manifest_path: str) -> dict:
    """Load contract JSON written by Unreal. No fallbacks; fail if missing/invalid."""
    path = Path(manifest_path)
    if not path.is_file():
        print(f"ERROR: Manifest file not found: {manifest_path}", file=sys.stderr)
        sys.exit(1)
    with open(path, "r") as f:
        data = json.load(f)
    for key in REQUIRED_CONTRACT_KEYS:
        if key not in data:
            print(f"ERROR: Contract missing required key: {key}", file=sys.stderr)
            sys.exit(1)
    obs = int(data["observation_size"])
    if obs != NEAT_OBSERVATION_SIZE:
        print(
            f"ERROR: Observation size mismatch: contract observation_size={obs}, NEAT path requires {NEAT_OBSERVATION_SIZE}. "
            "Unreal and Python must agree; optional sensors (e.g. LIDAR) are not supported in the NEAT contract.",
            file=sys.stderr,
        )
        sys.exit(1)
    action_size = int(data["action_size"])
    if action_size != 3:
        print(
            f"ERROR: Action size mismatch: contract action_size={action_size}, NEAT path requires 3 (steer, throttle, brake).",
            file=sys.stderr,
        )
        sys.exit(1)
    population_size = int(data["population_size"])
    if population_size < MIN_TRAINING_POPULATION_SIZE:
        print(
            f"ERROR: population_size={population_size} is too small for NEAT training. "
            f"Minimum is {MIN_TRAINING_POPULATION_SIZE}; use one Unreal agent with a population >= {MIN_TRAINING_POPULATION_SIZE} for sequential batch evaluation.",
            file=sys.stderr,
        )
        sys.exit(1)
    return data


def log_resolved_contract(contract: dict) -> None:
    """Print the full resolved contract and NEAT observation schema (must match Unreal)."""
    print("[NEAT contract] --- source of truth (from Unreal manifest) ---")
    print(f"[NEAT contract]   fitness_dir={contract['fitness_dir']}")
    print(f"[NEAT contract]   genome_dir={contract['genome_dir']}")
    print(f"[NEAT contract]   checkpoint_dir={contract['checkpoint_dir']}")
    print(f"[NEAT contract]   best_genome_path={contract['best_genome_path']}")
    print(f"[NEAT contract]   observation_size={contract['observation_size']} action_size={contract['action_size']} population_size={contract['population_size']}")
    print(f"[NEAT contract] NEAT observation schema: size={NEAT_OBSERVATION_SIZE} layout={NEAT_OBSERVATION_LAYOUT} (LIDAR not used)")
    print(f"[NEAT contract] Population parity: manifest population_size={contract['population_size']} -> neat_config pop_size")
    print("[NEAT contract] --- end contract ---")


# Required fields per INI section that must be present for neat-python compatibility.
# Keys are INI section names; values are tuples of required field names.
# Extend this dict when future neat-python versions add more required fields.
REQUIRED_CONFIG_FIELDS: Dict[str, Tuple[str, ...]] = {
    "NEAT": ("no_fitness_termination",),
    "DefaultReproduction": ("min_species_size",),
    "DefaultGenome": (
        "single_structural_mutation", "structural_mutation_surer", "bias_init_type",
        "response_init_type", "weight_init_type",
        "enabled_rate_to_true_add", "enabled_rate_to_false_add",
    ),
}


def _parse_ini_sections(text: str) -> Dict[str, str]:
    """
    Parse an INI-style config into {section_name: section_body} dict.
    Section body is the raw text between the section header and the next header (or EOF).
    """
    import re
    sections: Dict[str, str] = {}
    current_section: Optional[str] = None
    body_lines: List[str] = []
    for line in text.splitlines():
        header = re.match(r"^\[(\w+)\]", line.strip())
        if header:
            if current_section is not None:
                sections[current_section] = "\n".join(body_lines)
            current_section = header.group(1)
            body_lines = []
        else:
            body_lines.append(line)
    if current_section is not None:
        sections[current_section] = "\n".join(body_lines)
    return sections


def _find_missing_fields(config_text: str) -> List[str]:
    """
    Return list of 'section.field' strings that are required but absent in config_text.
    Checks each required field only within the correct INI section body.
    """
    import re
    sections = _parse_ini_sections(config_text)
    missing: List[str] = []
    for section, fields in REQUIRED_CONFIG_FIELDS.items():
        body = sections.get(section, "")
        for field in fields:
            if not re.search(rf"^\s*{re.escape(field)}\s*=", body, re.MULTILINE):
                missing.append(f"[{section}] {field}")
    return missing


def create_or_validate_config(config_path: str, obs_size: int, action_size: int, population_size: int) -> None:
    """
    Ensure neat_config.txt exists, is schema-compatible with the installed neat-python
    version, and has pop_size driven by the Unreal manifest.

    Decision logic (in order):
    1. File missing           → CREATED from current template.
    2. File has missing required fields (schema mismatch) → REGENERATED from template.
    3. File schema-OK but wrong pop_size (parity mismatch) → REGENERATED from template.
    4. File schema-OK and pop_size matches manifest → KEPT as-is.

    "KEPT" is only printed when the pre-parse field check passes.
    True schema validation (neat.Config() parse) happens in main() after this call.
    """
    import re

    path = Path(config_path)
    fresh_content = _config_template(obs_size, action_size, population_size)
    print(f"[NEAT] Config path: {path}")

    def _write_and_log(reason: str) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            f.write(fresh_content)
        print(f"[NEAT] Config status: {reason}")
        print(f"[NEAT] Config population parity: pop_size={population_size} (source: manifest)")

    if not path.exists():
        _write_and_log("CREATED (file did not exist)")
        return

    existing = path.read_text(encoding="utf-8")

    missing = _find_missing_fields(existing)
    if missing:
        _write_and_log(f"REGENERATED (schema mismatch — missing fields: {missing})")
        return

    pop_match = re.search(r"^\s*pop_size\s*=\s*(\d+)", existing, re.MULTILINE)
    existing_pop = int(pop_match.group(1)) if pop_match else None
    if existing_pop != population_size:
        _write_and_log(
            f"REGENERATED (population parity: config pop_size={existing_pop} != manifest {population_size})"
        )
        return

    print(f"[NEAT] Config status: KEPT (all required fields present, pop_size={population_size} matches manifest)")
    print(f"[NEAT] Config population parity: pop_size={population_size} (source: manifest)")


def main():
    parser = argparse.ArgumentParser(description="NEAT training driven by Unreal contract")
    parser.add_argument("--manifest", required=True, help="Path to neat_contract.json from NEATTrainingManager")
    args = parser.parse_args()

    contract = load_contract(args.manifest)
    log_resolved_contract(contract)

    fitness_dir = contract["fitness_dir"]
    genome_dir = contract["genome_dir"]
    checkpoint_dir = contract["checkpoint_dir"]
    obs_size = int(contract["observation_size"])
    action_size = int(contract["action_size"])
    population_size = int(contract["population_size"])
    genome_dir_path = Path(genome_dir)
    checkpoint_dir_path = Path(checkpoint_dir)

    script_dir = Path(__file__).resolve().parent
    config_path = script_dir / CONFIG_FILE
    create_or_validate_config(str(config_path), obs_size, action_size, population_size)
    try:
        config = neat.Config(
            neat.DefaultGenome,
            neat.DefaultReproduction,
            neat.DefaultSpeciesSet,
            neat.DefaultStagnation,
            str(config_path),
        )
    except Exception as e:
        error_text = str(e)
        # Extract the missing field name from the neat-python error message when possible.
        # neat-python raises: "Missing required configuration item in [Section]: 'field'"
        import re as _re
        neat_missing = _re.search(r"Missing required configuration item in \[(\w+)\][^']*'([^']+)'", error_text)
        if neat_missing:
            bad_section, bad_field = neat_missing.group(1), neat_missing.group(2)
            hint = (
                f"  Missing field '{bad_field}' in [{bad_section}] section of: {config_path}\n"
                f"  Add '{bad_field} = <value>' to [{bad_section}], or delete '{config_path}' to force full regeneration.\n"
                f"  Also add '{bad_section}.{bad_field}' to REQUIRED_CONFIG_FIELDS in train_neat.py to prevent future breakage."
            )
        else:
            hint = (
                f"  Config file used: {config_path}\n"
                f"  Raw error: {error_text}\n"
                f"  Delete '{config_path}' and re-run to force full regeneration from the current template."
            )
        print(f"ERROR: neat.Config() schema parse failed.\n{hint}", file=sys.stderr)
        sys.exit(1)
    print(f"[NEAT] Config compatibility: OK (neat.Config loaded successfully from {config_path})")

    # Single canonical checkpoint path (contract checkpoint_dir + fixed filename)
    checkpoint_path = get_checkpoint_path(checkpoint_dir_path)

    # training_mode: explicit fresh-vs-resume control from Unreal manifest.
    # "fresh" = discard checkpoint and best genome before running (clean slate).
    # "resume" = load latest checkpoint if available (default).
    training_mode = contract.get("training_mode", "resume")
    print(f"[NEAT] Training mode: {training_mode}")
    if training_mode == "fresh":
        if checkpoint_path.is_file():
            checkpoint_path.unlink()
            print(f"[NEAT] Fresh mode: deleted checkpoint {checkpoint_path}")
        else:
            print(f"[NEAT] Fresh mode: no checkpoint to delete at {checkpoint_path}")
        best_genome_path_val = contract.get("best_genome_path", "")
        if best_genome_path_val and Path(best_genome_path_val).is_file():
            Path(best_genome_path_val).unlink()
            print(f"[NEAT] Fresh mode: deleted best genome {best_genome_path_val}")
        state_file = get_training_state_path(genome_dir_path)
        if state_file.is_file():
            state_file.unlink()
            print(f"[NEAT] Fresh mode: deleted training state {state_file}")
    elif training_mode == "resume":
        print(f"[NEAT] Resume mode: will load checkpoint if found at {checkpoint_path}")
    else:
        print(f"ERROR: Unknown training_mode '{training_mode}'; expected 'fresh' or 'resume'", file=sys.stderr)
        sys.exit(1)

    resolved_checkpoint = find_checkpoint(checkpoint_dir_path)

    if resolved_checkpoint is None:
        # Fresh start: no checkpoint exists; create initial population, export generation_0, save checkpoint
        print(f"[NEAT] Fresh start: no checkpoint at {checkpoint_path}")
        export_initial_population_for_unreal(config, genome_dir_path, checkpoint_path)
        return

    # Resume: load checkpoint, load fitness for last_exported generation, run one reproduction, export next gen
    print(f"[NEAT] Resumed run: checkpoint found at {resolved_checkpoint}")
    try:
        population, config, last_exported = load_checkpoint(
            resolved_checkpoint, obs_size, action_size, population_size
        )
    except Exception as e:
        print(f"[NEAT] WARNING: Failed to load checkpoint: {e}", file=sys.stderr)
        try:
            quarantined_path = quarantine_checkpoint(resolved_checkpoint)
        except OSError as move_error:
            print(
                f"ERROR: Failed to quarantine unusable checkpoint {resolved_checkpoint}: {move_error}",
                file=sys.stderr,
            )
            sys.exit(1)
        print(f"[NEAT] Quarantined unusable checkpoint: {quarantined_path}", file=sys.stderr)
        print("[NEAT] Starting fresh generation 0 because the checkpoint cannot be resumed.", file=sys.stderr)
        export_initial_population_for_unreal(config, genome_dir_path, checkpoint_path)
        return
    print(f"[NEAT] Checkpoint used: {resolved_checkpoint} (last_exported_generation={last_exported})")
    print(
        f"[NEAT] Resumed generation: {last_exported} "
        f"(will evolve to {last_exported + 1} only after matching fitness is available)"
    )

    # Patch pop_size from manifest: checkpoint may have a stale pop_size embedded in its config
    # object (neat-python pickles the entire Config, so changing the file does not update the in-memory
    # object). If population.run() uses the stale pop_size it will crash in reproduction when the new
    # pop_size requires more elites than the old population had individuals.
    if config.pop_size != population_size:
        print(f"[NEAT] Checkpoint pop_size={config.pop_size} differs from manifest pop_size={population_size}; patching in-memory config.")
        config.pop_size = population_size
        population.config.pop_size = population_size

    fitness_loader = FitnessLoader(fitness_dir)
    fitness_map = fitness_loader.load_for_generation(last_exported)
    fitness_file = Path(fitness_dir) / f"generation_{last_exported}.json"
    if not fitness_map:
        reexport_population_for_unreal(
            population,
            config,
            genome_dir_path,
            last_exported,
            f"missing fitness export at {fitness_file}",
        )
        return
    try:
        validate_fitness_map_for_population(fitness_map, population.population.keys(), last_exported)
    except ValueError as e:
        reexport_population_for_unreal(
            population,
            config,
            genome_dir_path,
            last_exported,
            f"fitness export does not match checkpointed population ({e})",
        )
        return
    print(f"[NEAT] Fitness loaded: generation={last_exported}, {len(fitness_map)} genomes")

    for genome_id, genome in population.population.items():
        genome.fitness = fitness_map[genome_id]

    # Capture best genome from pre-reproduction population (before population.run() modifies it)
    best_id = max(population.population.keys(), key=lambda gid: population.population[gid].fitness)
    best_genome_obj = population.population[best_id]
    best_fitness = best_genome_obj.fitness
    print(f"[NEAT] Best genome from evaluated generation {last_exported}: genome_id={best_id} fitness={best_fitness:.2f}")

    def eval_only_assign(genomes, cfg):
        for gid, g in genomes:
            g.fitness = fitness_map[gid]

    population.run(eval_only_assign, 1)
    next_gen = last_exported + 1

    export_population_for_unreal(population, config, genome_dir_path, next_gen)
    list_path = genome_dir_path / f"generation_{next_gen}_genomes.json"
    print(f"[NEAT] Exported generation file: {list_path} (generation {next_gen} for Unreal to evaluate)")
    print(f"[NEAT] Number of genomes exported: {len(population.population)}")
    save_checkpoint(checkpoint_path, population, config, next_gen)
    print(f"[NEAT] Checkpoint saved: {checkpoint_path} (last_exported_generation={next_gen})")
    write_training_state(genome_dir_path, next_gen)

    # Export pre-reproduction best genome to best_genome_path from contract
    best_genome_path = contract["best_genome_path"]
    Path(best_genome_path).parent.mkdir(parents=True, exist_ok=True)
    GenomeExporter.export_genome(best_genome_obj, best_id, last_exported, best_fitness, str(best_genome_path), config)
    print(f"[NEAT] Best genome exported: path={best_genome_path} genome_id={best_id} fitness={best_fitness:.2f}")


if __name__ == "__main__":
    main()
