#!/usr/bin/env python3
"""
NEAT Training for Racing AI
=============================

Uses NEAT-Python to evolve neural networks for racing car control.

Workflow:
1. Load fitness values from Unreal (JSON exports)
2. NEAT evolves genomes based on fitness
3. Export best genome to JSON for Unreal to load

Requirements:
    pip install neat-python

Usage:
    python train_neat.py [fitness_dir] [output_dir] [num_generations]
"""

import os
import sys
import json
import neat
import pickle
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple, Optional

# ============================================================================
# Configuration
# ============================================================================

# Observation size (from RacingAgentTypes.h)
# Adaptive Ray System with IMU Sensor
OBS_SIZE = 16  # [Speed, YawRate, PitchRate, RollRate, 
                # RayFwd, RayL, RayR, RayL45, RayR45, RayFUp, RayFDown, RayGround,
                # GravityX, GravityY, GravityZ]

# Action size
ACTION_SIZE = 3  # [Steer, Throttle, Brake]

# NEAT Config file
CONFIG_FILE = "neat_config.txt"

# ============================================================================
# NEAT Config Template
# ============================================================================

NEAT_CONFIG_TEMPLATE = """
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
""".format(obs_size=OBS_SIZE, action_size=ACTION_SIZE)

# ============================================================================
# Fitness Loader
# ============================================================================

class FitnessLoader:
    """Loads fitness values from Unreal JSON exports."""
    
    def __init__(self, fitness_dir: str):
        self.fitness_dir = Path(fitness_dir)
        
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
                     output_path: str, config: neat.Config):
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
        
        print(f"💾 Exported genome {genome_id} to: {output_file.name}")

# ============================================================================
# NEAT Trainer
# ============================================================================

class NEATTrainer:
    """Main NEAT training loop."""
    
    def __init__(self, config_file: str, fitness_dir: str, output_dir: str):
        self.config = neat.Config(
            neat.DefaultGenome,
            neat.DefaultReproduction,
            neat.DefaultSpeciesSet,
            neat.DefaultStagnation,
            config_file
        )
        
        self.fitness_loader = FitnessLoader(fitness_dir)
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        self.population = neat.Population(self.config)
        self.stats = neat.StatisticsReporter()
        self.population.add_reporter(self.stats)
        self.population.add_reporter(neat.StdOutReporter(True))
        
        self.generation = 0
        self.best_genome = None
        self.best_fitness = 0.0
    
    def evaluate_generation(self, genomes, config):
        """
        Evaluate fitness for all genomes.
        
        This is called by NEAT after spawning a new generation.
        We load fitness values from Unreal's JSON exports.
        """
        print(f"\n🧬 Generation {self.generation}")
        print("=" * 60)
        
        # Load fitness values from Unreal
        fitness_map = self.fitness_loader.load_latest_generation()
        
        if not fitness_map:
            print("⚠️  No fitness values loaded! Using default fitness = 0.0")
        
        # Assign fitness to genomes
        for genome_id, genome in genomes:
            genome.fitness = fitness_map.get(genome_id, 0.0)
            
            # Track best genome
            if genome.fitness > self.best_fitness:
                self.best_fitness = genome.fitness
                self.best_genome = genome
                print(f"🏆 New best genome! ID={genome_id}, Fitness={genome.fitness:.2f}")
        
        # Export current generation genomes for Unreal to evaluate
        self.export_generation_for_evaluation(genomes, config)
        
        # Export best genome
        if self.best_genome:
            self.export_best_genome(config)
        
        self.generation += 1
    
    def export_generation_for_evaluation(self, genomes, config):
        """
        Export all genomes for Unreal to evaluate.
        
        Unreal will:
        1. Load this file
        2. Spawn agents with these genomes
        3. Evaluate fitness
        4. Export fitness values back to fitness_dir
        """
        output_file = self.output_dir / f"generation_{self.generation}_genomes.json"
        
        genome_list = []
        for genome_id, genome in genomes:
            # Simplified genome export (just ID for now - Unreal will load full genome separately)
            genome_list.append({
                "genome_id": genome_id,
                "generation": self.generation
            })
        
        data = {
            "generation": self.generation,
            "population_size": len(genome_list),
            "genomes": genome_list
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
        
        print(f"📤 Exported {len(genome_list)} genomes for evaluation")
        
        # Also export full genome data for Unreal to load
        for genome_id, genome in genomes:
            genome_file = self.output_dir / f"genome_{genome_id}.json"
            GenomeExporter.export_genome(
                genome, genome_id, self.generation, 0.0, genome_file, config
            )
    
    def export_best_genome(self, config):
        """Export best genome to 'best_genome.json'."""
        best_file = self.output_dir / "best_genome.json"
        GenomeExporter.export_genome(
            self.best_genome,
            self.best_genome.key,
            self.generation,
            self.best_fitness,
            best_file,
            config
        )
        print(f"🌟 Best genome exported (Fitness: {self.best_fitness:.2f})")
    
    def train(self, num_generations: int):
        """Run NEAT training for specified number of generations."""
        print(f"\n🚀 Starting NEAT Training")
        print(f"   Generations: {num_generations}")
        print(f"   Population: {self.config.pop_size}")
        print(f"   Inputs: {OBS_SIZE}, Outputs: {ACTION_SIZE}")
        print("=" * 60)
        
        winner = self.population.run(self.evaluate_generation, num_generations)
        
        print("\n✅ Training Complete!")
        print(f"   Winner Genome ID: {winner.key}")
        print(f"   Winner Fitness: {winner.fitness:.2f}")
        
        # Save final checkpoint
        checkpoint_file = self.output_dir / f"neat_checkpoint_gen_{self.generation}.pkl"
        with open(checkpoint_file, 'wb') as f:
            pickle.dump((self.population, self.config, self.generation), f)
        
        print(f"💾 Checkpoint saved: {checkpoint_file}")
        
        return winner

# ============================================================================
# Main
# ============================================================================

def create_default_config(config_path: str):
    """Create default NEAT config file if it doesn't exist."""
    if not Path(config_path).exists():
        print(f"📝 Creating default config: {config_path}")
        with open(config_path, 'w') as f:
            f.write(NEAT_CONFIG_TEMPLATE)

def main():
    # Parse arguments
    fitness_dir = sys.argv[1] if len(sys.argv) > 1 else "Saved/Training/Fitness"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "Saved/Training/NEAT"
    num_generations = int(sys.argv[3]) if len(sys.argv) > 3 else 50
    
    # Create default config if needed
    create_default_config(CONFIG_FILE)
    
    # Initialize trainer
    trainer = NEATTrainer(CONFIG_FILE, fitness_dir, output_dir)
    
    # Train
    trainer.train(num_generations)

if __name__ == "__main__":
    main()