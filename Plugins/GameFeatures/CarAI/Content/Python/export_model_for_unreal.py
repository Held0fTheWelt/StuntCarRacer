"""
Exportiert PyTorch-Modell in ein Format, das Unreal laden kann
Konvertiert .pt zu JSON mit Gewichten
"""

import torch
import json
import numpy as np
from pathlib import Path

def export_model_to_json(pytorch_model_path, output_json_path):
    """
    Exportiert PyTorch-Modell zu JSON
    
    Args:
        pytorch_model_path: Pfad zur .pt Datei
        output_json_path: Pfad zur Output JSON Datei
    """
    # Lade Modell
    checkpoint = torch.load(pytorch_model_path, map_location='cpu')
    
    if 'policy_state_dict' in checkpoint:
        state_dict = checkpoint['policy_state_dict']
    else:
        state_dict = checkpoint
    
    # Extrahiere Gewichte
    model_data = {
        'policy_layers': [],
        'value_layers': [],
        'policy_head': {},
        'value_head': {},
        'action_log_std': []
    }
    
    # Finde alle Layer
    policy_layers = []
    value_layers = []
    policy_head_weights = None
    policy_head_bias = None
    value_head_weights = None
    value_head_bias = None
    action_log_std = None
    
    # Debug: Zeige alle Keys
    print("Modell-Keys gefunden:")
    for key in sorted(state_dict.keys()):
        print(f"  {key}: {state_dict[key].shape}")
    
    # Extrahiere Shared Layers (nur Linear-Layer, nicht ReLU)
    shared_layer_indices = []
    for key in sorted(state_dict.keys()):
        if 'shared' in key and 'weight' in key:
            # Format: shared.0.weight, shared.2.weight, etc. (ReLU hat keine weights)
            parts = key.split('.')
            if len(parts) >= 2:
                try:
                    layer_idx = int(parts[1])
                    if layer_idx not in shared_layer_indices:
                        shared_layer_indices.append(layer_idx)
                except ValueError:
                    pass
    
    shared_layer_indices.sort()
    print(f"Shared Layer Indices: {shared_layer_indices}")
    
    # Extrahiere Gewichte und Biases für jeden Shared Layer
    for layer_idx in shared_layer_indices:
        weight_key = f'shared.{layer_idx}.weight'
        bias_key = f'shared.{layer_idx}.bias'
        
        if weight_key in state_dict:
            # Flache die Gewichte zu einem 1D-Array (row-major, wie Unreal es erwartet)
            weights_np = state_dict[weight_key].cpu().numpy()
            weights = weights_np.flatten().tolist()  # Flatten zu 1D
            biases = state_dict[bias_key].cpu().numpy().tolist() if bias_key in state_dict else None
            
            policy_layers.append({
                'weights': weights,
                'bias': biases
            })
            print(f"  Layer {layer_idx}: {state_dict[weight_key].shape} -> {len(weights)} weights (flattened)")
    
    # Extrahiere Policy Head
    if 'policy_mean.weight' in state_dict:
        weights_np = state_dict['policy_mean.weight'].cpu().numpy()
        policy_head_weights = weights_np.flatten().tolist()  # Flatten zu 1D
        print(f"  Policy Head Weights: {state_dict['policy_mean.weight'].shape} -> {len(policy_head_weights)} weights (flattened)")
    
    if 'policy_mean.bias' in state_dict:
        policy_head_bias = state_dict['policy_mean.bias'].cpu().numpy().tolist()
        print(f"  Policy Head Bias: {state_dict['policy_mean.bias'].shape}")
    
    # Extrahiere Policy Std (action_log_std)
    if 'policy_std' in state_dict:
        std_tensor = state_dict['policy_std'].cpu().numpy()
        action_log_std = np.log(std_tensor).tolist() if isinstance(std_tensor, np.ndarray) else [np.log(std_tensor)]
        print(f"  Policy Std: {state_dict['policy_std'].shape}")
    
    # Extrahiere Value Head
    if 'value.weight' in state_dict:
        weights_np = state_dict['value.weight'].cpu().numpy()
        value_head_weights = weights_np.flatten().tolist()  # Flatten zu 1D
        print(f"  Value Head Weights: {state_dict['value.weight'].shape} -> {len(value_head_weights)} weights (flattened)")
    
    if 'value.bias' in state_dict:
        value_head_bias = state_dict['value.bias'].cpu().numpy().tolist()
        print(f"  Value Head Bias: {state_dict['value.bias'].shape}")
    
    # Ermittle Input-Größe und Hidden-Layer-Größen
    if not policy_layers:
        print("Fehler: Keine Shared Layers gefunden!")
        return False
    
    # Berechne Input-Größe aus der ersten Layer-Gewichte-Größe
    # Gewichte sind jetzt 1D: InputSize * OutputSize
    if policy_layers[0]['weights']:
        first_layer_weights_count = len(policy_layers[0]['weights'])
        first_layer_output_size = len(policy_layers[0]['bias']) if policy_layers[0]['bias'] else 128
        input_size = first_layer_weights_count // first_layer_output_size
    else:
        input_size = 16
    
    # Hidden Sizes aus Bias-Größen (OutputSize)
    hidden_sizes = [len(layer['bias']) if layer['bias'] else 128 for layer in policy_layers]
    
    print(f"\nErmittelte Netzwerk-Struktur:")
    print(f"  Input Size: {input_size}")
    print(f"  Hidden Sizes: {hidden_sizes}")
    print(f"  Policy Output Size: 3")
    print(f"  Value Output Size: 1")
    
    # Baue JSON-Struktur
    output_data = {
        'network_config': {
            'input_size': input_size,
            'hidden_layers': hidden_sizes,
            'policy_output_size': 3,
            'value_output_size': 1
        },
        'policy_layers': [
            {
                'weights': layer['weights'],
                'biases': layer['bias'] if layer['bias'] else [0.0] * (len(layer['weights']) if layer['weights'] else 128)
            }
            for layer in policy_layers if layer['weights']
        ],
        'value_layers': [
            {
                'weights': layer['weights'],
                'biases': layer['bias'] if layer['bias'] else [0.0] * (len(layer['weights']) if layer['weights'] else 128)
            }
            for layer in policy_layers if layer['weights']  # Value teilt sich die Shared Layers
        ],
        'policy_head': {
            'weights': policy_head_weights or [],
            'biases': policy_head_bias or []
        },
        'value_head': {
            'weights': value_head_weights or [],
            'biases': value_head_bias or []
        },
        'action_log_std': action_log_std or [0.5, 0.5, 0.5]
    }
    
    # Speichere JSON
    with open(output_json_path, 'w') as f:
        json.dump(output_data, f, indent=2)
    
    print(f"Modell exportiert nach: {output_json_path}")
    return True

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) < 3:
        print("Usage: python export_model_for_unreal.py <pytorch_model.pt> <output.json>")
        sys.exit(1)
    
    pytorch_path = sys.argv[1]
    output_path = sys.argv[2]
    
    export_model_to_json(pytorch_path, output_path)

