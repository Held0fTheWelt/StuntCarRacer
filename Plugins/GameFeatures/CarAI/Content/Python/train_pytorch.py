"""
PyTorch Training Script für Racing AI
Liest exportierte Daten von Unreal Engine und trainiert ein PPO-Modell
"""

import json
import os
import glob
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Normal
from collections import deque
import matplotlib.pyplot as plt
from pathlib import Path

# ============================================================================
# Neural Network Definition
# ============================================================================

class PolicyNetwork(nn.Module):
    """Policy Network für kontinuierliche Actions"""
    def __init__(self, obs_size, hidden_sizes=[128, 128], action_size=3):
        super(PolicyNetwork, self).__init__()
        
        # Shared layers
        layers = []
        input_size = obs_size
        for hidden_size in hidden_sizes:
            layers.extend([
                nn.Linear(input_size, hidden_size),
                nn.ReLU()
            ])
            input_size = hidden_size
        
        self.shared = nn.Sequential(*layers)
        
        # Policy head (mean)
        self.policy_mean = nn.Linear(input_size, action_size)
        self.policy_std = nn.Parameter(torch.ones(action_size) * 0.5)  # Learnable std
        
        # Value head
        self.value = nn.Linear(input_size, 1)
        
        # Initialisiere Gewichte für Stabilität
        self._initialize_weights()
    
    def _initialize_weights(self):
        """Initialisiere Gewichte für numerische Stabilität"""
        for m in self.modules():
            if isinstance(m, nn.Linear):
                nn.init.orthogonal_(m.weight, gain=np.sqrt(2))
                nn.init.constant_(m.bias, 0.0)
        
        # Spezielle Initialisierung für Policy Mean (kleine Werte)
        nn.init.orthogonal_(self.policy_mean.weight, gain=0.01)
        nn.init.constant_(self.policy_mean.bias, 0.0)
        
        # Spezielle Initialisierung für Value Head
        nn.init.orthogonal_(self.value.weight, gain=1.0)
        nn.init.constant_(self.value.bias, 0.0)
        
    def forward(self, obs):
        shared_out = self.shared(obs)
        
        # Policy
        mean = torch.tanh(self.policy_mean(shared_out))  # Actions in [-1, 1]
        std = torch.clamp(self.policy_std.exp(), 0.01, 1.0)
        
        # Value
        value = self.value(shared_out).squeeze(-1)
        
        return mean, std, value
    
    def sample_action(self, obs, noise_std=0.5):
        """Sample action mit optionalem Noise"""
        mean, std, value = self.forward(obs)
        
        # Add exploration noise
        std = std + noise_std
        
        dist = Normal(mean, std)
        action = dist.sample()
        log_prob = dist.log_prob(action).sum(dim=-1)
        
        # Clamp actions
        action = torch.clamp(action, -1.0, 1.0)
        
        return action, log_prob, value
    
    def evaluate_action(self, obs, action):
        """Berechne Log-Prob für gegebene Action"""
        mean, std, value = self.forward(obs)
        dist = Normal(mean, std)
        log_prob = dist.log_prob(action).sum(dim=-1)
        return log_prob, value

# ============================================================================
# PPO Training
# ============================================================================

