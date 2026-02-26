#!/usr/bin/env python3
"""
NEAT Racing AI - Analysis CLI Tool

Comprehensive analysis tool for NEAT training progress:
- Fitness progress over generations
- Best/Average/Worst genome tracking
- Trajectory visualization (2D/3D)
- Network topology visualization
- Ray trace analysis
- Training statistics

Usage:
    python neat_analysis_cli.py --mode fitness
    python neat_analysis_cli.py --mode trajectories --generation 25
    python neat_analysis_cli.py --mode network --genome best
    python neat_analysis_cli.py --mode compare --generations 0,10,25,50
"""

import os
import json
import argparse
from pathlib import Path
from typing import List, Dict, Tuple, Optional
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import pandas as pd

try:
    import seaborn as sns
    HAS_SEABORN = True
except ImportError:
    HAS_SEABORN = False

try:
    import networkx as nx
    HAS_NETWORKX = True
except ImportError:
    HAS_NETWORKX = False

try:
    import plotly.graph_objects as go
    import plotly.express as px
    HAS_PLOTLY = True
except ImportError:
    HAS_PLOTLY = False


class NEATAnalyzer:
    """Main analyzer class for NEAT training data"""
    
    def __init__(self, base_dir: str = "Saved/Training"):
        self.base_dir = Path(base_dir)
        self.fitness_dir = self.base_dir / "Fitness"
        self.neat_dir = self.base_dir / "NEAT"
        self.trajectory_dir = self.base_dir / "Trajectories"
        
        # Set style
        if HAS_SEABORN:
            sns.set_theme(style="darkgrid")
        else:
            plt.style.use('seaborn-v0_8-darkgrid' if 'seaborn-v0_8-darkgrid' in plt.style.available else 'default')
    
    def load_fitness_data(self, generation: int) -> Optional[Dict]:
        """Load fitness data for a specific generation"""
        fitness_file = self.fitness_dir / f"generation_{generation}.json"
        if not fitness_file.exists():
            print(f"Warning: Fitness file not found: {fitness_file}")
            return None
        
        with open(fitness_file, 'r') as f:
            return json.load(f)
    
    def load_all_fitness(self) -> pd.DataFrame:
        """Load all fitness data into DataFrame"""
        data = []
        
        for fitness_file in sorted(self.fitness_dir.glob("generation_*.json")):
            with open(fitness_file, 'r') as f:
                gen_data = json.load(f)
                generation = gen_data['generation']
                
                for genome in gen_data['genomes']:
                    data.append({
                        'generation': generation,
                        'genome_id': genome['genome_id'],
                        'fitness': genome['fitness']
                    })
        
        return pd.DataFrame(data)
    
    def load_genome(self, genome_id: int = None, generation: int = None) -> Optional[Dict]:
        """Load genome data"""
        if genome_id is None:
            # Load best genome
            genome_file = self.neat_dir / "best_genome.json"
        else:
            genome_file = self.neat_dir / f"genome_{genome_id}.json"
        
        if not genome_file.exists():
            print(f"Warning: Genome file not found: {genome_file}")
            return None
        
        with open(genome_file, 'r') as f:
            return json.load(f)
    
    def load_trajectory(self, generation: int, genome_id: int) -> Optional[Dict]:
        """Load trajectory data"""
        traj_file = self.trajectory_dir / f"gen_{generation}_genome_{genome_id}.json"
        if not traj_file.exists():
            print(f"Warning: Trajectory file not found: {traj_file}")
            return None
        
        with open(traj_file, 'r') as f:
            return json.load(f)
    
    # ========================================================================
    # FITNESS ANALYSIS
    # ========================================================================
    
    def plot_fitness_progress(self, save_path: Optional[str] = None):
        """Plot fitness progress over generations"""
        df = self.load_all_fitness()
        
        if df.empty:
            print("No fitness data found!")
            return
        
        # Calculate statistics per generation
        stats = df.groupby('generation')['fitness'].agg(['max', 'mean', 'min', 'std']).reset_index()
        
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
        
        # Plot 1: Max/Mean/Min Fitness
        ax1.plot(stats['generation'], stats['max'], 'g-', linewidth=2, label='Best', marker='o')
        ax1.plot(stats['generation'], stats['mean'], 'b-', linewidth=2, label='Average', marker='s')
        ax1.plot(stats['generation'], stats['min'], 'r-', linewidth=1, label='Worst', alpha=0.6)
        ax1.fill_between(stats['generation'], 
                         stats['mean'] - stats['std'], 
                         stats['mean'] + stats['std'], 
                         alpha=0.3, color='blue', label='Std Dev')
        
        ax1.set_xlabel('Generation', fontsize=12)
        ax1.set_ylabel('Fitness', fontsize=12)
        ax1.set_title('NEAT Training Progress - Fitness over Generations', fontsize=14, fontweight='bold')
        ax1.legend(loc='lower right')
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Fitness Distribution per Generation (Box Plot)
        generations_to_plot = stats['generation'].values[::max(1, len(stats)//20)]  # Max 20 boxes
        box_data = [df[df['generation'] == g]['fitness'].values for g in generations_to_plot]
        
        bp = ax2.boxplot(box_data, positions=generations_to_plot, widths=0.6, 
                         patch_artist=True, showfliers=True)
        
        for patch in bp['boxes']:
            patch.set_facecolor('lightblue')
            patch.set_alpha(0.7)
        
        ax2.set_xlabel('Generation', fontsize=12)
        ax2.set_ylabel('Fitness Distribution', fontsize=12)
        ax2.set_title('Fitness Distribution per Generation', fontsize=14, fontweight='bold')
        ax2.grid(True, alpha=0.3, axis='y')
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Saved fitness plot to: {save_path}")
        else:
            plt.show()
    
    def print_fitness_summary(self):
        """Print fitness summary statistics"""
        df = self.load_all_fitness()
        
        if df.empty:
            print("No fitness data found!")
            return
        
        stats = df.groupby('generation')['fitness'].agg(['max', 'mean', 'min', 'std'])
        
        print("\n" + "="*80)
        print("NEAT TRAINING SUMMARY")
        print("="*80)
        
        print(f"\nTotal Generations: {stats.index.max() + 1}")
        print(f"Population Size: {len(df[df['generation'] == 0])}")
        
        print(f"\n{'Generation':<12} {'Best':<12} {'Average':<12} {'Worst':<12} {'Std Dev':<12}")
        print("-"*80)
        
        for gen, row in stats.iterrows():
            print(f"{gen:<12} {row['max']:<12.2f} {row['mean']:<12.2f} {row['min']:<12.2f} {row['std']:<12.2f}")
        
        # Overall Statistics
        print("\n" + "-"*80)
        print(f"Overall Best Fitness: {stats['max'].max():.2f} (Generation {stats['max'].idxmax()})")
        print(f"Final Generation Best: {stats.iloc[-1]['max']:.2f}")
        print(f"Improvement: {((stats.iloc[-1]['max'] / stats.iloc[0]['max']) - 1) * 100:.1f}%")
        print("="*80 + "\n")
    
    # ========================================================================
    # TRAJECTORY VISUALIZATION
    # ========================================================================
    
    def plot_trajectories_2d(self, generation: int, genome_ids: List[int] = None, 
                             save_path: Optional[str] = None):
        """Plot 2D top-down trajectories"""
        
        if genome_ids is None:
            # Load all trajectories for this generation
            traj_files = list(self.trajectory_dir.glob(f"gen_{generation}_genome_*.json"))
            genome_ids = [int(f.stem.split('_')[-1]) for f in traj_files]
        
        if not genome_ids:
            print(f"No trajectories found for generation {generation}")
            return
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))
        
        # Color map
        colors = plt.cm.rainbow(np.linspace(0, 1, len(genome_ids)))
        
        for i, genome_id in enumerate(genome_ids[:20]):  # Max 20 trajectories
            traj = self.load_trajectory(generation, genome_id)
            if not traj:
                continue
            
            positions = np.array(traj['positions'])
            fitness = traj.get('fitness', 0)
            
            # Plot 1: Full trajectories
            ax1.plot(positions[:, 0], positions[:, 1], 
                    color=colors[i], alpha=0.6, linewidth=1.5,
                    label=f"Genome {genome_id} (F={fitness:.0f})")
            
            # Mark start and end
            ax1.plot(positions[0, 0], positions[0, 1], 'go', markersize=8)  # Start
            ax1.plot(positions[-1, 0], positions[-1, 1], 'r^', markersize=8)  # End
        
        ax1.set_xlabel('X Position (cm)', fontsize=12)
        ax1.set_ylabel('Y Position (cm)', fontsize=12)
        ax1.set_title(f'Generation {generation} - Trajectories (Top 20)', 
                     fontsize=14, fontweight='bold')
        ax1.legend(loc='best', fontsize=8, ncol=2)
        ax1.grid(True, alpha=0.3)
        ax1.axis('equal')
        
        # Plot 2: Heatmap of visited positions
        all_positions = []
        for genome_id in genome_ids:
            traj = self.load_trajectory(generation, genome_id)
            if traj:
                all_positions.extend(traj['positions'])
        
        if all_positions:
            all_positions = np.array(all_positions)
            ax2.hexbin(all_positions[:, 0], all_positions[:, 1], 
                      gridsize=50, cmap='YlOrRd', alpha=0.8)
            ax2.set_xlabel('X Position (cm)', fontsize=12)
            ax2.set_ylabel('Y Position (cm)', fontsize=12)
            ax2.set_title('Heatmap of Explored Area', fontsize=14, fontweight='bold')
            ax2.axis('equal')
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Saved trajectory plot to: {save_path}")
        else:
            plt.show()
    
    def plot_trajectories_3d(self, generation: int, genome_ids: List[int] = None):
        """Plot 3D trajectories (interactive with Plotly if available)"""
        
        if not HAS_PLOTLY:
            print("Plotly not installed. Install with: pip install plotly")
            print("Falling back to matplotlib 3D...")
            self._plot_trajectories_3d_matplotlib(generation, genome_ids)
            return
        
        if genome_ids is None:
            traj_files = list(self.trajectory_dir.glob(f"gen_{generation}_genome_*.json"))
            genome_ids = [int(f.stem.split('_')[-1]) for f in traj_files][:10]  # Max 10
        
        fig = go.Figure()
        
        for genome_id in genome_ids:
            traj = self.load_trajectory(generation, genome_id)
            if not traj:
                continue
            
            positions = np.array(traj['positions'])
            fitness = traj.get('fitness', 0)
            
            # Add trajectory line
            fig.add_trace(go.Scatter3d(
                x=positions[:, 0],
                y=positions[:, 1],
                z=positions[:, 2],
                mode='lines',
                name=f'Genome {genome_id} (F={fitness:.0f})',
                line=dict(width=3)
            ))
            
            # Mark start
            fig.add_trace(go.Scatter3d(
                x=[positions[0, 0]],
                y=[positions[0, 1]],
                z=[positions[0, 2]],
                mode='markers',
                marker=dict(size=8, color='green', symbol='circle'),
                showlegend=False
            ))
        
        fig.update_layout(
            title=f'Generation {generation} - 3D Trajectories',
            scene=dict(
                xaxis_title='X Position (cm)',
                yaxis_title='Y Position (cm)',
                zaxis_title='Z Position (cm)',
                aspectmode='data'
            ),
            width=1200,
            height=800
        )
        
        fig.show()
    
    def _plot_trajectories_3d_matplotlib(self, generation: int, genome_ids: List[int] = None):
        """Matplotlib 3D trajectory plot (fallback)"""
        from mpl_toolkits.mplot3d import Axes3D
        
        if genome_ids is None:
            traj_files = list(self.trajectory_dir.glob(f"gen_{generation}_genome_*.json"))
            genome_ids = [int(f.stem.split('_')[-1]) for f in traj_files][:10]
        
        fig = plt.figure(figsize=(14, 10))
        ax = fig.add_subplot(111, projection='3d')
        
        colors = plt.cm.rainbow(np.linspace(0, 1, len(genome_ids)))
        
        for i, genome_id in enumerate(genome_ids):
            traj = self.load_trajectory(generation, genome_id)
            if not traj:
                continue
            
            positions = np.array(traj['positions'])
            fitness = traj.get('fitness', 0)
            
            ax.plot(positions[:, 0], positions[:, 1], positions[:, 2],
                   color=colors[i], alpha=0.7, linewidth=2,
                   label=f"Genome {genome_id} (F={fitness:.0f})")
            
            # Mark start
            ax.scatter(positions[0, 0], positions[0, 1], positions[0, 2],
                      color='green', s=100, marker='o')
        
        ax.set_xlabel('X Position (cm)')
        ax.set_ylabel('Y Position (cm)')
        ax.set_zlabel('Z Position (cm)')
        ax.set_title(f'Generation {generation} - 3D Trajectories')
        ax.legend(loc='best', fontsize=8)
        
        plt.show()
    
    def plot_trajectory_metrics(self, generation: int, genome_id: int, 
                               save_path: Optional[str] = None):
        """Plot detailed trajectory metrics over time"""
        
        traj = self.load_trajectory(generation, genome_id)
        if not traj:
            return
        
        fig, axes = plt.subplots(3, 2, figsize=(16, 12))
        
        # Extract data
        time = np.arange(len(traj['positions'])) * traj.get('dt', 0.1)
        positions = np.array(traj['positions'])
        velocities = np.array(traj.get('velocities', np.zeros_like(positions)))
        
        # Speed
        speed = np.linalg.norm(velocities, axis=1)
        
        # Plot 1: Speed over time
        axes[0, 0].plot(time, speed, 'b-', linewidth=1.5)
        axes[0, 0].set_xlabel('Time (s)')
        axes[0, 0].set_ylabel('Speed (cm/s)')
        axes[0, 0].set_title('Speed over Time')
        axes[0, 0].grid(True, alpha=0.3)
        
        # Plot 2: Distance traveled
        distance = np.cumsum(np.linalg.norm(np.diff(positions, axis=0), axis=1))
        distance = np.concatenate([[0], distance])
        axes[0, 1].plot(time, distance / 100, 'g-', linewidth=1.5)  # Convert to meters
        axes[0, 1].set_xlabel('Time (s)')
        axes[0, 1].set_ylabel('Distance (m)')
        axes[0, 1].set_title('Distance Traveled')
        axes[0, 1].grid(True, alpha=0.3)
        
        # Plot 3: Ray distances (if available)
        if 'ray_distances' in traj:
            rays = np.array(traj['ray_distances'])
            ray_names = ['Forward', 'Left', 'Right', 'Left45', 'Right45']
            for i, name in enumerate(ray_names[:rays.shape[1]]):
                axes[1, 0].plot(time, rays[:, i], label=name, alpha=0.7)
            axes[1, 0].set_xlabel('Time (s)')
            axes[1, 0].set_ylabel('Ray Distance (normalized)')
            axes[1, 0].set_title('Ray Distances over Time')
            axes[1, 0].legend(loc='best')
            axes[1, 0].grid(True, alpha=0.3)
        
        # Plot 4: Actions (if available)
        if 'actions' in traj:
            actions = np.array(traj['actions'])
            axes[1, 1].plot(time, actions[:, 0], 'r-', label='Steer', alpha=0.7)
            axes[1, 1].plot(time, actions[:, 1], 'g-', label='Throttle', alpha=0.7)
            axes[1, 1].plot(time, actions[:, 2], 'b-', label='Brake', alpha=0.7)
            axes[1, 1].set_xlabel('Time (s)')
            axes[1, 1].set_ylabel('Action Value')
            axes[1, 1].set_title('Control Actions over Time')
            axes[1, 1].legend(loc='best')
            axes[1, 1].grid(True, alpha=0.3)
            axes[1, 1].set_ylim([-1.1, 1.1])
        
        # Plot 5: X/Y Position
        axes[2, 0].plot(positions[:, 0], positions[:, 1], 'b-', linewidth=1.5, alpha=0.7)
        axes[2, 0].scatter(positions[0, 0], positions[0, 1], c='green', s=100, marker='o', label='Start')
        axes[2, 0].scatter(positions[-1, 0], positions[-1, 1], c='red', s=100, marker='^', label='End')
        axes[2, 0].set_xlabel('X Position (cm)')
        axes[2, 0].set_ylabel('Y Position (cm)')
        axes[2, 0].set_title('2D Trajectory')
        axes[2, 0].legend()
        axes[2, 0].grid(True, alpha=0.3)
        axes[2, 0].axis('equal')
        
        # Plot 6: Altitude (Z) over time
        axes[2, 1].plot(time, positions[:, 2], 'purple', linewidth=1.5)
        axes[2, 1].set_xlabel('Time (s)')
        axes[2, 1].set_ylabel('Z Position (cm)')
        axes[2, 1].set_title('Altitude over Time')
        axes[2, 1].grid(True, alpha=0.3)
        
        fig.suptitle(f'Trajectory Analysis - Gen {generation}, Genome {genome_id}, Fitness: {traj.get("fitness", 0):.1f}',
                    fontsize=16, fontweight='bold')
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Saved trajectory metrics to: {save_path}")
        else:
            plt.show()
    
    # ========================================================================
    # NETWORK VISUALIZATION
    # ========================================================================
    
    def plot_network_topology(self, genome_id: int = None, save_path: Optional[str] = None):
        """Visualize NEAT network topology"""
        
        if not HAS_NETWORKX:
            print("NetworkX not installed. Install with: pip install networkx")
            return
        
        genome = self.load_genome(genome_id)
        if not genome:
            return
        
        # Build graph
        G = nx.DiGraph()
        
        # Add nodes
        num_inputs = 16  # From observation size
        num_outputs = 3  # Steer, Throttle, Brake
        
        input_nodes = list(range(num_inputs))
        output_nodes = list(range(num_inputs, num_inputs + num_outputs))
        hidden_nodes = [n for n in genome['nodes'] if n not in input_nodes + output_nodes]
        
        # Add nodes with attributes
        for node in input_nodes:
            G.add_node(node, layer='input')
        for node in output_nodes:
            G.add_node(node, layer='output')
        for node in hidden_nodes:
            G.add_node(node, layer='hidden')
        
        # Add connections
        for conn_str in genome['connections']:
            parts = conn_str.split(',')
            in_node, out_node, weight, enabled = int(parts[0]), int(parts[1]), float(parts[2]), int(parts[3])
            if enabled:
                G.add_edge(in_node, out_node, weight=weight)
        
        # Layout
        pos = {}
        
        # Input layer
        for i, node in enumerate(input_nodes):
            pos[node] = (0, i * 2)
        
        # Output layer
        for i, node in enumerate(output_nodes):
            pos[node] = (4, i * 5 + 5)
        
        # Hidden layer (spring layout for hidden nodes)
        if hidden_nodes:
            hidden_pos = nx.spring_layout(G.subgraph(hidden_nodes), k=1)
            for node in hidden_nodes:
                pos[node] = (2, hidden_pos[node][1] * 20)
        
        # Plot
        fig, ax = plt.subplots(figsize=(16, 12))
        
        # Draw nodes
        nx.draw_networkx_nodes(G, pos, nodelist=input_nodes, 
                              node_color='lightblue', node_size=500, 
                              label='Input', ax=ax)
        nx.draw_networkx_nodes(G, pos, nodelist=output_nodes, 
                              node_color='lightcoral', node_size=500, 
                              label='Output', ax=ax)
        if hidden_nodes:
            nx.draw_networkx_nodes(G, pos, nodelist=hidden_nodes, 
                                  node_color='lightgreen', node_size=500, 
                                  label='Hidden', ax=ax)
        
        # Draw edges (color by weight)
        edges = G.edges()
        weights = [G[u][v]['weight'] for u, v in edges]
        max_weight = max(abs(w) for w in weights) if weights else 1
        
        for u, v in edges:
            weight = G[u][v]['weight']
            color = 'red' if weight < 0 else 'blue'
            alpha = min(abs(weight) / max_weight, 1.0)
            width = max(abs(weight) / max_weight * 3, 0.5)
            ax.plot([pos[u][0], pos[v][0]], [pos[u][1], pos[v][1]], 
                   color=color, alpha=alpha, linewidth=width)
        
        # Draw labels
        nx.draw_networkx_labels(G, pos, font_size=8, ax=ax)
        
        ax.set_title(f'NEAT Network Topology - Genome {genome_id if genome_id else "Best"}\n'
                    f'Nodes: {len(G.nodes())}, Connections: {len(G.edges())}',
                    fontsize=14, fontweight='bold')
        ax.legend(loc='upper right')
        ax.axis('off')
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Saved network topology to: {save_path}")
        else:
            plt.show()
    
    # ========================================================================
    # COMPARISON
    # ========================================================================
    
    def compare_generations(self, generations: List[int], save_path: Optional[str] = None):
        """Compare multiple generations side-by-side"""
        
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        
        colors = plt.cm.rainbow(np.linspace(0, 1, len(generations)))
        
        # Collect data
        gen_data = {}
        for gen in generations:
            fitness_data = self.load_fitness_data(gen)
            if fitness_data:
                fitnesses = [g['fitness'] for g in fitness_data['genomes']]
                gen_data[gen] = fitnesses
        
        # Plot 1: Fitness distribution (violin plot)
        violin_data = [gen_data[g] for g in generations if g in gen_data]
        parts = axes[0, 0].violinplot(violin_data, positions=range(len(violin_data)), 
                                      widths=0.7, showmeans=True, showextrema=True)
        axes[0, 0].set_xticks(range(len(generations)))
        axes[0, 0].set_xticklabels([f"Gen {g}" for g in generations])
        axes[0, 0].set_ylabel('Fitness')
        axes[0, 0].set_title('Fitness Distribution Comparison')
        axes[0, 0].grid(True, alpha=0.3, axis='y')
        
        # Plot 2: Best fitness progression
        best_fitness = [max(gen_data[g]) for g in generations if g in gen_data]
        axes[0, 1].plot(generations, best_fitness, 'go-', linewidth=2, markersize=8)
        axes[0, 1].set_xlabel('Generation')
        axes[0, 1].set_ylabel('Best Fitness')
        axes[0, 1].set_title('Best Fitness Progression')
        axes[0, 1].grid(True, alpha=0.3)
        
        # Plot 3: Trajectory comparison (2D)
        for i, gen in enumerate(generations):
            # Load best genome trajectory
            fitness_data = self.load_fitness_data(gen)
            if not fitness_data:
                continue
            best_genome_id = max(fitness_data['genomes'], key=lambda x: x['fitness'])['genome_id']
            traj = self.load_trajectory(gen, best_genome_id)
            if traj:
                positions = np.array(traj['positions'])
                axes[1, 0].plot(positions[:, 0], positions[:, 1], 
                              color=colors[i], alpha=0.7, linewidth=2,
                              label=f"Gen {gen}")
        
        axes[1, 0].set_xlabel('X Position (cm)')
        axes[1, 0].set_ylabel('Y Position (cm)')
        axes[1, 0].set_title('Best Trajectories Comparison')
        axes[1, 0].legend(loc='best')
        axes[1, 0].grid(True, alpha=0.3)
        axes[1, 0].axis('equal')
        
        # Plot 4: Statistics table
        stats_text = "Generation Statistics:\n\n"
        stats_text += f"{'Gen':<6} {'Best':<10} {'Mean':<10} {'Std':<10}\n"
        stats_text += "-" * 40 + "\n"
        
        for gen in generations:
            if gen in gen_data:
                fitnesses = gen_data[gen]
                stats_text += f"{gen:<6} {max(fitnesses):<10.1f} {np.mean(fitnesses):<10.1f} {np.std(fitnesses):<10.1f}\n"
        
        axes[1, 1].text(0.1, 0.5, stats_text, fontsize=10, family='monospace',
                       verticalalignment='center')
        axes[1, 1].axis('off')
        
        fig.suptitle('Generation Comparison Analysis', fontsize=16, fontweight='bold')
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Saved comparison plot to: {save_path}")
        else:
            plt.show()


