"""
Findet das beste trainierte Modell durch Analyse der Loss-Werte und optionaler Evaluation.

Verwendung:
    python find_best_model.py [--evaluate] [--model-dir PATH]
"""

import os
import glob
import torch
import numpy as np
import json
import argparse
from pathlib import Path
from collections import defaultdict

# Importiere PolicyNetwork aus train_pytorch.py
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from train_pytorch import PolicyNetwork, load_unreal_export

# ============================================================================
# Konfiguration
# ============================================================================

DEFAULT_MODEL_DIR = r"C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Models"
DEFAULT_EXPORT_DIR = r"C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Exports"

# ============================================================================
# Modell-Analyse
# ============================================================================

def load_model_checkpoint(filepath):
    """Lade Modell-Checkpoint und extrahiere Metriken"""
    try:
        checkpoint = torch.load(filepath, map_location='cpu')
        
        # Extrahiere Training History
        training_history = checkpoint.get('training_history', {})
        
        # Berechne Metriken
        metrics = {
            'filepath': filepath,
            'filename': os.path.basename(filepath),
            'epoch': extract_epoch_number(filepath),
        }
        
        # Loss-Werte aus Training History
        if training_history:
            policy_losses = training_history.get('policy_loss', [])
            value_losses = training_history.get('value_loss', [])
            total_losses = training_history.get('total_loss', [])
            
            if policy_losses:
                metrics['policy_loss_mean'] = np.mean(policy_losses)
                metrics['policy_loss_final'] = policy_losses[-1] if policy_losses else None
                metrics['policy_loss_min'] = np.min(policy_losses)
                metrics['policy_loss_trend'] = calculate_trend(policy_losses)
            
            if value_losses:
                metrics['value_loss_mean'] = np.mean(value_losses)
                metrics['value_loss_final'] = value_losses[-1] if value_losses else None
                metrics['value_loss_min'] = np.min(value_losses)
                metrics['value_loss_trend'] = calculate_trend(value_losses)
            
            if total_losses:
                metrics['total_loss_mean'] = np.mean(total_losses)
                metrics['total_loss_final'] = total_losses[-1] if total_losses else None
                metrics['total_loss_min'] = np.min(total_losses)
                metrics['total_loss_trend'] = calculate_trend(total_losses)
        
        # Modell-Größe
        if 'policy_state_dict' in checkpoint:
            state_dict = checkpoint['policy_state_dict']
            total_params = sum(p.numel() for p in state_dict.values())
            metrics['num_parameters'] = total_params
        
        return metrics
    
    except Exception as e:
        print(f"Fehler beim Laden von {filepath}: {e}")
        return None

def extract_epoch_number(filepath):
    """Extrahiere Epochen-Nummer aus Dateinamen"""
    filename = os.path.basename(filepath)
    try:
        # Format: model_epoch_X.pt
        parts = filename.replace('.pt', '').split('_')
        if len(parts) >= 3 and parts[0] == 'model' and parts[1] == 'epoch':
            return int(parts[2])
    except:
        pass
    return None

def calculate_trend(values, window=10):
    """Berechne Trend: -1 = sinkend, 0 = stabil, 1 = steigend"""
    if len(values) < window * 2:
        return 0
    
    early_mean = np.mean(values[:window])
    late_mean = np.mean(values[-window:])
    
    if late_mean < early_mean * 0.95:  # >5% Verbesserung
        return -1  # Sinkend (gut!)
    elif late_mean > early_mean * 1.05:  # >5% Verschlechterung
        return 1  # Steigend (schlecht!)
    else:
        return 0  # Stabil

def evaluate_model_on_data(model, test_data, device='cpu'):
    """Evaluiere Modell auf Test-Daten"""
    model.eval()
    
    obs = torch.FloatTensor(test_data['obs']).to(device)
    actions = torch.FloatTensor(test_data['actions']).to(device)
    rewards = test_data['rewards']
    
    total_reward = 0.0
    total_samples = 0
    
    with torch.no_grad():
        # Berechne Value Predictions
        _, _, values = model(obs)
        value_pred = values.cpu().numpy()
        
        # Berechne Log Probs für gegebene Actions
        mean, std, _ = model(obs)
        from torch.distributions import Normal
        dist = Normal(mean, std.clamp(0.01, 1.0))
        log_probs = dist.log_prob(actions).sum(dim=-1).cpu().numpy()
        
        # Metriken
        value_error = np.mean(np.abs(value_pred - test_data['values']))
        log_prob_mean = np.mean(log_probs)
        
        # Reward-basierte Metriken
        reward_mean = np.mean(rewards)
        reward_std = np.std(rewards)
    
    return {
        'value_error': value_error,
        'log_prob_mean': log_prob_mean,
        'reward_mean': reward_mean,
        'reward_std': reward_std,
    }

