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
2. Load fitness values from contract fitness_dir
3. NEAT evolves genomes; export to contract genome_dir
4. Unreal loads genomes and repeats

File naming (must match Unreal): generation_{N}.json, generation_{N}_genomes.json, genome_{id}.json, best_genome.json

Requirements:
    pip install neat-python

Usage (from Unreal):
    python train_neat.py --manifest <path_to_neat_contract.json>
"""

import argparse
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
REQUIRED_CONTRACT_KEYS = ("fitness_dir", "genome_dir", "checkpoint_dir", "best_genome_path", "observation_size", "action_size")

# ============================================================================
# NEAT Config Template
# ============================================================================

def _config_template(obs_size: int, action_size: int) -> str:
    return """
[NEAT]
fitness_criterion     = max
fitness_threshold     = 1000.0
pop_size              = 50
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

feed_forward            = True
initial_connection      = full

# node add/remove rates
node_add_prob           = 0.2
node_delete_prob        = 0.2

# network parameters
num_hidden              = 0
num_inputs              = {obs_size}
num_outputs             = {action_size}

# node response options
response_init_mean      = 1.0
response_init_stdev     = 0.0
response_max_value      = 30.0
response_min_value      = -30.0
response_mutate_power   = 0.0
response_mutate_rate    = 0.0
response_replace_rate   = 0.0

