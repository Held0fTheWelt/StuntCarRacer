# NVIDIA GPU Optimierungen für PyTorch Training

## NVIDIA Control Panel Einstellungen

### 1. Power Management Mode (WICHTIG!)

**Pfad:** `NVIDIA Control Panel` → `Manage 3D Settings` → `Global Settings` oder `Program Settings`

**Einstellung:** `Power management mode`
- **Empfohlen:** `Prefer maximum performance` (oder `Maximum performance`)
- **Warum:** Verhindert, dass die GPU in einen Energiesparmodus wechselt während des Trainings
- **Standard:** `Adaptive` (kann zu Performance-Drops führen)

### 2. CUDA - GPUs

**Pfad:** `NVIDIA Control Panel` → `Manage 3D Settings` → `Global Settings`

**Einstellung:** `CUDA - GPUs`
- **Empfohlen:** `All` (oder explizit deine RTX 3080 auswählen)
- **Warum:** Stellt sicher, dass CUDA alle verfügbaren GPUs nutzen kann

### 3. Threaded Optimization

**Pfad:** `NVIDIA Control Panel` → `Manage 3D Settings` → `Global Settings`

**Einstellung:** `Threaded optimization`
- **Empfohlen:** `On` (oder `Auto`)
- **Warum:** Erlaubt Multi-Threading für bessere CPU-GPU-Kommunikation

### 4. Texture Filtering - Quality

**Pfad:** `NVIDIA Control Panel` → `Manage 3D Settings` → `Global Settings`

**Einstellung:** `Texture filtering - Quality`
- **Empfohlen:** `High performance` (für Training nicht kritisch, aber kann helfen)
- **Hinweis:** Dies ist hauptsächlich für Gaming, aber kann bei Compute-Workloads auch helfen

## Windows Energieeinstellungen

### 1. Energieplan

**Pfad:** `Windows Einstellungen` → `System` → `Energie & Akku` → `Energieplan`

**Empfohlen:** `Höchstleistung` (oder `High performance`)

**Warum:** Verhindert CPU/GPU-Throttling während des Trainings

### 2. PCI Express Link State Power Management

**Pfad:** `Energieoptionen` → `Erweiterte Energieeinstellungen` → `PCI Express` → `Link State Power Management`

**Empfohlen:** `Aus` (oder `Off`)

**Warum:** Verhindert, dass PCIe in einen Energiesparmodus wechselt (wichtig für GPU-Kommunikation)

## PyTorch-spezifische Optimierungen

### 1. CUDA Memory Management

PyTorch verwendet standardmäßig `cudnn.benchmark = False`. Du kannst es in deinem Script aktivieren:

```python
import torch
if torch.cuda.is_available():
    torch.backends.cudnn.benchmark = True  # Optimiert für konstante Input-Größen
    torch.backends.cudnn.deterministic = False  # Für bessere Performance
```

**Hinweis:** `benchmark = True` ist nur sinnvoll, wenn Input-Größen konstant sind (was bei deinem Training der Fall ist).

### 2. Memory Pinning

Für schnelleren CPU-GPU-Transfer kannst du Memory Pinning verwenden:

```python
# In train_pytorch.py, wenn du DataLoader verwendest
pin_memory=True  # Beschleunigt CPU→GPU Transfer
```

## GPU-Temperatur und Kühlung

### Überwachung

**Tools:**
- `nvidia-smi` (Command Line)
- `GPU-Z` (Windows Tool)
- `MSI Afterburner` (mit Overlay)

**Während Training überwachen:**
```bash
# In PowerShell
nvidia-smi -l 1  # Aktualisiert alle 1 Sekunde
```

**Wichtige Werte:**
- **Temperatur:** Sollte unter 83°C bleiben (Thermal Throttling bei ~83°C)
- **GPU-Auslastung:** Sollte bei 95-100% sein während Training
- **Memory-Auslastung:** Sollte nicht 100% sein (kann zu Out-of-Memory führen)

### Kühlung optimieren

1. **Lüfter-Kurve:** Verwende `MSI Afterburner` oder `EVGA Precision` um Lüfter-Kurve anzupassen
2. **Case-Airflow:** Stelle sicher, dass genug Luft durch dein Gehäuse strömt
3. **GPU-Overclocking:** Nur wenn du weißt, was du tust! Kann zu Instabilität führen

## CUDA Version prüfen

```bash
# Prüfe CUDA Version
nvidia-smi

# Prüfe PyTorch CUDA Version
python -c "import torch; print('PyTorch CUDA:', torch.version.cuda)"
```

**Wichtig:** PyTorch CUDA Version sollte mit NVIDIA Driver kompatibel sein.

## Benchmark nach Optimierungen

Nach den Änderungen solltest du den Benchmark erneut ausführen:

```bash
python benchmark_cpu_gpu.py --epochs 3
```

**Erwartete Verbesserungen:**
- GPU sollte jetzt deutlich schneller sein (2-5x Speedup erwartet)
- Konsistentere Performance (keine Drops durch Power Management)
- Höhere GPU-Auslastung (95-100%)

## Troubleshooting

### GPU wird nicht erkannt

1. Prüfe `nvidia-smi` - wird die GPU angezeigt?
2. Prüfe PyTorch: `python -c "import torch; print(torch.cuda.is_available())"`
3. Treiber aktualisieren: [NVIDIA Driver Download](https://www.nvidia.com/Download/index.aspx)

### Out of Memory (OOM) Fehler

- Reduziere Batch-Größe (z.B. von 256 auf 128)
- Oder reduziere die Anzahl der Samples pro Epoche

### Performance ist immer noch schlecht

1. Prüfe GPU-Auslastung mit `nvidia-smi -l 1`
2. Prüfe ob andere Programme die GPU nutzen
3. Prüfe ob Windows Updates im Hintergrund laufen
4. Prüfe ob Antivirus die GPU blockiert
