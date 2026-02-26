"""
Findet das beste trainierte Modell und exportiert es zu JSON für Unreal.
Wird vom Auto-Training-System verwendet.

Verwendung:
    python find_and_export_best_model.py [MODEL_DIR] [EXPORT_DIR]
    
Ausgabe:
    - Druckt den Pfad zum besten Modell (JSON)
    - Exit Code: 0 = Erfolg, 1 = Fehler
"""

import os
import sys
import glob
import torch
import numpy as np
import json
from pathlib import Path

# Importiere notwendige Funktionen aus find_best_model.py
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# ============================================================================
# Hilfsfunktionen (aus find_best_model.py)
# ============================================================================

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
            
            if value_losses:
                metrics['value_loss_mean'] = np.mean(value_losses)
                metrics['value_loss_final'] = value_losses[-1] if value_losses else None
                metrics['value_loss_min'] = np.min(value_losses)
                metrics['value_loss_trend'] = calculate_trend(value_losses)
        
        return metrics
    
    except Exception as e:
        print(f"Fehler beim Laden von {filepath}: {e}", file=sys.stderr)
        return None

def rank_models(metrics_list):
    """Rangiere Modelle basierend auf kombiniertem Score"""
    
    # Filtere ungültige Metriken
    valid_metrics = [m for m in metrics_list if m is not None and 'value_loss_final' in m]
    
    if not valid_metrics:
        return None
    
    # Kombinierter Score (gewichteter Durchschnitt)
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
    
    # Sortiere nach Score (niedriger = besser)
    combined_scores.sort(key=lambda x: x[1])
    
    if combined_scores:
        return combined_scores[0][0]  # Bestes Modell
    return None

# ============================================================================
# Main
# ============================================================================

def main():
    # Lese Kommandozeilen-Argumente
    if len(sys.argv) > 1:
        model_dir = sys.argv[1]
    else:
        # Standard-Pfad relativ zum Script
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.normpath(os.path.join(script_dir, '..', '..', '..', '..', '..'))
        model_dir = os.path.join(project_root, 'Saved', 'Training', 'Models')
    
    if len(sys.argv) > 2:
        export_dir = sys.argv[2]
    else:
        export_dir = model_dir  # Gleiches Verzeichnis
    
    # Finde alle Modell-Dateien
    model_pattern = os.path.join(model_dir, "model_epoch_*.pt")
    model_files = glob.glob(model_pattern)
    model_files.sort(key=lambda x: extract_epoch_number(x) or 0)
    
    if not model_files:
        print(f"ERROR: Keine Modelle gefunden in {model_dir}", file=sys.stderr)
        sys.exit(1)
    
    # Lade alle Modelle
    metrics_list = []
    for model_file in model_files:
        metrics = load_model_checkpoint(model_file)
        metrics_list.append(metrics)
    
    # Finde bestes Modell
    best_model = rank_models(metrics_list)
    
    if not best_model:
        print("ERROR: Konnte kein gültiges Modell finden", file=sys.stderr)
        sys.exit(1)
    
    best_pt_path = best_model['filepath']
    best_epoch = best_model.get('epoch', '?')
    best_json_path = best_pt_path.replace('.pt', '.json')
    
    print(f"Best model: epoch {best_epoch} ({os.path.basename(best_pt_path)})", file=sys.stderr)
    
    # Prüfe ob JSON bereits existiert
    if os.path.exists(best_json_path):
        # Schreibe Pfad in Datei für Unreal
        output_file = os.path.join(model_dir, "best_model_path.txt")
        with open(output_file, 'w') as f:
            f.write(best_json_path)
        print(best_json_path)  # Auch stdout für Kompatibilität
        sys.exit(0)
    
    # Exportiere zu JSON
    try:
        import export_model_for_unreal
        export_model_for_unreal.export_model_to_json(best_pt_path, best_json_path)
        
        if os.path.exists(best_json_path):
            # Schreibe Pfad in Datei für Unreal
            output_file = os.path.join(model_dir, "best_model_path.txt")
            with open(output_file, 'w') as f:
                f.write(best_json_path)
            print(best_json_path)  # Auch stdout für Kompatibilität
            sys.exit(0)
        else:
            print(f"ERROR: Export fehlgeschlagen: {best_json_path}", file=sys.stderr)
            sys.exit(1)
    
    except Exception as e:
        print(f"ERROR: Fehler beim Export: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
