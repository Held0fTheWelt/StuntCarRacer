#!/usr/bin/env python3
"""
CarAI Training Analysis CLI Tool
Analyzes rollout data, creates visualizations, and identifies problems
"""

import os
import sys
import json
import glob
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime
from pathlib import Path

# ============================================================================
# Configuration
# ============================================================================

class Config:
    """Global configuration"""
    # Thresholds for problem detection
    STEERING_MEAN_THRESHOLD = 0.7  # |Mean| > 0.7 is problematic
    STEERING_STD_THRESHOLD = 0.1   # Std < 0.1 means no exploration
    THROTTLE_STD_THRESHOLD = 0.05  # Std < 0.05 means no exploration
    REWARD_NEGATIVE_THRESHOLD = -2.0  # Average reward < -2 is bad
    SUCCESS_RATE_THRESHOLD = 0.3  # Success rate < 30% is problematic
    
    # Colors
    COLOR_OK = '\033[92m'      # Green
    COLOR_WARNING = '\033[93m'  # Yellow
    COLOR_ERROR = '\033[91m'    # Red
    COLOR_INFO = '\033[94m'     # Blue
    COLOR_RESET = '\033[0m'
    
    # Default paths (relative to script)
    SCRIPT_DIR = Path(__file__).parent
    DEFAULT_EXPORT_DIR = SCRIPT_DIR.parent.parent.parent.parent.parent / "Saved" / "Training" / "Exports"
    DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "analysis_output"

# ============================================================================
# Data Loading
# ============================================================================

def load_rollout_files(export_dir):
    """Load all rollout JSON files from directory"""
    pattern = os.path.join(export_dir, "rollout_*.json")
    files = sorted(glob.glob(pattern))
    
    if not files:
        print(f"{Config.COLOR_ERROR}❌ No rollout files found in {export_dir}{Config.COLOR_RESET}")
        return []
    
    print(f"{Config.COLOR_INFO}📁 Found {len(files)} rollout files{Config.COLOR_RESET}")
    
    rollouts = []
    for filepath in files:
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
                # Extract rollout number from filename
                filename = os.path.basename(filepath)
                rollout_num = int(filename.replace('rollout_', '').replace('.json', ''))
                data['rollout_number'] = rollout_num
                data['filepath'] = filepath
                rollouts.append(data)
        except Exception as e:
            print(f"{Config.COLOR_WARNING}⚠️  Failed to load {filepath}: {e}{Config.COLOR_RESET}")
    
    return rollouts

# ============================================================================
# Analysis Functions
# ============================================================================

def analyze_steering_distribution(rollouts):
    """Analyze steering behavior across all rollouts"""
    all_steers = []
    for rollout in rollouts:
        for exp in rollout.get('experiences', []):
            action = exp.get('action', {})
            steer = action.get('steer', 0.0)
            all_steers.append(steer)
    
    if not all_steers:
        return None
    
    steers = np.array(all_steers)
    return {
        'mean': np.mean(steers),
        'std': np.std(steers),
        'min': np.min(steers),
        'max': np.max(steers),
        'median': np.median(steers),
        'count': len(steers),
        'data': steers
    }

def analyze_action_diversity(rollouts):
    """Analyze action diversity (throttle, brake, steering)"""
    all_steers = []
    all_throttles = []
    all_brakes = []
    
    for rollout in rollouts:
        for exp in rollout.get('experiences', []):
            action = exp.get('action', {})
            all_steers.append(action.get('steer', 0.0))
            all_throttles.append(action.get('throttle', 0.0))
            all_brakes.append(action.get('brake', 0.0))
    
    return {
        'steering': {
            'mean': np.mean(all_steers),
            'std': np.std(all_steers)
        },
        'throttle': {
            'mean': np.mean(all_throttles),
            'std': np.std(all_throttles)
        },
        'brake': {
            'mean': np.mean(all_brakes),
            'std': np.std(all_brakes)
        }
    }

def analyze_episode_performance(rollouts):
    """Analyze episode rewards and success rates"""
    episode_rewards = []
    episode_lengths = []
    success_count = 0
    
    for rollout in rollouts:
        total_reward = rollout.get('total_reward', 0.0)
        num_steps = len(rollout.get('experiences', []))
        
        episode_rewards.append(total_reward)
        episode_lengths.append(num_steps)
        
        # Consider successful if reward > 0 and length > 100
        if total_reward > 0 and num_steps > 100:
            success_count += 1
    
    success_rate = success_count / len(rollouts) if rollouts else 0.0
    
    return {
        'mean_reward': np.mean(episode_rewards) if episode_rewards else 0.0,
        'std_reward': np.std(episode_rewards) if episode_rewards else 0.0,
        'min_reward': np.min(episode_rewards) if episode_rewards else 0.0,
        'max_reward': np.max(episode_rewards) if episode_rewards else 0.0,
        'mean_length': np.mean(episode_lengths) if episode_lengths else 0.0,
        'success_rate': success_rate,
        'total_episodes': len(rollouts),
        'rewards': episode_rewards,
        'lengths': episode_lengths
    }

def analyze_throttle_brake(rollouts):
    """Detailed throttle and brake analysis"""
    throttles = []
    brakes = []
    combined = []  # throttle - brake (net acceleration)
    
    for rollout in rollouts:
        for exp in rollout.get('experiences', []):
            action = exp.get('action', {})
            thr = action.get('throttle', 0.0)
            brk = action.get('brake', 0.0)
            throttles.append(thr)
            brakes.append(brk)
            combined.append(thr - brk)
    
    if not throttles:
        return None
    
    throttles = np.array(throttles)
    brakes = np.array(brakes)
    combined = np.array(combined)
    
    # Detect patterns
    both_high = np.sum((throttles > 0.8) & (brakes > 0.8))
    both_low = np.sum((throttles < 0.2) & (brakes < 0.2))
    
    return {
        'throttle': {
            'mean': np.mean(throttles),
            'std': np.std(throttles),
            'min': np.min(throttles),
            'max': np.max(throttles),
            'data': throttles
        },
        'brake': {
            'mean': np.mean(brakes),
            'std': np.std(brakes),
            'min': np.min(brakes),
            'max': np.max(brakes),
            'data': brakes
        },
        'combined': {
            'mean': np.mean(combined),
            'std': np.std(combined),
            'data': combined
        },
        'both_high_count': both_high,
        'both_high_pct': both_high / len(throttles) * 100,
        'both_low_count': both_low,
        'both_low_pct': both_low / len(throttles) * 100
    }

def analyze_reward_components(rollouts):
    """Analyze reward components if available in JSON"""
    # Try to extract reward breakdown if present
    component_data = {
        'progress': [],
        'speed': [],
        'lateral': [],
        'heading': [],
        'angular': [],
        'smoothness': [],
        'airborne': [],
        'steering': []
    }
    
    has_components = False
    
    for rollout in rollouts:
        for exp in rollout.get('experiences', []):
            # Check if reward breakdown exists
            reward_breakdown = exp.get('reward_breakdown', {})
            if reward_breakdown:
                has_components = True
                component_data['progress'].append(reward_breakdown.get('progress', 0.0))
                component_data['speed'].append(reward_breakdown.get('speed', 0.0))
                component_data['lateral'].append(reward_breakdown.get('lateral', 0.0))
                component_data['heading'].append(reward_breakdown.get('heading', 0.0))
                component_data['angular'].append(reward_breakdown.get('angular_velocity', 0.0))
                component_data['smoothness'].append(reward_breakdown.get('smoothness', 0.0))
                component_data['airborne'].append(reward_breakdown.get('airborne', 0.0))
                component_data['steering'].append(reward_breakdown.get('steering', 0.0))
    
    if not has_components:
        return None
    
    # Calculate statistics for each component
    result = {}
    for key, values in component_data.items():
        if values:
            result[key] = {
                'mean': np.mean(values),
                'std': np.std(values),
                'sum': np.sum(values),
                'data': np.array(values)
            }
    
    return result