class PPOTrainer:
    def __init__(self, obs_size, hidden_sizes=[128, 128], lr=1e-4, gamma=0.99, 
                 lambda_gae=0.95, clip_range=0.2, value_coef=0.5, entropy_coef=0.05, device=None):
        self.gamma = gamma
        self.lambda_gae = lambda_gae
        self.clip_range = clip_range
        self.value_coef = value_coef
        self.entropy_coef = entropy_coef
        
        # Device: explizit gesetzt, sonst CPU (Standard)
        # GPU ist optional und nur bei expliziter Anforderung
        if device is not None:
            self.device = torch.device(device)
        else:
            # Standard: CPU (schneller für kleine Modelle/Batches)
            self.device = torch.device("cpu")
        
        if self.device.type == "cuda" and torch.cuda.is_available():
            print(f"GPU verwendet: {torch.cuda.get_device_name(0)} (explizit angefordert)")
            # CUDA Optimierungen aktivieren
            torch.backends.cudnn.benchmark = True  # Optimiert für konstante Input-Größen (bessere Performance)
            torch.backends.cudnn.deterministic = False  # Für bessere Performance (Trade-off: weniger deterministisch)
        else:
            print(f"Verwende Device: {self.device} (Standard: CPU ist für kleine Modelle/Batches schneller)")
        
        # Network
        self.policy = PolicyNetwork(obs_size, hidden_sizes).to(self.device)
        self.optimizer = optim.Adam(self.policy.parameters(), lr=lr)
        
        # Training stats
        self.training_history = {
            'policy_loss': [],
            'value_loss': [],
            'entropy_loss': [],
            'total_loss': [],
            'mean_reward': [],
            'mean_episode_length': []
        }
    
    def compute_gae(self, rewards, values, dones, next_value=0.0):
        """Generalized Advantage Estimation (GPU-optimiert, korrigierte Version)"""
        device = rewards.device
        dtype = rewards.dtype
        
        # Konvertiere next_value zu Tensor falls nötig
        if not isinstance(next_value, torch.Tensor):
            next_value = torch.tensor(next_value, device=device, dtype=dtype)
        
        # Konvertiere dones zu float für Berechnung
        dones_float = dones.float()
        
        # Berechne GAE rückwärts (korrekte Implementierung)
        advantages = torch.zeros_like(rewards)
        gae = torch.tensor(0.0, device=device, dtype=dtype)
        
        # Rückwärts-Iteration (muss sequenziell sein wegen Abhängigkeiten)
        # Verwende next_value für den letzten Schritt
        next_val = next_value
        for step in reversed(range(len(rewards))):
            if dones[step]:
                # Episode endet hier - reset GAE
                delta = rewards[step] - values[step]
                gae = delta
            else:
                # Normale GAE-Berechnung
                delta = rewards[step] + self.gamma * next_val - values[step]
                gae = delta + self.gamma * self.lambda_gae * gae
            
            advantages[step] = gae
            next_val = values[step]  # Für nächsten Schritt
        
        # Returns = advantages + values
        returns = advantages + values
        
        return advantages, returns
    
    def train_step(self, obs, actions, old_log_probs, rewards, dones, values):
        """Ein PPO Training Step"""
        obs = torch.FloatTensor(obs).to(self.device)
        actions = torch.FloatTensor(actions).to(self.device)
        old_log_probs = torch.FloatTensor(old_log_probs).to(self.device)
        rewards = torch.FloatTensor(rewards).to(self.device)
        dones = torch.BoolTensor(dones).to(self.device)
        values = torch.FloatTensor(values).to(self.device)
        
        # Prüfe auf NaN/Inf in Inputs
        if torch.isnan(obs).any() or torch.isinf(obs).any():
            print("Warnung: NaN/Inf in Observations gefunden!")
            return {'policy_loss': 0.0, 'value_loss': 0.0, 'entropy_loss': 0.0, 'total_loss': 0.0}
        
        # Compute GAE (direkt auf GPU, kein CPU-Transfer!)
        next_value = torch.tensor(0.0, device=self.device, dtype=torch.float32)
        advantages, returns = self.compute_gae(rewards, values, dones, next_value)
        
        # Clip extreme values
        advantages = torch.clamp(advantages, -10.0, 10.0)
        returns = torch.clamp(returns, -100.0, 100.0)
        
        # Normalize advantages
        if advantages.std() > 1e-8:
            advantages = (advantages - advantages.mean()) / (advantages.std() + 1e-8)
        else:
            advantages = advantages - advantages.mean()
        
        # Forward pass
        new_log_probs, new_values = self.policy.evaluate_action(obs, actions)
        
        # Prüfe auf NaN nach Forward Pass
        if torch.isnan(new_log_probs).any() or torch.isnan(new_values).any():
            print("Warnung: NaN nach Forward Pass! Überspringe Batch.")
            return {'policy_loss': 0.0, 'value_loss': 0.0, 'entropy_loss': 0.0, 'total_loss': 0.0}
        
        # Clip log probs to prevent extreme ratios
        old_log_probs = torch.clamp(old_log_probs, -10.0, 10.0)
        new_log_probs = torch.clamp(new_log_probs, -10.0, 10.0)
        
        # PPO Loss
        ratio = torch.exp(new_log_probs - old_log_probs)
        ratio = torch.clamp(ratio, 0.1, 10.0)  # Clip ratio to prevent explosion
        surr1 = ratio * advantages
        surr2 = torch.clamp(ratio, 1.0 - self.clip_range, 1.0 + self.clip_range) * advantages
        policy_loss = -torch.min(surr1, surr2).mean()
        
        # Clip policy loss
        policy_loss = torch.clamp(policy_loss, -100.0, 100.0)
        
        # Value Loss mit Clipping (PPO Paper)
        old_values = values  # Alte Values aus Rollout
        value_pred_clipped = old_values + torch.clamp(new_values - old_values, -self.clip_range, self.clip_range)
        value_loss1 = (new_values - returns).pow(2)
        value_loss2 = (value_pred_clipped - returns).pow(2)
        value_loss = torch.max(value_loss1, value_loss2).mean() * self.value_coef
        value_loss = torch.clamp(value_loss, 0.0, 100.0)
        
        # Entropy (für Exploration)
        mean, std, _ = self.policy(obs)
        # Clip std to prevent NaN
        std = torch.clamp(std, 1e-6, 10.0)
        dist = Normal(mean, std)
        entropy = dist.entropy().sum(dim=-1).mean()
        entropy_loss = -entropy * self.entropy_coef
        
        # Total Loss
        total_loss = policy_loss + value_loss + entropy_loss
        
        # Prüfe auf NaN vor Backward
        if torch.isnan(total_loss) or torch.isinf(total_loss):
            print("Warnung: NaN/Inf in Total Loss! Überspringe Backward.")
            return {'policy_loss': 0.0, 'value_loss': 0.0, 'entropy_loss': 0.0, 'total_loss': 0.0}
        
        # Backward
        self.optimizer.zero_grad()
        total_loss.backward()
        torch.nn.utils.clip_grad_norm_(self.policy.parameters(), 0.5)
        self.optimizer.step()
        
        # Log
        self.training_history['policy_loss'].append(policy_loss.item())
        self.training_history['value_loss'].append(value_loss.item())
        self.training_history['entropy_loss'].append(entropy_loss.item())
        self.training_history['total_loss'].append(total_loss.item())
        
        return {
            'policy_loss': policy_loss.item(),
            'value_loss': value_loss.item(),
            'entropy_loss': entropy_loss.item(),
            'total_loss': total_loss.item()
        }
    
    def save_model(self, filepath):
        """Speichere Modell"""
        torch.save({
            'policy_state_dict': self.policy.state_dict(),
            'optimizer_state_dict': self.optimizer.state_dict(),
            'training_history': self.training_history
        }, filepath)
    
    def load_model(self, filepath):
        """Lade Modell"""
        checkpoint = torch.load(filepath)
        self.policy.load_state_dict(checkpoint['policy_state_dict'])
        self.optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        self.training_history = checkpoint.get('training_history', self.training_history)
        print(f"Model loaded from {filepath}")