# ============================================================================
# CLI Interface
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='NEAT Racing AI - Analysis Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Fitness progress
  python neat_analysis_cli.py --mode fitness
  
  # Trajectory visualization
  python neat_analysis_cli.py --mode trajectories --generation 25
  
  # Network topology
  python neat_analysis_cli.py --mode network --genome best
  
  # Compare generations
  python neat_analysis_cli.py --mode compare --generations 0,10,25,50
  
  # Detailed trajectory analysis
  python neat_analysis_cli.py --mode trajectory-detail --generation 25 --genome 42
        """
    )
    
    parser.add_argument('--mode', type=str, required=True,
                       choices=['fitness', 'trajectories', 'network', 'compare', 'trajectory-detail', 'summary'],
                       help='Analysis mode')
    
    parser.add_argument('--base-dir', type=str, default='Saved/Training',
                       help='Base directory for training data')
    
    parser.add_argument('--generation', type=int, help='Generation number')
    parser.add_argument('--genome', type=str, help='Genome ID or "best"')
    parser.add_argument('--generations', type=str, help='Comma-separated list of generations')
    parser.add_argument('--output', type=str, help='Output file path for saving plots')
    
    args = parser.parse_args()
    
    analyzer = NEATAnalyzer(args.base_dir)
    
    # Execute analysis
    if args.mode == 'fitness':
        print("Analyzing fitness progress...")
        analyzer.plot_fitness_progress(save_path=args.output)
    
    elif args.mode == 'trajectories':
        if args.generation is None:
            print("Error: --generation required for trajectories mode")
            return
        print(f"Visualizing trajectories for generation {args.generation}...")
        analyzer.plot_trajectories_2d(args.generation, save_path=args.output)
        analyzer.plot_trajectories_3d(args.generation)
    
    elif args.mode == 'network':
        genome_id = None if args.genome == 'best' else int(args.genome)
        print(f"Visualizing network topology for genome {args.genome}...")
        analyzer.plot_network_topology(genome_id, save_path=args.output)
    
    elif args.mode == 'compare':
        if args.generations is None:
            print("Error: --generations required for compare mode")
            return
        generations = [int(g.strip()) for g in args.generations.split(',')]
        print(f"Comparing generations: {generations}...")
        analyzer.compare_generations(generations, save_path=args.output)
    
    elif args.mode == 'trajectory-detail':
        if args.generation is None or args.genome is None:
            print("Error: --generation and --genome required for trajectory-detail mode")
            return
        genome_id = int(args.genome)
        print(f"Analyzing trajectory details for Gen {args.generation}, Genome {genome_id}...")
        analyzer.plot_trajectory_metrics(args.generation, genome_id, save_path=args.output)
    
    elif args.mode == 'summary':
        analyzer.print_fitness_summary()
    
    print("\nAnalysis complete!")


if __name__ == '__main__':
    main()