# ============================================================================
# Ranking und Empfehlungen
# ============================================================================

def rank_models(metrics_list):
    """Rangiere Modelle basierend auf verschiedenen Kriterien"""
    
    # Filtere ungültige Metriken
    valid_metrics = [m for m in metrics_list if m is not None]
    
    if not valid_metrics:
        return []
    
    # Sortiere nach verschiedenen Kriterien
    rankings = {}
    
    # 1. Nach finalem Value Loss (niedriger = besser)
    if any('value_loss_final' in m for m in valid_metrics):
        rankings['value_loss_final'] = sorted(
            valid_metrics,
            key=lambda x: x.get('value_loss_final', float('inf'))
        )
    
    # 2. Nach minimalem Value Loss (niedriger = besser)
    if any('value_loss_min' in m for m in valid_metrics):
        rankings['value_loss_min'] = sorted(
            valid_metrics,
            key=lambda x: x.get('value_loss_min', float('inf'))
        )
    
    # 3. Nach finalem Total Loss (niedriger = besser)
    if any('total_loss_final' in m for m in valid_metrics):
        rankings['total_loss_final'] = sorted(
            valid_metrics,
            key=lambda x: x.get('total_loss_final', float('inf'))
        )
    
    # 4. Nach Trend (sinkend = besser)
    if any('value_loss_trend' in m for m in valid_metrics):
        rankings['trend'] = sorted(
            valid_metrics,
            key=lambda x: (
                x.get('value_loss_trend', 1),  # -1 (sinkend) kommt zuerst
                x.get('value_loss_final', float('inf'))
            )
        )
    
    # 5. Kombinierter Score (gewichteter Durchschnitt)
    combined_scores = []
    for m in valid_metrics:
        score = 0.0
        weight = 0.0
        
        # Value Loss Final (40% Gewicht)
        if 'value_loss_final' in m:
            score += m['value_loss_final'] * 0.4
            weight += 0.4
        
        # Value Loss Min (30% Gewicht)
        if 'value_loss_min' in m:
            score += m['value_loss_min'] * 0.3
            weight += 0.3
        
        # Trend Bonus (30% Gewicht)
        if 'value_loss_trend' in m:
            trend_bonus = 0.0
            if m['value_loss_trend'] == -1:  # Sinkend
                trend_bonus = -0.1  # Bonus
            elif m['value_loss_trend'] == 1:  # Steigend
                trend_bonus = 0.1  # Penalty
            score += trend_bonus * 0.3
            weight += 0.3
        
        if weight > 0:
            combined_scores.append((m, score / weight))
        else:
            combined_scores.append((m, float('inf')))
    
    rankings['combined'] = sorted(combined_scores, key=lambda x: x[1])
    
    return rankings