# ============================================================================
# Data Loading
# ============================================================================

def load_unreal_export(json_file):
    """Lade exportierte Daten von Unreal (reduziertes Logging)"""
    try:
        with open(json_file, 'r', encoding='utf-8') as f:
            content = f.read().strip()
            # Prüfe ob Datei vollständig ist (endet mit } oder ])
            if not content or (not content.endswith('}') and not content.endswith(']')):
                return None  # Kein Logging (reduziert Output)
            
            # Versuche JSON zu parsen
            data = json.loads(content)
    except (json.JSONDecodeError, Exception):
        # Kein Logging für einzelne Fehler (reduziert Output)
        return None
    
    if 'experiences' not in data:
        return None  # Kein Logging
    
    experiences = data['experiences']
    
    if len(experiences) == 0:
        return None  # Kein Logging
    
    obs = []
    actions = []
    rewards = []
    dones = []
    log_probs = []
    values = []
    
    valid_count = 0
    for i, exp in enumerate(experiences):
        try:
            # Prüfe auf NaN/Infinity und ersetze sie
            def sanitize_value(val, default=0.0):
                if isinstance(val, (int, float)):
                    if np.isnan(val) or np.isinf(val):
                        return default
                    return float(val)
                return default
            
            # State
            state = [sanitize_value(v) for v in exp['state']]
            
            # Action
            action_steer = sanitize_value(exp['action']['steer'], 0.0)
            action_throttle = sanitize_value(exp['action']['throttle'], 0.0)
            action_brake = sanitize_value(exp['action']['brake'], 0.0)
            
            # Reward, LogProb, Value
            reward = sanitize_value(exp['reward'], 0.0)
            log_prob = sanitize_value(exp['log_prob'], 0.0)
            value = sanitize_value(exp['value'], 0.0)
            
            obs.append(state)
            actions.append([action_steer, action_throttle, action_brake])
            rewards.append(reward)
            dones.append(exp.get('done', False))
            log_probs.append(log_prob)
            values.append(value)
            valid_count += 1
        except (KeyError, Exception):
            # Überspringe ungültige Experiences ohne Logging (reduziert Output)
            continue
    
    if valid_count == 0:
        # Kein Logging - wird in Zusammenfassung behandelt
        return None
    
    # Kein Logging für jede Datei (reduziert Output bei vielen Rollout-Dateien)
    
    return {
        'obs': np.array(obs, dtype=np.float32),
        'actions': np.array(actions, dtype=np.float32),
        'rewards': np.array(rewards, dtype=np.float32),
        'dones': np.array(dones, dtype=bool),
        'log_probs': np.array(log_probs, dtype=np.float32),
        'values': np.array(values, dtype=np.float32)
    }

