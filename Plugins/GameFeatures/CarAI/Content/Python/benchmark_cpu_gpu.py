"""
Benchmark-Script: Vergleicht Training-Performance zwischen CPU und GPU

Dieses Script trainiert zweimal mit den gleichen Daten:
1. Einmal mit CPU
2. Einmal mit GPU (falls verfügbar)

Die Gesamtzeiten werden verglichen und ausgegeben.
"""

import os
import sys
import subprocess
import time
import argparse

# Pfade
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.normpath(os.path.join(script_dir, '..', '..', '..', '..', '..'))
EXPORT_DIR = os.path.join(project_root, 'Saved', 'Training', 'Exports')
MODEL_DIR_CPU = os.path.join(project_root, 'Saved', 'Training', 'Models_CPU_Benchmark')
MODEL_DIR_GPU = os.path.join(project_root, 'Saved', 'Training', 'Models_GPU_Benchmark')
TRAIN_SCRIPT = os.path.join(script_dir, 'train_pytorch.py')

def run_training(device, model_dir, num_epochs=3):
    """Führe Training mit gegebenem Device durch"""
    print(f"\n{'='*60}")
    print(f"Training mit {device.upper()}")
    print(f"{'='*60}")
    
    cmd = [
        sys.executable,  # Python-Interpreter
        TRAIN_SCRIPT,
        EXPORT_DIR,
        model_dir,
        str(num_epochs),
        device
    ]
    
    print(f"Befehl: {' '.join(cmd)}")
    print(f"Modell-Verzeichnis: {model_dir}")
    
    start_time = time.time()
    
    try:
        result = subprocess.run(
            cmd,
            cwd=script_dir,
            capture_output=False,  # Zeige Output direkt an
            text=True,
            check=True
        )
        
        elapsed_time = time.time() - start_time
        return elapsed_time, True
        
    except subprocess.CalledProcessError as e:
        elapsed_time = time.time() - start_time
        print(f"\n[ERROR] Training mit {device} fehlgeschlagen: {e}")
        return elapsed_time, False
    
    except KeyboardInterrupt:
        elapsed_time = time.time() - start_time
        print(f"\n[INFO] Training mit {device} abgebrochen")
        return elapsed_time, False

def main():
    parser = argparse.ArgumentParser(description='Benchmark CPU vs GPU Training Performance')
    parser.add_argument('--epochs', type=int, default=3,
                        help='Anzahl Epochen pro Training (Standard: 3)')
    parser.add_argument('--cpu-only', action='store_true',
                        help='Nur CPU-Training durchführen (überspringt GPU)')
    parser.add_argument('--gpu-only', action='store_true',
                        help='Nur GPU-Training durchführen (überspringt CPU)')
    
    args = parser.parse_args()
    
    # Prüfe ob Export-Daten vorhanden sind
    import glob
    json_files = glob.glob(os.path.join(EXPORT_DIR, "rollout_*.json"))
    if not json_files:
        print(f"[ERROR] Keine Export-Dateien gefunden in {EXPORT_DIR}")
        print("Bitte führe zuerst Training in Unreal durch und exportiere Daten!")
        return
    
    print(f"[INFO] Gefunden: {len(json_files)} Rollout-Dateien")
    print(f"[INFO] Epochen pro Training: {args.epochs}")
    
    # Prüfe CUDA-Verfügbarkeit
    import torch
    cuda_available = torch.cuda.is_available()
    
    if cuda_available:
        print(f"[INFO] CUDA verfügbar: {torch.cuda.get_device_name(0)}")
    else:
        print(f"[INFO] CUDA nicht verfügbar")
        if args.gpu_only:
            print(f"[ERROR] --gpu-only angegeben, aber keine GPU verfügbar!")
            return
    
    # Erstelle Modell-Verzeichnisse
    os.makedirs(MODEL_DIR_CPU, exist_ok=True)
    if cuda_available:
        os.makedirs(MODEL_DIR_GPU, exist_ok=True)
    
    results = {}
    
    # GPU Training (zuerst)
    if cuda_available and not args.cpu_only:
        print(f"\n{'#'*60}")
        print(f"# PHASE 1: GPU TRAINING")
        print(f"{'#'*60}")
        gpu_time, gpu_success = run_training("cuda", MODEL_DIR_GPU, args.epochs)
        results['gpu'] = {'time': gpu_time, 'success': gpu_success}
    
    # CPU Training (danach)
    if not args.gpu_only:
        print(f"\n{'#'*60}")
        print(f"# PHASE 2: CPU TRAINING")
        print(f"{'#'*60}")
        cpu_time, cpu_success = run_training("cpu", MODEL_DIR_CPU, args.epochs)
        results['cpu'] = {'time': cpu_time, 'success': cpu_success}
    
    # Zusammenfassung
    print(f"\n{'='*60}")
    print("BENCHMARK-ZUSAMMENFASSUNG")
    print(f"{'='*60}")
    
    if 'cpu' in results:
        cpu_result = results['cpu']
        if cpu_result['success']:
            print(f"\nCPU Training:")
            print(f"  Zeit: {cpu_result['time']:.2f} Sekunden ({cpu_result['time']/60:.2f} Minuten)")
        else:
            print(f"\nCPU Training: FEHLGESCHLAGEN")
    
    if 'gpu' in results:
        gpu_result = results['gpu']
        if gpu_result['success']:
            print(f"\nGPU Training:")
            print(f"  Zeit: {gpu_result['time']:.2f} Sekunden ({gpu_result['time']/60:.2f} Minuten)")
        else:
            print(f"\nGPU Training: FEHLGESCHLAGEN")
    
    # Vergleich
    if 'cpu' in results and 'gpu' in results:
        if results['cpu']['success'] and results['gpu']['success']:
            cpu_time = results['cpu']['time']
            gpu_time = results['gpu']['time']
            speedup = cpu_time / gpu_time
            
            print(f"\n{'='*60}")
            print("VERGLEICH")
            print(f"{'='*60}")
            print(f"CPU Zeit: {cpu_time:.2f}s")
            print(f"GPU Zeit: {gpu_time:.2f}s")
            print(f"Speedup: {speedup:.2f}x ({'GPU' if speedup > 1 else 'CPU'} ist schneller)")
            
            if speedup > 1:
                print(f"\n[OK] GPU ist {speedup:.2f}x schneller als CPU!")
            else:
                print(f"\n[INFO] CPU ist {1/speedup:.2f}x schneller als GPU (ungewöhnlich!)")
    
    print(f"\n{'='*60}")
    print(f"Modell-Verzeichnisse:")
    if 'cpu' in results and results['cpu']['success']:
        print(f"  CPU: {MODEL_DIR_CPU}")
    if 'gpu' in results and results['gpu']['success']:
        print(f"  GPU: {MODEL_DIR_GPU}")
    print(f"{'='*60}")

if __name__ == "__main__":
    main()