def detect_problems(steering_stats, action_stats, episode_stats):
    """Detect problems in training data"""
    problems = []
    warnings = []
    
    # Check steering bias
    if steering_stats:
        mean = steering_stats['mean']
        std = steering_stats['std']
        
        if abs(mean) > Config.STEERING_MEAN_THRESHOLD:
            problems.append(f"CRITICAL: Steering heavily biased to {'left' if mean < 0 else 'right'} (Mean: {mean:.3f})")
        
        if std < Config.STEERING_STD_THRESHOLD:
            problems.append(f"CRITICAL: No steering exploration (Std: {std:.3f})")
    
    # Check action diversity
    if action_stats:
        if action_stats['throttle']['std'] < Config.THROTTLE_STD_THRESHOLD:
            warnings.append(f"WARNING: Low throttle variation (Std: {action_stats['throttle']['std']:.3f})")
    
    # Check episode performance
    if episode_stats:
        if episode_stats['mean_reward'] < Config.REWARD_NEGATIVE_THRESHOLD:
            problems.append(f"CRITICAL: Negative average reward ({episode_stats['mean_reward']:.2f})")
        
        if episode_stats['success_rate'] < Config.SUCCESS_RATE_THRESHOLD:
            warnings.append(f"WARNING: Low success rate ({episode_stats['success_rate']*100:.1f}%)")
    
    return problems, warnings

# ============================================================================
# Visualization Functions
# ============================================================================

def create_steering_distribution_plot(steering_stats, output_dir):
    """Create steering distribution visualization"""
    if not steering_stats:
        return None
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Steering Analysis', fontsize=16, fontweight='bold')
    
    steers = steering_stats['data']
    
    # Histogram
    ax = axes[0, 0]
    ax.hist(steers, bins=50, edgecolor='black', alpha=0.7)
    ax.axvline(x=0, color='red', linestyle='--', label='Center')
    ax.axvline(x=steering_stats['mean'], color='green', linestyle='--', label=f"Mean: {steering_stats['mean']:.3f}")
    ax.set_xlabel('Steering Value')
    ax.set_ylabel('Frequency')
    ax.set_title('Steering Distribution')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Box plot
    ax = axes[0, 1]
    ax.boxplot(steers, vert=True)
    ax.axhline(y=0, color='red', linestyle='--', alpha=0.5)
    ax.set_ylabel('Steering Value')
    ax.set_title('Steering Box Plot')
    ax.grid(True, alpha=0.3)
    
    # Statistics text
    ax = axes[1, 0]
    ax.axis('off')
    stats_text = f"""
    STEERING STATISTICS
    ═══════════════════
    Mean:      {steering_stats['mean']:>8.3f}
    Std Dev:   {steering_stats['std']:>8.3f}
    Median:    {steering_stats['median']:>8.3f}
    Min:       {steering_stats['min']:>8.3f}
    Max:       {steering_stats['max']:>8.3f}
    Samples:   {steering_stats['count']:>8d}
    """
    ax.text(0.1, 0.5, stats_text, fontsize=12, family='monospace',
            verticalalignment='center')
    
    # Time series (sample)
    ax = axes[1, 1]
    sample_size = min(500, len(steers))
    ax.plot(steers[:sample_size], alpha=0.6)
    ax.axhline(y=0, color='red', linestyle='--', alpha=0.5)
    ax.axhline(y=steering_stats['mean'], color='green', linestyle='--', alpha=0.5)
    ax.set_xlabel('Step')
    ax.set_ylabel('Steering Value')
    ax.set_title(f'Steering Over Time (First {sample_size} steps)')
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, 'steering_analysis.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    return output_path

def create_episode_performance_plot(episode_stats, output_dir):
    """Create episode performance visualization"""
    if not episode_stats or not episode_stats['rewards']:
        return None
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Episode Performance', fontsize=16, fontweight='bold')
    
    rewards = episode_stats['rewards']
    lengths = episode_stats['lengths']
    episodes = range(1, len(rewards) + 1)
    
    # Reward over episodes
    ax = axes[0, 0]
    ax.plot(episodes, rewards, marker='o', alpha=0.6)
    ax.axhline(y=0, color='red', linestyle='--', alpha=0.5, label='Zero')
    ax.axhline(y=np.mean(rewards), color='green', linestyle='--', alpha=0.5, label=f'Mean: {np.mean(rewards):.2f}')
    ax.set_xlabel('Episode')
    ax.set_ylabel('Total Reward')
    ax.set_title('Episode Rewards Over Time')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Reward histogram
    ax = axes[0, 1]
    ax.hist(rewards, bins=30, edgecolor='black', alpha=0.7)
    ax.axvline(x=0, color='red', linestyle='--', alpha=0.5)
    ax.set_xlabel('Episode Reward')
    ax.set_ylabel('Frequency')
    ax.set_title('Reward Distribution')
    ax.grid(True, alpha=0.3)
    
    # Episode length
    ax = axes[1, 0]
    ax.plot(episodes, lengths, marker='o', alpha=0.6, color='orange')
    ax.set_xlabel('Episode')
    ax.set_ylabel('Episode Length (steps)')
    ax.set_title('Episode Length Over Time')
    ax.grid(True, alpha=0.3)
    
    # Statistics
    ax = axes[1, 1]
    ax.axis('off')
    stats_text = f"""
    EPISODE STATISTICS
    ═══════════════════
    Total Episodes:  {episode_stats['total_episodes']:>6d}
    Success Rate:    {episode_stats['success_rate']*100:>6.1f}%
    
    Mean Reward:     {episode_stats['mean_reward']:>8.2f}
    Std Reward:      {episode_stats['std_reward']:>8.2f}
    Min Reward:      {episode_stats['min_reward']:>8.2f}
    Max Reward:      {episode_stats['max_reward']:>8.2f}
    
    Mean Length:     {episode_stats['mean_length']:>8.1f}
    """
    ax.text(0.1, 0.5, stats_text, fontsize=12, family='monospace',
            verticalalignment='center')
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, 'episode_performance.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    return output_path

def create_action_diversity_plot(action_stats, output_dir):
    """Create action diversity visualization"""
    if not action_stats:
        return None
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle('Action Diversity Analysis', fontsize=16, fontweight='bold')
    
    # Bar plot of means
    ax = axes[0]
    actions = ['Steering', 'Throttle', 'Brake']
    means = [
        action_stats['steering']['mean'],
        action_stats['throttle']['mean'],
        action_stats['brake']['mean']
    ]
    colors = ['blue', 'green', 'red']
    ax.bar(actions, means, color=colors, alpha=0.6)
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5)
    ax.set_ylabel('Mean Value')
    ax.set_title('Mean Action Values')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Bar plot of stds
    ax = axes[1]
    stds = [
        action_stats['steering']['std'],
        action_stats['throttle']['std'],
        action_stats['brake']['std']
    ]
    bars = ax.bar(actions, stds, color=colors, alpha=0.6)
    ax.set_ylabel('Standard Deviation')
    ax.set_title('Action Exploration (Std Dev)')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add threshold line for steering
    ax.axhline(y=Config.STEERING_STD_THRESHOLD, color='red', linestyle='--', 
               alpha=0.5, label=f'Critical: {Config.STEERING_STD_THRESHOLD}')
    ax.legend()
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, 'action_diversity.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    return output_path