# ============================================================================
# Main Training Loop
# ============================================================================

def main():
    # Konfiguration - liest aus Kommandozeilen-Argumenten oder verwendet Standard-Pfade
    import os
    import sys
    
    # Versuche Pfade aus Kommandozeilen-Argumenten zu lesen (für Auto-Training)
    if len(sys.argv) > 1:
        EXPORT_DIR = sys.argv[1]
    else:
        # Versuche Projekt-Pfad aus Script-Pfad abzuleiten
        script_dir = os.path.dirname(os.path.abspath(__file__))
        # Script ist in: Plugins/GameFeatures/CarAI/Content/Python/
        # Projekt-Root ist: ../../../../../
        project_root = os.path.normpath(os.path.join(script_dir, '..', '..', '..', '..', '..'))
        EXPORT_DIR = os.path.join(project_root, 'Saved', 'Training', 'Exports')
    
    if len(sys.argv) > 2:
        MODEL_DIR = sys.argv[2]
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.normpath(os.path.join(script_dir, '..', '..', '..', '..', '..'))
        MODEL_DIR = os.path.join(project_root, 'Saved', 'Training', 'Models')
    
    # Anzahl Epochen (optionales 3. Argument, Standard: 10)
    NUM_EPOCHS = 10
    if len(sys.argv) > 3:
        try:
            NUM_EPOCHS = int(sys.argv[3])
            if NUM_EPOCHS < 1:
                NUM_EPOCHS = 10
        except ValueError:
            NUM_EPOCHS = 10
    
    # Device (optionales 4. Argument: "cpu" oder "cuda", Standard: cpu)
    # GPU ist optional und nur für Tests/Benchmarks empfohlen (CPU ist für kleine Modelle schneller)
    FORCE_DEVICE = None
    if len(sys.argv) > 4:
        device_arg = sys.argv[4].lower()
        if device_arg in ["cpu", "cuda"]:
            FORCE_DEVICE = device_arg
            if device_arg == "cuda" and not torch.cuda.is_available():
                print(f"Warnung: CUDA angefordert, aber nicht verfügbar. Verwende CPU.")
                FORCE_DEVICE = "cpu"
        else:
            print(f"Warnung: Ungültiger Device-Parameter '{sys.argv[4]}', verwende Standard (CPU)")
    
    print(f"EXPORT_DIR: {EXPORT_DIR}")
    print(f"MODEL_DIR: {MODEL_DIR}")
    print(f"NUM_EPOCHS: {NUM_EPOCHS}")
    if FORCE_DEVICE:
        print(f"Device: {FORCE_DEVICE} (explizit angefordert)")
    else:
        print(f"Device: CPU (Standard - optimal für kleine Modelle/Batches)")
    
    # Erstelle Verzeichnisse
    os.makedirs(MODEL_DIR, exist_ok=True)
    
    # Finde alle Export-Dateien
    json_files = glob.glob(os.path.join(EXPORT_DIR, "rollout_*.json"))
    json_files.sort()
    
    if not json_files:
        print(f"Keine Export-Dateien gefunden in {EXPORT_DIR}")
        print("Starte Unreal Training und exportiere Daten!")
        return
    
    print(f"Gefunden: {len(json_files)} Rollout-Dateien")
    
    # Ermittle Observation-Größe aus den ersten gültigen Daten
    OBS_SIZE = None
    for json_file in json_files:
        data = load_unreal_export(json_file)
        if data is not None and len(data['obs']) > 0:
            OBS_SIZE = data['obs'].shape[1]
            print(f"Observation-Größe ermittelt: {OBS_SIZE}")
            break
    
    if OBS_SIZE is None:
        print("Fehler: Konnte Observation-Größe nicht ermitteln!")
        return
    
    # Initialisiere Trainer mit korrekter Observation-Größe
    trainer = PPOTrainer(obs_size=OBS_SIZE, hidden_sizes=[128, 128], device=FORCE_DEVICE)
    print(f"Training auf Device: {trainer.device}")
    
    # Training Loop
    import time
    training_start_time = time.time()
    
    for epoch in range(NUM_EPOCHS):  # Epochen über alle Daten
        epoch_start_time = time.time()
        print(f"\n=== Epoch {epoch + 1}/{NUM_EPOCHS} ===")
        
        all_obs = []
        all_actions = []
        all_rewards = []
        all_dones = []
        all_log_probs = []
        all_values = []
        
        # Lade alle Rollouts (reduziertes Logging)
        loaded_files = 0
        skipped_files = 0
        for json_file in json_files:
            data = load_unreal_export(json_file)
            if data is None:
                skipped_files += 1
                continue  # Überspringe fehlerhafte Dateien
            
            # Prüfe Observation-Größe
            if data['obs'].shape[1] != OBS_SIZE:
                skipped_files += 1
                continue  # Überspringe ohne Warnung (reduziertes Logging)
            
            all_obs.append(data['obs'])
            all_actions.append(data['actions'])
            all_rewards.append(data['rewards'])
            all_dones.append(data['dones'])
            all_log_probs.append(data['log_probs'])
            all_values.append(data['values'])
            loaded_files += 1
        
        # Kurze Zusammenfassung (nur wenn Dateien übersprungen wurden)
        if skipped_files > 0:
            print(f"  Rollouts: {loaded_files} geladen, {skipped_files} übersprungen")
        
        # Prüfe ob Daten vorhanden sind
        if len(all_obs) == 0:
            print("Fehler: Keine gültigen Daten gefunden! Überspringe Epoch.")
            continue
        
        # Konkateniere alle Daten
        obs = np.concatenate(all_obs, axis=0)
        actions = np.concatenate(all_actions, axis=0)
        rewards = np.concatenate(all_rewards, axis=0)
        dones = np.concatenate(all_dones, axis=0)
        log_probs = np.concatenate(all_log_probs, axis=0)
        values = np.concatenate(all_values, axis=0)
        
        # Batch-Größe: Dynamisch basierend auf Device und Datenmenge
        # Bei sehr unterschiedlichen Datensatzgrößen (92KB - 7538KB) brauchen wir adaptive Batch-Größe
        num_samples = len(obs)
        if trainer.device.type == "cuda":
            # GPU: Größere Batches sind effizienter, aber an Datenmenge angepasst
            # Bei wenig Samples: kleinere Batches, bei vielen: größere
            if num_samples < 500:
                batch_size = 32  # Sehr kleine Datensätze
            elif num_samples < 2000:
                batch_size = 64  # Kleine Datensätze
            elif num_samples < 10000:
                batch_size = 128  # Mittlere Datensätze
            else:
                batch_size = min(256, num_samples // 8)  # Große Datensätze (max 256)
        else:
            # CPU: Kleinere Batches sind OK
            batch_size = 64
        
        num_batches = num_samples // batch_size
        if num_batches == 0:
            num_batches = 1  # Mindestens 1 Batch
            batch_size = num_samples  # Verwende alle Samples
        
        print(f"Samples: {num_samples:,} | Batches: {num_batches} | Batch-Size: {batch_size}")
        
        # Mini-Batch Training
        # Sammle Loss-Werte für Zusammenfassung
        epoch_policy_losses = []
        epoch_value_losses = []
        epoch_total_losses = []
        
        # Logging-Intervall: Nur bei 0%, 25%, 50%, 75%, 100%
        log_intervals = sorted(set([0, max(0, int(num_batches * 0.25)), max(0, int(num_batches * 0.5)), 
                                    max(0, int(num_batches * 0.75)), max(0, num_batches - 1)]))
        
        for batch_idx in range(num_batches):
            start_idx = batch_idx * batch_size
            end_idx = start_idx + batch_size
            
            batch_obs = obs[start_idx:end_idx]
            batch_actions = actions[start_idx:end_idx]
            batch_rewards = rewards[start_idx:end_idx]
            batch_dones = dones[start_idx:end_idx]
            batch_log_probs = log_probs[start_idx:end_idx]
            batch_values = values[start_idx:end_idx]
            
            loss = trainer.train_step(
                batch_obs, batch_actions, batch_log_probs,
                batch_rewards, batch_dones, batch_values
            )
            
            # Sammle Loss-Werte
            epoch_policy_losses.append(loss['policy_loss'])
            epoch_value_losses.append(loss['value_loss'])
            epoch_total_losses.append(loss['total_loss'])
            
            # Nur an bestimmten Intervallen loggen
            if batch_idx in log_intervals:
                progress_pct = (batch_idx + 1) / num_batches * 100
                print(f"  [{progress_pct:.0f}%] Policy: {loss['policy_loss']:.4f} | Value: {loss['value_loss']:.4f} | Total: {loss['total_loss']:.4f}")
        
        # Epochen-Zusammenfassung
        epoch_duration = time.time() - epoch_start_time
        avg_policy_loss = np.mean(epoch_policy_losses)
        avg_value_loss = np.mean(epoch_value_losses)
        avg_total_loss = np.mean(epoch_total_losses)
        final_policy_loss = epoch_policy_losses[-1]
        final_value_loss = epoch_value_losses[-1]
        
        print(f"  [OK] Epoch {epoch + 1} abgeschlossen ({epoch_duration:.1f}s)")
        print(f"    Durchschnitt: Policy={avg_policy_loss:.4f} | Value={avg_value_loss:.4f} | Total={avg_total_loss:.4f}")
        print(f"    Final: Policy={final_policy_loss:.4f} | Value={final_value_loss:.4f}")
        
        # Speichere Modell nach jeder Epoche
        model_path = os.path.join(MODEL_DIR, f"model_epoch_{epoch + 1}.pt")
        trainer.save_model(model_path)
    
    # Gesamt-Zusammenfassung
    total_training_time = time.time() - training_start_time
    print(f"\n{'='*60}")
    print(f"Training abgeschlossen!")
    print(f"{'='*60}")
    print(f"Gesamtzeit: {total_training_time/60:.1f} Minuten ({total_training_time:.1f} Sekunden)")
    print(f"Epochen: {NUM_EPOCHS}")
    print(f"Durchschnittliche Zeit pro Epoche: {total_training_time/NUM_EPOCHS:.1f} Sekunden")
    print(f"Modelle gespeichert in: {MODEL_DIR}")
    print(f"{'='*60}")
    
    # Auto-Export: Finde das beste Modell und exportiere es zu JSON für Unreal
    if len(json_files) > 0:  # Nur wenn Training durchgeführt wurde
        try:
            import export_model_for_unreal
            import sys
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            
            # Importiere Funktionen aus find_best_model
            from find_best_model import load_model_checkpoint, extract_epoch_number, calculate_trend, rank_models
            
            # Finde alle trainierten Modelle
            model_pattern = os.path.join(MODEL_DIR, "model_epoch_*.pt")
            model_files = glob.glob(model_pattern)
            model_files.sort(key=lambda x: extract_epoch_number(x) or 0)
            
            if not model_files:
                print(f"\n[!] Warnung: Keine Modelle gefunden zum Exportieren")
                return
            
            # Lade Metriken für alle Modelle
            print(f"\nAuto-Export: Analysiere {len(model_files)} Modelle...")
            metrics_list = []
            for model_file in model_files:
                metrics = load_model_checkpoint(model_file)
                if metrics:
                    metrics_list.append(metrics)
            
            if not metrics_list:
                print(f"[!] Warnung: Keine gültigen Modelle gefunden")
                return
            
            # Rangiere Modelle und finde das beste
            rankings = rank_models(metrics_list)
            
            best_model_path = None
            best_model_epoch = None
            
            if 'combined' in rankings and rankings['combined']:
                # Verwende kombinierten Score (beste Metrik)
                best, score = rankings['combined'][0]
                best_model_path = best['filepath']
                best_model_epoch = best.get('epoch', '?')
                print(f"Bestes Modell gefunden: {best['filename']} (Epoche {best_model_epoch}, Score: {score:.6f})")
            else:
                # Fallback: Verwende letztes Modell
                best_model_path = model_files[-1]
                best_model_epoch = extract_epoch_number(best_model_path)
                print(f"Verwende letztes Modell (Fallback): {os.path.basename(best_model_path)} (Epoche {best_model_epoch})")
            
            # Exportiere das beste Modell
            if best_model_path and os.path.exists(best_model_path):
                best_model_json = best_model_path.replace('.pt', '.json')
                print(f"\nAuto-Export: Konvertiere {os.path.basename(best_model_path)} zu JSON...")
                export_model_for_unreal.export_model_to_json(best_model_path, best_model_json)
                print(f"[OK] Bestes Modell exportiert: {best_model_json} (Epoche {best_model_epoch})")
            else:
                print(f"[!] Warnung: Bestes Modell-Datei nicht gefunden: {best_model_path}")
        except Exception as e:
            print(f"[!] Warnung: Auto-Export fehlgeschlagen: {e}")
            import traceback
            traceback.print_exc()
            print("Du kannst das Modell manuell exportieren mit:")
            print(f"  python export_model_for_unreal.py model_epoch_X.pt model_epoch_X.json")

if __name__ == "__main__":
    main()