def print_analysis(metrics_list, rankings, evaluate_results=None):
    """Drucke detaillierte Analyse"""
    
    print("\n" + "="*80)
    print("MODELL-ANALYSE: Beste Generation finden")
    print("="*80)
    
    # Übersicht aller Modelle
    print(f"\n[INFO] Gefundene Modelle: {len(metrics_list)}")
    print("-" * 80)
    
    for i, m in enumerate(metrics_list, 1):
        if m is None:
            continue
        
        epoch = m.get('epoch', '?')
        filename = m.get('filename', '?')
        
        print(f"\n{i}. {filename} (Epoche {epoch})")
        
        if 'value_loss_final' in m:
            print(f"   Value Loss (final): {m['value_loss_final']:.6f}")
        if 'value_loss_min' in m:
            print(f"   Value Loss (min):   {m['value_loss_min']:.6f}")
        if 'value_loss_mean' in m:
            print(f"   Value Loss (mean):  {m['value_loss_mean']:.6f}")
        
        if 'policy_loss_final' in m:
            print(f"   Policy Loss (final): {m['policy_loss_final']:.6f}")
        
        if 'value_loss_trend' in m:
            trend_str = {-1: "[DOWN] Sinkend (gut!)", 0: "[=] Stabil", 1: "[UP] Steigend (schlecht!)"}
            print(f"   Trend: {trend_str.get(m['value_loss_trend'], '?')}")
        
        if evaluate_results and filename in evaluate_results:
            eval_res = evaluate_results[filename]
            print(f"   Evaluation:")
            print(f"     Value Error: {eval_res['value_error']:.6f}")
            print(f"     Reward Mean: {eval_res['reward_mean']:.4f}")
    
    # Rankings
    print("\n" + "="*80)
    print("RANKINGS")
    print("="*80)
    
    # Bestes Modell nach Value Loss Final
    if 'value_loss_final' in rankings and rankings['value_loss_final']:
        best = rankings['value_loss_final'][0]
        print(f"\n[1] Bestes Modell (niedrigster Value Loss Final):")
        print(f"   {best['filename']} (Epoche {best.get('epoch', '?')})")
        print(f"   Value Loss Final: {best.get('value_loss_final', 'N/A'):.6f}")
    
    # Bestes Modell nach Value Loss Min
    if 'value_loss_min' in rankings and rankings['value_loss_min']:
        best = rankings['value_loss_min'][0]
        print(f"\n[2] Bestes Modell (niedrigster Value Loss Min):")
        print(f"   {best['filename']} (Epoche {best.get('epoch', '?')})")
        print(f"   Value Loss Min: {best.get('value_loss_min', 'N/A'):.6f}")
    
    # Bestes Modell nach Trend
    if 'trend' in rankings and rankings['trend']:
        best = rankings['trend'][0]
        print(f"\n[3] Bestes Modell (bester Trend):")
        print(f"   {best['filename']} (Epoche {best.get('epoch', '?')})")
        trend_str = {-1: "Sinkend", 0: "Stabil", 1: "Steigend"}
        print(f"   Trend: {trend_str.get(best.get('value_loss_trend', 0), '?')}")
    
    # Bestes Modell nach kombiniertem Score
    if 'combined' in rankings and rankings['combined']:
        best, score = rankings['combined'][0]
        print(f"\n[*] Bestes Modell (kombinierter Score):")
        print(f"   {best['filename']} (Epoche {best.get('epoch', '?')})")
        print(f"   Score: {score:.6f} (niedriger = besser)")
    
    # Empfehlung
    print("\n" + "="*80)
    print("EMPF推荐UNG")
    print("="*80)
    
    if 'combined' in rankings and rankings['combined']:
        best, score = rankings['combined'][0]
        epoch = best.get('epoch', '?')
        filename = best.get('filename', '?')
        
        print(f"\n[OK] Empfohlenes Modell: {filename}")
        print(f"   Epoche: {epoch}")
        print(f"\nNächste Schritte:")
        print(f"   1. Exportiere Modell zu JSON:")
        print(f"      python export_model_for_unreal.py \"{best['filepath']}\" \"{best['filepath'].replace('.pt', '.json')}\"")
        print(f"   2. Lade Modell in Unreal Editor Widget")
        print(f"   3. Teste das Modell im Spiel")
        
        # Warnung bei Overfitting
        if best.get('value_loss_trend', 0) == 1:
            print(f"\n[!] WARNUNG: Value Loss steigt (Overfitting möglich!)")
            print(f"   Erwäge eine frühere Epoche zu testen.")
    
    print("\n" + "="*80)

# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description='Finde das beste trainierte Modell')
    parser.add_argument('--model-dir', type=str, default=DEFAULT_MODEL_DIR,
                        help='Verzeichnis mit trainierten Modellen')
    parser.add_argument('--export-dir', type=str, default=DEFAULT_EXPORT_DIR,
                        help='Verzeichnis mit Export-Daten für Evaluation')
    parser.add_argument('--evaluate', action='store_true',
                        help='Evaluiere Modelle auf Test-Daten')
    parser.add_argument('--test-split', type=float, default=0.2,
                        help='Anteil der Daten für Test-Set (0.0-1.0)')
    
    args = parser.parse_args()
    
    # Finde alle Modell-Dateien
    model_pattern = os.path.join(args.model_dir, "model_epoch_*.pt")
    model_files = glob.glob(model_pattern)
    model_files.sort(key=lambda x: extract_epoch_number(x) or 0)
    
    if not model_files:
        print(f"[ERROR] Keine Modelle gefunden in {args.model_dir}")
        print(f"   Suche nach: {model_pattern}")
        return
    
    print(f"[INFO] Gefunden: {len(model_files)} Modelle")
    
    # Lade alle Modelle
    print("\n[INFO] Lade Modelle...")
    metrics_list = []
    for model_file in model_files:
        print(f"   Lade {os.path.basename(model_file)}...")
        metrics = load_model_checkpoint(model_file)
        metrics_list.append(metrics)
    
    # Optional: Evaluation auf Test-Daten
    evaluate_results = None
    if args.evaluate:
        print("\n[INFO] Evaluiere Modelle auf Test-Daten...")
        
        # Lade Test-Daten
        export_files = glob.glob(os.path.join(args.export_dir, "rollout_*.json"))
        if not export_files:
            print(f"[!] Keine Export-Dateien gefunden für Evaluation")
        else:
            # Lade Daten
            all_data = []
            for export_file in export_files:
                data = load_unreal_export(export_file)
                if data is not None:
                    all_data.append(data)
            
            if all_data:
                # Split in Train/Test
                split_idx = int(len(all_data) * (1 - args.test_split))
                test_data_list = all_data[split_idx:]
                
                # Konkateniere Test-Daten
                test_obs = np.concatenate([d['obs'] for d in test_data_list], axis=0)
                test_actions = np.concatenate([d['actions'] for d in test_data_list], axis=0)
                test_rewards = np.concatenate([d['rewards'] for d in test_data_list], axis=0)
                test_values = np.concatenate([d['values'] for d in test_data_list], axis=0)
                
                test_data = {
                    'obs': test_obs,
                    'actions': test_actions,
                    'rewards': test_rewards,
                    'values': test_values,
                }
                
                print(f"   Test-Set: {len(test_obs)} Samples")
                
                # Evaluiere jedes Modell
                evaluate_results = {}
                obs_size = test_obs.shape[1]
                
                for metrics in metrics_list:
                    if metrics is None:
                        continue
                    
                    try:
                        # Lade Modell
                        checkpoint = torch.load(metrics['filepath'], map_location='cpu')
                        model = PolicyNetwork(obs_size=obs_size, hidden_sizes=[128, 128])
                        model.load_state_dict(checkpoint['policy_state_dict'])
                        model.eval()
                        
                        # Evaluiere
                        eval_res = evaluate_model_on_data(model, test_data)
                        evaluate_results[metrics['filename']] = eval_res
                        
                        print(f"   [OK] {metrics['filename']}: Value Error = {eval_res['value_error']:.6f}")
                    
                    except Exception as e:
                        print(f"   [ERROR] {metrics['filename']}: Fehler - {e}")
    
    # Rangiere Modelle
    rankings = rank_models(metrics_list)
    
    # Drucke Analyse
    print_analysis(metrics_list, rankings, evaluate_results)
    
    # Speichere Ergebnisse als JSON
    output_file = os.path.join(args.model_dir, "model_analysis.json")
    
    # Konvertiere Rankings zu serialisierbaren Listen
    rankings_serializable = {}
    for k, v in rankings.items():
        if k == 'combined':
            # Combined enthält Tupel (metrics, score)
            rankings_serializable[k] = [
                {'filename': m[0].get('filename', '?'), 'score': m[1]}
                for m in v[:5]
            ]
        else:
            # Andere Rankings enthalten direkt metrics
            rankings_serializable[k] = [
                m.get('filename', '?') for m in v[:5]
            ]
    
    output_data = {
        'models': metrics_list,
        'rankings': rankings_serializable,
        'evaluation': evaluate_results
    }
    
    with open(output_file, 'w') as f:
        json.dump(output_data, f, indent=2, default=str)
    
    print(f"\n[INFO] Detaillierte Analyse gespeichert: {output_file}")

if __name__ == "__main__":
    main()