# connection weight options
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
""".format(obs_size=obs_size, action_size=action_size)

# ============================================================================
# Checkpoint (deterministic filename, one file per contract)
# ============================================================================

CHECKPOINT_FILENAME = "neat_checkpoint_latest.pkl"


def find_checkpoint(checkpoint_dir: str) -> Optional[Path]:
    """Return path to the single checkpoint file if it exists, else None."""
    path = Path(checkpoint_dir) / CHECKPOINT_FILENAME
    return path if path.is_file() else None


def load_checkpoint(checkpoint_path: Path) -> Tuple[neat.Population, neat.Config, int]:
    """Load (population, config, last_exported_unreal_generation). Raises on failure."""
    with open(checkpoint_path, "rb") as f:
        data = pickle.load(f)
    if len(data) != 3:
        raise ValueError(f"Checkpoint has wrong format: expected 3 items, got {len(data)}")
    population, config, last_exported = data[0], data[1], int(data[2])
    return population, config, last_exported


def save_checkpoint(checkpoint_path: Path, population: neat.Population, config: neat.Config, last_exported_unreal_gen: int) -> None:
    """Save checkpoint so the next run can resume."""
    checkpoint_path.parent.mkdir(parents=True, exist_ok=True)
    with open(checkpoint_path, "wb") as f:
        pickle.dump((population, config, last_exported_unreal_gen), f)


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
            print(f"⚠️  No fitness files found in {self.fitness_dir}")
            return {}
        
        latest_file = fitness_files[-1]
        print(f"📂 Loading fitness from: {latest_file.name}")
        
        with open(latest_file, 'r') as f:
            data = json.load(f)
        
        fitness_map = {}
        for genome_data in data.get("genomes", []):
            gid = genome_data["genome_id"]
            fitness = genome_data["fitness"]
            fitness_map[gid] = fitness
        
        print(f"   ✓ Loaded {len(fitness_map)} fitness values")
        return fitness_map

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
        
        # Export nodes
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
            print(f"💾 Exported genome {genome_id} to: {output_file.name}")


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
    print(f"📤 Exported {len(genomes)} genomes for Unreal generation {unreal_generation} -> {list_file.name}")

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
    return data


def log_resolved_contract(contract: dict) -> None:
    """Print the full resolved contract (must match Unreal startup logs)."""
    print("[NEAT contract] --- source of truth (from Unreal manifest) ---")
    print(f"[NEAT contract]   fitness_dir={contract['fitness_dir']}")
    print(f"[NEAT contract]   genome_dir={contract['genome_dir']}")
    print(f"[NEAT contract]   checkpoint_dir={contract['checkpoint_dir']}")
    print(f"[NEAT contract]   best_genome_path={contract['best_genome_path']}")
    print(f"[NEAT contract]   observation_size={contract['observation_size']} action_size={contract['action_size']}")
    print("[NEAT contract] --- end contract ---")


def create_default_config(config_path: str, obs_size: int, action_size: int):
    """Create default NEAT config file if it doesn't exist."""
    if not Path(config_path).exists():
        print(f"📝 Creating default config: {config_path}")
        with open(config_path, "w") as f:
            f.write(_config_template(obs_size, action_size))


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
    genome_dir_path = Path(genome_dir)
    checkpoint_dir_path = Path(checkpoint_dir)

    script_dir = Path(__file__).resolve().parent
    config_path = script_dir / CONFIG_FILE
    create_default_config(str(config_path), obs_size, action_size)
    config = neat.Config(
        neat.DefaultGenome,
        neat.DefaultReproduction,
        neat.DefaultSpeciesSet,
        neat.DefaultStagnation,
        str(config_path),
    )

    checkpoint_path = find_checkpoint(checkpoint_dir)

    if checkpoint_path is None:
        # Fresh start: create initial population, export generation_0 for Unreal, save checkpoint
        print("[NEAT] Fresh start: no checkpoint found.")
        population = neat.Population(config)
        population.add_reporter(neat.StdOutReporter(True))
        export_population_for_unreal(population, config, genome_dir_path, 0)
        save_checkpoint(checkpoint_dir_path / CHECKPOINT_FILENAME, population, config, 0)
        print(f"[NEAT] Exported generation file: {genome_dir_path / 'generation_0_genomes.json'}")
        print(f"[NEAT] Number of genomes exported: {len(population.population)}")
        print(f"[NEAT] Checkpoint saved: {checkpoint_dir_path / CHECKPOINT_FILENAME}")
        return

    # Resume: load checkpoint, assign fitness for that generation, run one reproduction, export next gen
    print(f"[NEAT] Resumed run: checkpoint file used: {checkpoint_path}")
    population, config, last_exported = load_checkpoint(checkpoint_path)
    print(f"[NEAT] Resumed checkpoint: last exported Unreal generation = {last_exported}")

    fitness_loader = FitnessLoader(fitness_dir)
    fitness_map = fitness_loader.load_for_generation(last_exported)
    fitness_file = Path(fitness_dir) / f"generation_{last_exported}.json"
    if not fitness_map:
        print(f"ERROR: Missing fitness export for generation {last_exported}. Expected: {fitness_file}", file=sys.stderr)
        print("[NEAT] Cannot resume without fitness; aborting to avoid silent zero-fitness evolution.", file=sys.stderr)
        sys.exit(1)

    for genome_id, genome in population.population.items():
        genome.fitness = fitness_map.get(genome_id, 0.0)

    def eval_only_assign(genomes, cfg):
        for gid, g in genomes:
            g.fitness = fitness_map.get(gid, 0.0)

    population.run(eval_only_assign, 1)
    next_gen = last_exported + 1

    # Best genome summary (after reproduction)
    best_id = max(population.population.keys(), key=lambda gid: population.population[gid].fitness)
    best_fitness = population.population[best_id].fitness
    print(f"[NEAT] Best genome summary: genome_id={best_id} fitness={best_fitness:.2f}")

    export_population_for_unreal(population, config, genome_dir_path, next_gen)
    list_path = genome_dir_path / f"generation_{next_gen}_genomes.json"
    print(f"[NEAT] Exported generation file: {list_path}")
    print(f"[NEAT] Number of genomes exported: {len(population.population)}")
    save_checkpoint(checkpoint_dir_path / CHECKPOINT_FILENAME, population, config, next_gen)
    print(f"[NEAT] Checkpoint saved (Unreal generation {next_gen}).")


if __name__ == "__main__":
    main()