def create_throttle_brake_plot(tb_stats, output_dir):
    """Create detailed throttle/brake analysis plot"""
    if not tb_stats:
        return None
    
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    fig.suptitle('Throttle & Brake Analysis', fontsize=16, fontweight='bold')
    
    throttle_data = tb_stats['throttle']['data']
    brake_data = tb_stats['brake']['data']
    combined_data = tb_stats['combined']['data']
    
    # Throttle histogram
    ax = axes[0, 0]
    ax.hist(throttle_data, bins=50, color='green', edgecolor='black', alpha=0.7)
    ax.axvline(x=tb_stats['throttle']['mean'], color='red', linestyle='--', 
               label=f"Mean: {tb_stats['throttle']['mean']:.3f}")
    ax.set_xlabel('Throttle Value')
    ax.set_ylabel('Frequency')
    ax.set_title('Throttle Distribution')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Brake histogram
    ax = axes[0, 1]
    ax.hist(brake_data, bins=50, color='red', edgecolor='black', alpha=0.7)
    ax.axvline(x=tb_stats['brake']['mean'], color='darkred', linestyle='--', 
               label=f"Mean: {tb_stats['brake']['mean']:.3f}")
    ax.set_xlabel('Brake Value')
    ax.set_ylabel('Frequency')
    ax.set_title('Brake Distribution')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Combined (net acceleration)
    ax = axes[0, 2]
    ax.hist(combined_data, bins=50, color='purple', edgecolor='black', alpha=0.7)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=0.5)
    ax.axvline(x=tb_stats['combined']['mean'], color='red', linestyle='--', 
               label=f"Mean: {tb_stats['combined']['mean']:.3f}")
    ax.set_xlabel('Net Acceleration (Throttle - Brake)')
    ax.set_ylabel('Frequency')
    ax.set_title('Net Acceleration Distribution')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Throttle time series
    ax = axes[1, 0]
    sample_size = min(1000, len(throttle_data))
    ax.plot(throttle_data[:sample_size], color='green', alpha=0.6, linewidth=0.5)
    ax.axhline(y=tb_stats['throttle']['mean'], color='red', linestyle='--', alpha=0.5)
    ax.set_xlabel('Step')
    ax.set_ylabel('Throttle')
    ax.set_title(f'Throttle Over Time (First {sample_size} steps)')
    ax.grid(True, alpha=0.3)
    
    # Brake time series
    ax = axes[1, 1]
    ax.plot(brake_data[:sample_size], color='red', alpha=0.6, linewidth=0.5)
    ax.axhline(y=tb_stats['brake']['mean'], color='darkred', linestyle='--', alpha=0.5)
    ax.set_xlabel('Step')
    ax.set_ylabel('Brake')
    ax.set_title(f'Brake Over Time (First {sample_size} steps)')
    ax.grid(True, alpha=0.3)
    
    # Statistics text
    ax = axes[1, 2]
    ax.axis('off')
    stats_text = f"""
    THROTTLE & BRAKE STATS
    ══════════════════════
    
    Throttle:
      Mean:    {tb_stats['throttle']['mean']:>6.3f}
      Std:     {tb_stats['throttle']['std']:>6.3f}
      Range:   [{tb_stats['throttle']['min']:.2f}, {tb_stats['throttle']['max']:.2f}]
    
    Brake:
      Mean:    {tb_stats['brake']['mean']:>6.3f}
      Std:     {tb_stats['brake']['std']:>6.3f}
      Range:   [{tb_stats['brake']['min']:.2f}, {tb_stats['brake']['max']:.2f}]
    
    Issues:
      Both High: {tb_stats['both_high_pct']:>5.1f}%
      Both Low:  {tb_stats['both_low_pct']:>5.1f}%
    """
    ax.text(0.1, 0.5, stats_text, fontsize=11, family='monospace',
            verticalalignment='center')
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, 'throttle_brake_analysis.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    return output_path

def create_reward_components_plot(reward_components, output_dir):
    """Create reward components visualization"""
    if not reward_components:
        return None
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('Reward Components Analysis', fontsize=16, fontweight='bold')
    
    # Component contributions (bar plot)
    ax = axes[0, 0]
    components = list(reward_components.keys())
    sums = [reward_components[c]['sum'] for c in components]
    colors_map = {
        'progress': 'green',
        'speed': 'blue',
        'lateral': 'orange',
        'heading': 'purple',
        'angular': 'brown',
        'smoothness': 'pink',
        'airborne': 'gray',
        'steering': 'red'
    }
    colors = [colors_map.get(c, 'gray') for c in components]
    
    ax.barh(components, sums, color=colors, alpha=0.7)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=0.5)
    ax.set_xlabel('Total Contribution')
    ax.set_title('Reward Component Contributions')
    ax.grid(True, alpha=0.3, axis='x')
    
    # Mean values
    ax = axes[0, 1]
    means = [reward_components[c]['mean'] for c in components]
    ax.barh(components, means, color=colors, alpha=0.7)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=0.5)
    ax.set_xlabel('Mean Value per Step')
    ax.set_title('Average Reward per Component')
    ax.grid(True, alpha=0.3, axis='x')
    
    # Time series of major components
    ax = axes[1, 0]
    sample_size = min(500, len(reward_components['progress']['data']))
    for component in ['progress', 'speed', 'steering']:
        if component in reward_components:
            data = reward_components[component]['data'][:sample_size]
            ax.plot(data, label=component.capitalize(), alpha=0.6)
    ax.axhline(y=0, color='black', linestyle='--', alpha=0.3)
    ax.set_xlabel('Step')
    ax.set_ylabel('Reward Value')
    ax.set_title(f'Key Components Over Time (First {sample_size} steps)')
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Statistics table
    ax = axes[1, 1]
    ax.axis('off')
    stats_lines = ["  COMPONENT STATISTICS", "  ═══════════════════════", ""]
    for comp in components:
        stats = reward_components[comp]
        stats_lines.append(f"  {comp.capitalize():12s}")
        stats_lines.append(f"    Mean: {stats['mean']:>7.3f}")
        stats_lines.append(f"    Sum:  {stats['sum']:>7.1f}")
        stats_lines.append("")
    
    stats_text = "\n".join(stats_lines)
    ax.text(0.1, 0.9, stats_text, fontsize=10, family='monospace',
            verticalalignment='top')
    
    plt.tight_layout()
    
    output_path = os.path.join(output_dir, 'reward_components.png')
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    
    return output_path

# ============================================================================
# Rollout Inspection
# ============================================================================

def parse_rollout_selection(selection_str, rollouts):
    """Parse rollout selection string (first, last, N, -N, etc.)"""
    if not rollouts:
        return None
    
    selection_str = selection_str.lower().strip()
    
    # Get rollout numbers sorted
    rollout_nums = sorted([r.get('rollout_number', 0) for r in rollouts])
    
    if selection_str == "first":
        return rollout_nums[0]
    elif selection_str == "last" or selection_str == "latest":
        return rollout_nums[-1]
    elif selection_str.startswith("-"):
        # Negative index (e.g. -1 = last, -2 = second to last)
        try:
            idx = int(selection_str)
            if abs(idx) <= len(rollout_nums):
                return rollout_nums[idx]
        except ValueError:
            pass
    else:
        # Try as direct number
        try:
            return int(selection_str)
        except ValueError:
            pass
    
    return None

def inspect_rollout(export_dir, rollout_number=None):
    """Inspect a single rollout in detail"""
    print(f"\n{Config.COLOR_INFO}Inspecting Rollout...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    # Select rollout
    if rollout_number is not None:
        # Find specific rollout
        target = None
        for r in rollouts:
            if r.get('rollout_number') == rollout_number:
                target = r
                break
        
        if not target:
            print(f"{Config.COLOR_ERROR}❌ Rollout #{rollout_number} not found{Config.COLOR_RESET}")
            print(f"{Config.COLOR_INFO}Available rollouts: {min([r['rollout_number'] for r in rollouts])} - {max([r['rollout_number'] for r in rollouts])}{Config.COLOR_RESET}\n")
            return
        
        rollout = target
    else:
        # Use latest rollout
        rollout = max(rollouts, key=lambda x: x.get('rollout_number', 0))
    
    rollout_num = rollout.get('rollout_number', '?')
    
    print(f"{Config.COLOR_INFO}╔════════════════════════════════════════════════════════════════╗{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}║              ROLLOUT #{rollout_num} DETAILED INSPECTION{' ' * (28 - len(str(rollout_num)))}║{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}╚════════════════════════════════════════════════════════════════╝{Config.COLOR_RESET}\n")
    
    # Basic info
    total_reward = rollout.get('total_reward', 0.0)
    experiences = rollout.get('experiences', [])
    num_steps = len(experiences)
    
    reward_status = 'ok' if total_reward > 50 else ('warning' if total_reward > 0 else 'error')
    
    print(f"{Config.COLOR_INFO}EPISODE SUMMARY:{Config.COLOR_RESET}")
    print_status_bar(f"Total Reward:  {total_reward:>8.2f}", reward_status)
    print_status_bar(f"Episode Steps: {num_steps:>8d}", 'info')
    print_status_bar(f"Avg Reward/Step: {total_reward/num_steps if num_steps > 0 else 0:>6.3f}", 'info')
    
    if not experiences:
        print(f"\n{Config.COLOR_WARNING}⚠️  No experience data found in rollout{Config.COLOR_RESET}\n")
        return
    
    # Extract actions
    steers = []
    throttles = []
    brakes = []
    rewards = []
    
    for exp in experiences:
        action = exp.get('action', {})
        steers.append(action.get('steer', 0.0))
        throttles.append(action.get('throttle', 0.0))
        brakes.append(action.get('brake', 0.0))
        rewards.append(exp.get('reward', 0.0))
    
    # Action statistics
    print(f"\n{Config.COLOR_INFO}ACTION STATISTICS:{Config.COLOR_RESET}")
    
    steer_mean = np.mean(steers)
    steer_std = np.std(steers)
    
    steer_mean_status = 'ok' if abs(steer_mean) < 0.5 else ('warning' if abs(steer_mean) < 0.7 else 'error')
    steer_std_status = 'ok' if steer_std > 0.15 else ('warning' if steer_std > 0.1 else 'error')
    
    print(f"  Steering:")
    print_status_bar(f"    Mean: {steer_mean:>7.3f} | Std: {steer_std:.3f} | Range: [{np.min(steers):.2f}, {np.max(steers):.2f}]", steer_mean_status)
    
    print(f"  Throttle:")
    print_status_bar(f"    Mean: {np.mean(throttles):>7.3f} | Std: {np.std(throttles):.3f} | Range: [{np.min(throttles):.2f}, {np.max(throttles):.2f}]", 'info')
    
    print(f"  Brake:")
    print_status_bar(f"    Mean: {np.mean(brakes):>7.3f} | Std: {np.std(brakes):.3f} | Range: [{np.min(brakes):.2f}, {np.max(brakes):.2f}]", 'info')
    
    # Reward breakdown
    print(f"\n{Config.COLOR_INFO}REWARD BREAKDOWN:{Config.COLOR_RESET}")
    
    positive_rewards = [r for r in rewards if r > 0]
    negative_rewards = [r for r in rewards if r < 0]
    zero_rewards = [r for r in rewards if r == 0]
    
    print(f"  Positive Steps: {len(positive_rewards):>6d} ({len(positive_rewards)/num_steps*100:>5.1f}%)")
    if positive_rewards:
        print(f"    Sum: {sum(positive_rewards):>8.2f} | Avg: {np.mean(positive_rewards):>6.3f}")
    
    print(f"  Negative Steps: {len(negative_rewards):>6d} ({len(negative_rewards)/num_steps*100:>5.1f}%)")
    if negative_rewards:
        print(f"    Sum: {sum(negative_rewards):>8.2f} | Avg: {np.mean(negative_rewards):>6.3f}")
    
    print(f"  Zero Steps:     {len(zero_rewards):>6d} ({len(zero_rewards)/num_steps*100:>5.1f}%)")
    
    # Sample steps (first 5 and last 5)
    print(f"\n{Config.COLOR_INFO}SAMPLE STEPS:{Config.COLOR_RESET}")
    
    print(f"\n  First 5 Steps:")
    print(f"  {'Step':>6} | {'Reward':>8} | {'Steer':>7} | {'Throttle':>8} | {'Brake':>7}")
    print(f"  {'-'*6}-+-{'-'*8}-+-{'-'*7}-+-{'-'*8}-+-{'-'*7}")
    
    for i in range(min(5, len(experiences))):
        exp = experiences[i]
        action = exp.get('action', {})
        reward = exp.get('reward', 0.0)
        
        reward_color = Config.COLOR_OK if reward > 0 else (Config.COLOR_ERROR if reward < 0 else Config.COLOR_INFO)
        print(f"  {i:>6} | {reward_color}{reward:>8.3f}{Config.COLOR_RESET} | {action.get('steer', 0):>7.2f} | {action.get('throttle', 0):>8.2f} | {action.get('brake', 0):>7.2f}")
    
    if num_steps > 10:
        print(f"\n  Last 5 Steps:")
        print(f"  {'Step':>6} | {'Reward':>8} | {'Steer':>7} | {'Throttle':>8} | {'Brake':>7}")
        print(f"  {'-'*6}-+-{'-'*8}-+-{'-'*7}-+-{'-'*8}-+-{'-'*7}")
        
        for i in range(max(0, num_steps-5), num_steps):
            exp = experiences[i]
            action = exp.get('action', {})
            reward = exp.get('reward', 0.0)
            
            reward_color = Config.COLOR_OK if reward > 0 else (Config.COLOR_ERROR if reward < 0 else Config.COLOR_INFO)
            print(f"  {i:>6} | {reward_color}{reward:>8.3f}{Config.COLOR_RESET} | {action.get('steer', 0):>7.2f} | {action.get('throttle', 0):>8.2f} | {action.get('brake', 0):>7.2f}")
    
    # Detect issues in this rollout
    print(f"\n{Config.COLOR_INFO}ISSUES IN THIS ROLLOUT:{Config.COLOR_RESET}")
    
    issues_found = False
    
    if abs(steer_mean) > Config.STEERING_MEAN_THRESHOLD:
        print(f"  {Config.COLOR_ERROR}❌ Steering heavily biased ({steer_mean:.3f}){Config.COLOR_RESET}")
        issues_found = True
    
    if steer_std < Config.STEERING_STD_THRESHOLD:
        print(f"  {Config.COLOR_ERROR}❌ No steering exploration (Std: {steer_std:.3f}){Config.COLOR_RESET}")
        issues_found = True
    
    if len(zero_rewards) / num_steps > 0.8:
        print(f"  {Config.COLOR_WARNING}⚠️  Too many zero rewards ({len(zero_rewards)/num_steps*100:.1f}%){Config.COLOR_RESET}")
        issues_found = True
    
    if total_reward < 0:
        print(f"  {Config.COLOR_WARNING}⚠️  Negative total reward ({total_reward:.2f}){Config.COLOR_RESET}")
        issues_found = True
    
    if not issues_found:
        print(f"  {Config.COLOR_OK}✅ No obvious issues detected in this rollout{Config.COLOR_RESET}")
    
    print()

def browse_rollouts(export_dir):
    """Interactive browsing of rollouts"""
    print(f"\n{Config.COLOR_INFO}Browse Mode - Navigate through rollouts{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    rollout_nums = sorted([r.get('rollout_number', 0) for r in rollouts])
    current_idx = len(rollout_nums) - 1  # Start with latest
    
    print(f"{Config.COLOR_INFO}Commands: [n]ext, [p]revious, [f]irst, [l]ast, [q]uit{Config.COLOR_RESET}\n")
    
    while True:
        current_num = rollout_nums[current_idx]
        
        # Find and show current rollout
        rollout = next((r for r in rollouts if r.get('rollout_number') == current_num), None)
        if not rollout:
            print(f"{Config.COLOR_ERROR}Error loading rollout #{current_num}{Config.COLOR_RESET}")
            break
        
        # Show brief summary
        total_reward = rollout.get('total_reward', 0.0)
        num_steps = len(rollout.get('experiences', []))
        
        print(f"\n{Config.COLOR_INFO}{'═'*60}{Config.COLOR_RESET}")
        print(f"{Config.COLOR_INFO}Rollout #{current_num} ({current_idx + 1}/{len(rollout_nums)}){Config.COLOR_RESET}")
        print(f"  Reward: {total_reward:.2f} | Steps: {num_steps}")
        print(f"{Config.COLOR_INFO}{'═'*60}{Config.COLOR_RESET}\n")
        
        # Get user input
        cmd = input(f"{Config.COLOR_INFO}browse>{Config.COLOR_RESET} ").lower().strip()
        
        if cmd == 'n' or cmd == 'next':
            if current_idx < len(rollout_nums) - 1:
                current_idx += 1
            else:
                print(f"{Config.COLOR_WARNING}Already at last rollout{Config.COLOR_RESET}")
        elif cmd == 'p' or cmd == 'prev' or cmd == 'previous':
            if current_idx > 0:
                current_idx -= 1
            else:
                print(f"{Config.COLOR_WARNING}Already at first rollout{Config.COLOR_RESET}")
        elif cmd == 'f' or cmd == 'first':
            current_idx = 0
        elif cmd == 'l' or cmd == 'last' or cmd == 'latest':
            current_idx = len(rollout_nums) - 1
        elif cmd == 'i' or cmd == 'inspect':
            inspect_rollout(export_dir, current_num)
        elif cmd == 'q' or cmd == 'quit' or cmd == 'exit':
            print(f"{Config.COLOR_INFO}Exiting browse mode{Config.COLOR_RESET}\n")
            break
        elif cmd == '':
            continue
        else:
            try:
                # Try to jump to specific index or number
                target = int(cmd)
                if target in rollout_nums:
                    current_idx = rollout_nums.index(target)
                elif 0 <= target < len(rollout_nums):
                    current_idx = target
                else:
                    print(f"{Config.COLOR_WARNING}Invalid index or rollout number{Config.COLOR_RESET}")
            except ValueError:
                print(f"{Config.COLOR_WARNING}Unknown command. Use: n/p/f/l/i/q{Config.COLOR_RESET}")

def compare_rollouts(export_dir, num1, num2):
    """Compare two rollouts side by side"""
    print(f"\n{Config.COLOR_INFO}Comparing Rollouts #{num1} vs #{num2}...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    # Find both rollouts
    r1 = next((r for r in rollouts if r.get('rollout_number') == num1), None)
    r2 = next((r for r in rollouts if r.get('rollout_number') == num2), None)
    
    if not r1:
        print(f"{Config.COLOR_ERROR}Rollout #{num1} not found{Config.COLOR_RESET}\n")
        return
    if not r2:
        print(f"{Config.COLOR_ERROR}Rollout #{num2} not found{Config.COLOR_RESET}\n")
        return
    
    # Extract data for both
    def extract_stats(rollout):
        exps = rollout.get('experiences', [])
        steers = [e.get('action', {}).get('steer', 0) for e in exps]
        throttles = [e.get('action', {}).get('throttle', 0) for e in exps]
        brakes = [e.get('action', {}).get('brake', 0) for e in exps]
        rewards = [e.get('reward', 0) for e in exps]
        
        return {
            'total_reward': rollout.get('total_reward', 0.0),
            'steps': len(exps),
            'steer_mean': np.mean(steers) if steers else 0,
            'steer_std': np.std(steers) if steers else 0,
            'throttle_mean': np.mean(throttles) if throttles else 0,
            'brake_mean': np.mean(brakes) if brakes else 0,
            'reward_mean': np.mean(rewards) if rewards else 0
        }
    
    stats1 = extract_stats(r1)
    stats2 = extract_stats(r2)
    
    # Print comparison
    print(f"{Config.COLOR_INFO}╔{'═'*29}╦{'═'*29}╗{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}║ Rollout #{num1:>5d}            ║ Rollout #{num2:>5d}            ║{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}╠{'═'*29}╬{'═'*29}╣{Config.COLOR_RESET}")
    
    def print_row(label, val1, val2, formatter=".2f"):
        diff = val2 - val1
        diff_symbol = "▲" if diff > 0 else ("▼" if diff < 0 else "═")
        diff_color = Config.COLOR_OK if diff > 0 else (Config.COLOR_ERROR if diff < 0 else Config.COLOR_INFO)
        
        print(f"║ {label:12s}            ║                             ║")
        print(f"║   {format(val1, formatter):>20s}  ║   {format(val2, formatter):>20s}  ║ {diff_color}{diff_symbol} {abs(diff):{formatter}}{Config.COLOR_RESET}")
    
    print_row("Total Reward", stats1['total_reward'], stats2['total_reward'])
    print(f"{Config.COLOR_INFO}╠{'═'*29}╬{'═'*29}╣{Config.COLOR_RESET}")
    
    print_row("Steps", float(stats1['steps']), float(stats2['steps']), ".0f")
    print(f"{Config.COLOR_INFO}╠{'═'*29}╬{'═'*29}╣{Config.COLOR_RESET}")
    
    print_row("Steer Mean", stats1['steer_mean'], stats2['steer_mean'], ".3f")
    print_row("Steer Std", stats1['steer_std'], stats2['steer_std'], ".3f")
    print(f"{Config.COLOR_INFO}╠{'═'*29}╬{'═'*29}╣{Config.COLOR_RESET}")
    
    print_row("Throttle Mean", stats1['throttle_mean'], stats2['throttle_mean'], ".3f")
    print_row("Brake Mean", stats1['brake_mean'], stats2['brake_mean'], ".3f")
    print(f"{Config.COLOR_INFO}╠{'═'*29}╬{'═'*29}╣{Config.COLOR_RESET}")
    
    print_row("Reward/Step", stats1['reward_mean'], stats2['reward_mean'], ".3f")
    
    print(f"{Config.COLOR_INFO}╚{'═'*29}╩{'═'*29}╝{Config.COLOR_RESET}\n")

def show_throttle_analysis(export_dir):
    """Detailed throttle and brake analysis"""
    print(f"\n{Config.COLOR_INFO}Analyzing Throttle & Brake...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    tb_stats = analyze_throttle_brake(rollouts)
    if not tb_stats:
        print(f"{Config.COLOR_ERROR}No throttle/brake data found{Config.COLOR_RESET}\n")
        return
    
    print(f"{Config.COLOR_INFO}THROTTLE:{Config.COLOR_RESET}")
    print(f"  Mean: {tb_stats['throttle']['mean']:.3f}")
    print(f"  Std:  {tb_stats['throttle']['std']:.3f}")
    print(f"  Range: [{tb_stats['throttle']['min']:.2f}, {tb_stats['throttle']['max']:.2f}]")
    
    print(f"\n{Config.COLOR_INFO}BRAKE:{Config.COLOR_RESET}")
    print(f"  Mean: {tb_stats['brake']['mean']:.3f}")
    print(f"  Std:  {tb_stats['brake']['std']:.3f}")
    print(f"  Range: [{tb_stats['brake']['min']:.2f}, {tb_stats['brake']['max']:.2f}]")
    
    print(f"\n{Config.COLOR_INFO}NET ACCELERATION (Throttle - Brake):{Config.COLOR_RESET}")
    print(f"  Mean: {tb_stats['combined']['mean']:.3f}")
    print(f"  Std:  {tb_stats['combined']['std']:.3f}")
    
    print(f"\n{Config.COLOR_INFO}ISSUES:{Config.COLOR_RESET}")
    if tb_stats['both_high_pct'] > 10:
        print_status_bar(f"Both Throttle & Brake high: {tb_stats['both_high_pct']:.1f}% of steps", 'warning')
    else:
        print_status_bar(f"Both Throttle & Brake high: {tb_stats['both_high_pct']:.1f}% of steps", 'ok')
    
    if tb_stats['both_low_pct'] > 20:
        print_status_bar(f"Both Throttle & Brake low: {tb_stats['both_low_pct']:.1f}% of steps", 'warning')
    else:
        print_status_bar(f"Both Throttle & Brake low: {tb_stats['both_low_pct']:.1f}% of steps", 'ok')
    
    print()

def show_reward_analysis(export_dir):
    """Detailed reward components analysis"""
    print(f"\n{Config.COLOR_INFO}Analyzing Reward Components...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    reward_components = analyze_reward_components(rollouts)
    
    if not reward_components:
        print(f"{Config.COLOR_WARNING}No reward component breakdown found in rollout data.{Config.COLOR_RESET}")
        print(f"{Config.COLOR_INFO}Tip: Reward components require detailed logging in C++{Config.COLOR_RESET}\n")
        return
    
    print(f"{Config.COLOR_INFO}REWARD COMPONENT CONTRIBUTIONS:{Config.COLOR_RESET}\n")
    
    # Sort by absolute contribution
    sorted_components = sorted(reward_components.items(), 
                               key=lambda x: abs(x[1]['sum']), 
                               reverse=True)
    
    for component, stats in sorted_components:
        color = Config.COLOR_OK if stats['sum'] > 0 else Config.COLOR_ERROR
        print(f"  {component.capitalize():12s}: {color}{stats['sum']:>8.1f}{Config.COLOR_RESET} (mean: {stats['mean']:>6.3f})")
    
    print()

# ============================================================================
# CLI Functions
# ============================================================================

def show_help():
    """Display help information"""
    help_text = f"""
{Config.COLOR_INFO}╔════════════════════════════════════════════════════════════════╗
║         CarAI Training Analysis Tool - Help                    ║
╚════════════════════════════════════════════════════════════════╝{Config.COLOR_RESET}

{Config.COLOR_OK}Analysis Commands:{Config.COLOR_RESET}
  analyze          - Full analysis with all plots
  quick            - Quick status check (no plots)
  throttle         - Detailed throttle/brake analysis
  rewards          - Detailed reward components analysis
  problems         - Detect and list all problems
  trends           - Show training trends over time

{Config.COLOR_OK}Rollout Inspection:{Config.COLOR_RESET}
  inspect [N]      - Inspect single rollout
                     • inspect          → Latest rollout
                     • inspect first    → First rollout
                     • inspect last     → Last rollout
                     • inspect 50       → Rollout #50
                     • inspect -1       → Last rollout
                     • inspect -2       → Second to last
  
  browse           - Interactive browsing through rollouts
  compare N M      - Compare two rollouts side-by-side

{Config.COLOR_OK}Visualization:{Config.COLOR_RESET}
  plots            - Generate all visualization plots

{Config.COLOR_OK}Other:{Config.COLOR_RESET}
  config           - Show configuration
  help             - Show this help
  exit             - Exit the program

{Config.COLOR_INFO}Examples:{Config.COLOR_RESET}
  > quick                  # Fast status check
  > throttle               # Analyze gas & brake
  > inspect first          # Check first rollout
  > compare 1 100          # Compare rollouts #1 and #100
  > browse                 # Navigate through rollouts
  > analyze                # Full analysis with plots
"""
    print(help_text)

def show_config():
    """Display current configuration"""
    print(f"\n{Config.COLOR_INFO}Current Configuration:{Config.COLOR_RESET}")
    print(f"  Export Directory:  {Config.DEFAULT_EXPORT_DIR}")
    print(f"  Output Directory:  {Config.DEFAULT_OUTPUT_DIR}")
    print(f"\n{Config.COLOR_INFO}Thresholds:{Config.COLOR_RESET}")
    print(f"  Steering Mean:     ±{Config.STEERING_MEAN_THRESHOLD}")
    print(f"  Steering Std:      {Config.STEERING_STD_THRESHOLD}")
    print(f"  Throttle Std:      {Config.THROTTLE_STD_THRESHOLD}")
    print(f"  Reward Threshold:  {Config.REWARD_NEGATIVE_THRESHOLD}")
    print(f"  Success Rate:      {Config.SUCCESS_RATE_THRESHOLD * 100}%")
    print()

def print_status_bar(message, status='info'):
    """Print a status message with color"""
    colors = {
        'info': Config.COLOR_INFO,
        'ok': Config.COLOR_OK,
        'warning': Config.COLOR_WARNING,
        'error': Config.COLOR_ERROR
    }
    color = colors.get(status, Config.COLOR_INFO)
    symbols = {
        'info': 'ℹ️ ',
        'ok': '✅',
        'warning': '⚠️ ',
        'error': '❌'
    }
    symbol = symbols.get(status, '')
    print(f"{color}{symbol} {message}{Config.COLOR_RESET}")

def quick_check(export_dir):
    """Quick status check without plots"""
    print(f"\n{Config.COLOR_INFO}Running Quick Check...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    print(f"📊 Analyzing {len(rollouts)} rollouts...\n")
    
    steering_stats = analyze_steering_distribution(rollouts)
    action_stats = analyze_action_diversity(rollouts)
    episode_stats = analyze_episode_performance(rollouts)
    
    # Print steering
    print(f"{Config.COLOR_INFO}STEERING:{Config.COLOR_RESET}")
    if steering_stats:
        mean = steering_stats['mean']
        std = steering_stats['std']
        
        mean_status = 'ok' if abs(mean) < 0.5 else ('warning' if abs(mean) < 0.7 else 'error')
        std_status = 'ok' if std > 0.15 else ('warning' if std > 0.1 else 'error')
        
        print_status_bar(f"Mean: {mean:>7.3f} (Target: ~0.0)", mean_status)
        print_status_bar(f"Std:  {std:>7.3f} (Target: >0.15)", std_status)
    
    # Print episode stats
    print(f"\n{Config.COLOR_INFO}EPISODES:{Config.COLOR_RESET}")
    if episode_stats:
        reward = episode_stats['mean_reward']
        success = episode_stats['success_rate']
        
        reward_status = 'ok' if reward > 50 else ('warning' if reward > 0 else 'error')
        success_status = 'ok' if success > 0.5 else ('warning' if success > 0.3 else 'error')
        
        print_status_bar(f"Mean Reward:  {reward:>7.2f}", reward_status)
        print_status_bar(f"Success Rate: {success*100:>6.1f}%", success_status)
    
    # Detect problems
    problems, warnings = detect_problems(steering_stats, action_stats, episode_stats)
    
    if problems or warnings:
        print(f"\n{Config.COLOR_WARNING}═══════════════════════════════════════════{Config.COLOR_RESET}")
        
        if problems:
            print(f"\n{Config.COLOR_ERROR}CRITICAL ISSUES:{Config.COLOR_RESET}")
            for problem in problems:
                print(f"  {Config.COLOR_ERROR}• {problem}{Config.COLOR_RESET}")
        
        if warnings:
            print(f"\n{Config.COLOR_WARNING}WARNINGS:{Config.COLOR_RESET}")
            for warning in warnings:
                print(f"  {Config.COLOR_WARNING}• {warning}{Config.COLOR_RESET}")
        
        print(f"\n{Config.COLOR_WARNING}═══════════════════════════════════════════{Config.COLOR_RESET}")
    else:
        print(f"\n{Config.COLOR_OK}✅ No critical issues detected!{Config.COLOR_RESET}")
    
    print()

def full_analysis(export_dir, output_dir):
    """Run full analysis with plots"""
    print(f"\n{Config.COLOR_INFO}Running Full Analysis...{Config.COLOR_RESET}\n")
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    print(f"📊 Analyzing {len(rollouts)} rollouts...")
    
    # Run analyses
    steering_stats = analyze_steering_distribution(rollouts)
    action_stats = analyze_action_diversity(rollouts)
    episode_stats = analyze_episode_performance(rollouts)
    tb_stats = analyze_throttle_brake(rollouts)
    reward_components = analyze_reward_components(rollouts)
    
    # Create plots
    print(f"\n{Config.COLOR_INFO}Creating visualizations...{Config.COLOR_RESET}")
    
    plots_created = []
    
    if steering_stats:
        path = create_steering_distribution_plot(steering_stats, output_dir)
        if path:
            plots_created.append(path)
            print_status_bar(f"Created: steering_analysis.png", 'ok')
    
    if episode_stats:
        path = create_episode_performance_plot(episode_stats, output_dir)
        if path:
            plots_created.append(path)
            print_status_bar(f"Created: episode_performance.png", 'ok')
    
    if action_stats:
        path = create_action_diversity_plot(action_stats, output_dir)
        if path:
            plots_created.append(path)
            print_status_bar(f"Created: action_diversity.png", 'ok')
    
    if tb_stats:
        path = create_throttle_brake_plot(tb_stats, output_dir)
        if path:
            plots_created.append(path)
            print_status_bar(f"Created: throttle_brake_analysis.png", 'ok')
    
    if reward_components:
        path = create_reward_components_plot(reward_components, output_dir)
        if path:
            plots_created.append(path)
            print_status_bar(f"Created: reward_components.png", 'ok')
    
    # Print summary
    print(f"\n{Config.COLOR_INFO}═══════════════════════════════════════════{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}ANALYSIS SUMMARY{Config.COLOR_RESET}")
    print(f"{Config.COLOR_INFO}═══════════════════════════════════════════{Config.COLOR_RESET}\n")
    
    if steering_stats:
        print(f"Steering Mean: {steering_stats['mean']:.3f}")
        print(f"Steering Std:  {steering_stats['std']:.3f}")
    
    if tb_stats:
        print(f"Throttle Mean: {tb_stats['throttle']['mean']:.3f}")
        print(f"Brake Mean:    {tb_stats['brake']['mean']:.3f}")
    
    if episode_stats:
        print(f"Mean Reward:   {episode_stats['mean_reward']:.2f}")
        print(f"Success Rate:  {episode_stats['success_rate']*100:.1f}%")
    
    # Detect problems
    problems, warnings = detect_problems(steering_stats, action_stats, episode_stats)
    
    if problems:
        print(f"\n{Config.COLOR_ERROR}CRITICAL ISSUES:{Config.COLOR_RESET}")
        for problem in problems:
            print(f"  • {problem}")
    
    if warnings:
        print(f"\n{Config.COLOR_WARNING}WARNINGS:{Config.COLOR_RESET}")
        for warning in warnings:
            print(f"  • {warning}")
    
    if not problems and not warnings:
        print(f"\n{Config.COLOR_OK}✅ No issues detected - training looks healthy!{Config.COLOR_RESET}")
    
    print(f"\n{Config.COLOR_OK}📁 Plots saved to: {output_dir}{Config.COLOR_RESET}")
    print(f"{Config.COLOR_OK}   Total plots created: {len(plots_created)}{Config.COLOR_RESET}")
    print()

def show_trends(export_dir):
    """Show training trends"""
    print(f"\n{Config.COLOR_INFO}Analyzing Training Trends...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    episode_stats = analyze_episode_performance(rollouts)
    
    if not episode_stats or len(episode_stats['rewards']) < 2:
        print(f"{Config.COLOR_WARNING}Not enough data for trend analysis{Config.COLOR_RESET}")
        return
    
    rewards = episode_stats['rewards']
    
    # Calculate trend (last 10 vs first 10)
    n = min(10, len(rewards) // 2)
    early_mean = np.mean(rewards[:n])
    late_mean = np.mean(rewards[-n:])
    
    trend = late_mean - early_mean
    trend_pct = (trend / abs(early_mean)) * 100 if early_mean != 0 else 0
    
    print(f"Early Episodes (1-{n}):  Mean Reward = {early_mean:>7.2f}")
    print(f"Late Episodes  ({len(rewards)-n+1}-{len(rewards)}): Mean Reward = {late_mean:>7.2f}")
    print()
    
    if trend > 10:
        print_status_bar(f"📈 IMPROVING: +{trend:.2f} ({trend_pct:+.1f}%)", 'ok')
        print(f"   Training is progressing well!")
    elif trend > 0:
        print_status_bar(f"📊 SLIGHT IMPROVEMENT: +{trend:.2f} ({trend_pct:+.1f}%)", 'info')
        print(f"   Training is making slow progress")
    elif trend > -10:
        print_status_bar(f"📉 SLIGHT DECLINE: {trend:.2f} ({trend_pct:+.1f}%)", 'warning')
        print(f"   Training may be plateauing")
    else:
        print_status_bar(f"⚠️  DECLINING: {trend:.2f} ({trend_pct:+.1f}%)", 'error')
        print(f"   Training is getting worse - consider reset!")
    
    print()

def show_problems(export_dir):
    """Detect and list all problems"""
    print(f"\n{Config.COLOR_INFO}Detecting Problems...{Config.COLOR_RESET}\n")
    
    rollouts = load_rollout_files(export_dir)
    if not rollouts:
        return
    
    steering_stats = analyze_steering_distribution(rollouts)
    action_stats = analyze_action_diversity(rollouts)
    episode_stats = analyze_episode_performance(rollouts)
    
    problems, warnings = detect_problems(steering_stats, action_stats, episode_stats)
    
    if not problems and not warnings:
        print(f"{Config.COLOR_OK}✅ No problems detected!{Config.COLOR_RESET}")
        print(f"{Config.COLOR_OK}   Training appears healthy.{Config.COLOR_RESET}\n")
        return
    
    if problems:
        print(f"{Config.COLOR_ERROR}╔════════════════════════════════════════════╗{Config.COLOR_RESET}")
        print(f"{Config.COLOR_ERROR}║          CRITICAL ISSUES DETECTED          ║{Config.COLOR_RESET}")
        print(f"{Config.COLOR_ERROR}╚════════════════════════════════════════════╝{Config.COLOR_RESET}\n")
        
        for i, problem in enumerate(problems, 1):
            print(f"{Config.COLOR_ERROR}{i}. {problem}{Config.COLOR_RESET}\n")
        
        print(f"{Config.COLOR_ERROR}🚨 ACTION REQUIRED:{Config.COLOR_RESET}")
        print(f"   These issues will prevent successful training.")
        print(f"   See TRAINING_FIX_GUIDE.md for solutions.\n")
    
    if warnings:
        print(f"{Config.COLOR_WARNING}╔════════════════════════════════════════════╗{Config.COLOR_RESET}")
        print(f"{Config.COLOR_WARNING}║              WARNINGS DETECTED             ║{Config.COLOR_RESET}")
        print(f"{Config.COLOR_WARNING}╚════════════════════════════════════════════╝{Config.COLOR_RESET}\n")
        
        for i, warning in enumerate(warnings, 1):
            print(f"{Config.COLOR_WARNING}{i}. {warning}{Config.COLOR_RESET}\n")
        
        print(f"{Config.COLOR_WARNING}⚠️  RECOMMENDED:{Config.COLOR_RESET}")
        print(f"   Monitor these issues - they may indicate problems.\n")

def execute_input(input_command, export_dir, output_dir):
    """Execute user command"""
    cmd = input_command.lower().strip()
    parts = cmd.split()
    
    if cmd == "exit" or cmd == "quit":
        return False
    
    elif cmd == "help" or cmd == "h" or cmd == "?":
        show_help()
    
    elif cmd == "analyze" or cmd == "full":
        full_analysis(export_dir, output_dir)
    
    elif cmd == "quick" or cmd == "q":
        quick_check(export_dir)
    
    elif cmd == "throttle" or cmd == "gas" or cmd == "brake":
        show_throttle_analysis(export_dir)
    
    elif cmd == "rewards" or cmd == "reward":
        show_reward_analysis(export_dir)
    
    elif cmd == "browse":
        browse_rollouts(export_dir)
    
    elif cmd.startswith("compare"):
        # Parse compare N M
        if len(parts) >= 3:
            try:
                num1 = int(parts[1])
                num2 = int(parts[2])
                compare_rollouts(export_dir, num1, num2)
            except ValueError:
                print(f"{Config.COLOR_WARNING}Usage: compare N M (where N and M are rollout numbers){Config.COLOR_RESET}\n")
        else:
            print(f"{Config.COLOR_WARNING}Usage: compare N M{Config.COLOR_RESET}")
            print(f"Example: compare 1 100\n")
    
    elif cmd.startswith("inspect") or cmd.startswith("debug"):
        # Parse rollout selection
        rollout_num = None
        
        if len(parts) > 1:
            # Load rollouts for parsing
            rollouts = load_rollout_files(export_dir)
            if rollouts:
                rollout_num = parse_rollout_selection(parts[1], rollouts)
                
                if rollout_num is None:
                    print(f"{Config.COLOR_WARNING}Invalid selection: '{parts[1]}'{Config.COLOR_RESET}")
                    print(f"Valid options: first, last, -N, or rollout number\n")
                    return True
        
        inspect_rollout(export_dir, rollout_num)
    
    elif cmd == "plots":
        print(f"\n{Config.COLOR_INFO}Generating plots...{Config.COLOR_RESET}")
        rollouts = load_rollout_files(export_dir)
        if rollouts:
            os.makedirs(output_dir, exist_ok=True)
            
            # Generate all analyses
            steering_stats = analyze_steering_distribution(rollouts)
            action_stats = analyze_action_diversity(rollouts)
            episode_stats = analyze_episode_performance(rollouts)
            tb_stats = analyze_throttle_brake(rollouts)
            reward_components = analyze_reward_components(rollouts)
            
            # Create all plots
            if steering_stats:
                create_steering_distribution_plot(steering_stats, output_dir)
                print_status_bar("Created: steering_analysis.png", 'ok')
            
            if action_stats:
                create_action_diversity_plot(action_stats, output_dir)
                print_status_bar("Created: action_diversity.png", 'ok')
            
            if episode_stats:
                create_episode_performance_plot(episode_stats, output_dir)
                print_status_bar("Created: episode_performance.png", 'ok')
            
            if tb_stats:
                create_throttle_brake_plot(tb_stats, output_dir)
                print_status_bar("Created: throttle_brake_analysis.png", 'ok')
            
            if reward_components:
                create_reward_components_plot(reward_components, output_dir)
                print_status_bar("Created: reward_components.png", 'ok')
            
            print(f"\n{Config.COLOR_OK}✅ All plots created in: {output_dir}{Config.COLOR_RESET}\n")
    
    elif cmd == "trends" or cmd == "trend":
        show_trends(export_dir)
    
    elif cmd == "problems" or cmd == "issues":
        show_problems(export_dir)
    
    elif cmd == "config" or cmd == "settings":
        show_config()
    
    elif cmd == "":
        pass  # Empty input, do nothing
    
    else:
        print(f"{Config.COLOR_WARNING}Unknown command: '{cmd}'{Config.COLOR_RESET}")
        print(f"Type 'help' for available commands.\n")
    
    return True

def get_input():
    """Get input from command line"""
    return input(f"{Config.COLOR_INFO}carAI>{Config.COLOR_RESET} ")

def menu():
    """Display menu (simplified for cleaner interface)"""
    pass  # Menu removed for cleaner look, help command available

def start():
    """Start information"""
    banner = f"""
{Config.COLOR_INFO}╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║         🏎️   CarAI Training Analysis Tool   🏎️                ║
║                                                                ║
║              Analyze • Visualize • Detect Issues               ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝{Config.COLOR_RESET}

{Config.COLOR_OK}Welcome!{Config.COLOR_RESET} Type '{Config.COLOR_OK}help{Config.COLOR_RESET}' for available commands, '{Config.COLOR_OK}quick{Config.COLOR_RESET}' for fast check.
"""
    print(banner)

def main():
    """Main function"""
    # Setup paths
    export_dir = Config.DEFAULT_EXPORT_DIR
    output_dir = Config.DEFAULT_OUTPUT_DIR
    
    # Allow override via command line
    if len(sys.argv) > 1:
        export_dir = sys.argv[1]
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]
    
    # Ensure export dir exists
    if not os.path.exists(export_dir):
        print(f"{Config.COLOR_ERROR}Error: Export directory not found: {export_dir}{Config.COLOR_RESET}")
        print(f"{Config.COLOR_INFO}Usage: python {sys.argv[0]} [export_dir] [output_dir]{Config.COLOR_RESET}")
        return
    
    start()
    
    try:
        while True:
            menu()
            user_input = get_input()
            
            if not execute_input(user_input, export_dir, output_dir):
                print(f"\n{Config.COLOR_OK}👋 Goodbye!{Config.COLOR_RESET}\n")
                break
    
    except KeyboardInterrupt:
        print(f"\n\n{Config.COLOR_WARNING}Interrupted by user{Config.COLOR_RESET}")
        print(f"{Config.COLOR_OK}👋 Goodbye!{Config.COLOR_RESET}\n")
    
    except Exception as e:
        print(f"\n{Config.COLOR_ERROR}Unexpected error: {e}{Config.COLOR_RESET}\n")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()