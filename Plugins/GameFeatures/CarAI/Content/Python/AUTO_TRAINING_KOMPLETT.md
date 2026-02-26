# Vollständiger Auto-Training-Workflow

## Übersicht

Das Auto-Training-System automatisiert den gesamten Training-Zyklus:
1. **Daten sammeln** in Unreal Engine (während Play Mode)
2. **Export zu JSON** (automatisch nach N Rollouts)
3. **Python-Training** (automatisch gestartet)
4. **Modell-Import** zurück nach Unreal (automatisch)
5. **Fortsetzung** des Trainings mit dem neuen Modell

**Alles läuft automatisch, solange Play Mode aktiv ist!**

## Aktivierung

### Schritt 1: Training Config konfigurieren

Im `RacingTrainingEditorWidget` oder im Blueprint:

1. **Aktiviere Auto-Training:**
   - `bEnableAutoTraining = true`

2. **Konfiguriere Parameter:**
   - `AutoTrainAfterNRollouts = 10` (Starte Training nach 10 Rollouts)
   - `PythonExecutablePath = "C:/Python310/python.exe"` (Pfad zu Python)
   - `PythonTrainingScriptPath = "train_pytorch.py"` (leer = Standard)
   - `AutoLoadModelEpoch = 0` (0 = beste Epoche automatisch, >0 = spezifische Epoche)
   - `bClearExportsAfterTraining = false` (Export-Dateien nach Training löschen? `false` = behalten, `true` = löschen)

3. **Wichtig:** `bExportOnly = true` sollte gesetzt sein (verhindert PPO-Updates in Unreal)

### Schritt 2: Training starten

**Reihenfolge beachten:**

1. Öffne das `RacingTrainingEditorWidget`
2. **Spawn Agents** (Button - **VOR Play!**)
3. **Starte Play Mode** ⏯️
4. **Initialisiere Training** (Button - **nach Play!**)
5. **Starte Training** (Button)

**Das war's!** Der Rest läuft automatisch.

## Wie es funktioniert

### Phase 1: Daten sammeln

- Agents sammeln Erfahrungen während des Trainings
- Nach jedem Rollout (2048 Steps) werden Daten zu `CollectedRollouts` hinzugefügt
- **Kein Export während des Sammelns** (verhindert Blockierung)

### Phase 2: Auto-Training-Trigger

- Nach `AutoTrainAfterNRollouts` Rollouts wird der Auto-Training-Zyklus gestartet
- Training wird automatisch **pausiert** während des Zyklus
- **Wichtig:** Während des Zyklus werden **keine neuen Rollouts gesammelt** (damit Daten nicht obsolet werden)

### Phase 3: Export (Schritt 1/4)

- Alle gesammelten Rollouts werden **asynchron** zu JSON exportiert
- Export-Verzeichnis: `Saved/Training/Exports/`
- Dateien: `rollout_0_YYYYMMDD_HHMMSS.json`, `rollout_1_...`, etc.

### Phase 4: Python-Training (Schritt 2/4)

- Python-Script wird **asynchron** gestartet (blockiert nicht!)
- Script: `train_pytorch.py`
- Pfade werden automatisch als Kommandozeilen-Argumente übergeben
- Training läuft im Hintergrund (10 Epochen standardmäßig)
- Modelle werden gespeichert in: `Saved/Training/Models/model_epoch_1.pt`, etc.
- **Auto-Export:** Letztes Modell wird automatisch zu JSON konvertiert (`model_epoch_10.json`)
- **Best Model Selection:** Beste Epoche wird automatisch ermittelt (basierend auf Loss-Werten und Trends)

### Phase 5: Bestes Modell finden (Schritt 3/4)

- Nach erfolgreichem Training wird automatisch die **beste Epoche** ermittelt
- Analyse basiert auf Value Loss, Trends und kombiniertem Score
- Falls `AutoLoadModelEpoch > 0`: Spezifische Epoche wird geladen (Überschreibt beste Epoche)
- Bestes Modell wird automatisch zu JSON exportiert (falls noch nicht vorhanden)

### Phase 6: Modell-Import (Schritt 4/5)

- Bestes Modell wird automatisch in `PolicyNetwork` geladen
- Training setzt mit dem verbesserten Modell fort

### Phase 7: Fortsetzung (Schritt 5/5)

- Training wird automatisch **fortgesetzt**
- Nächster Auto-Training-Zyklus startet nach weiteren `AutoTrainAfterNRollouts` Rollouts
- **Endlosschleife** solange Play Mode aktiv ist!

## Log-Ausgaben

Während des Auto-Training-Zyklus siehst du:

```
LogTemp: RacingTrainingManager: === AUTO-TRAINING ZYKLUS STARTET ===
LogTemp: RacingTrainingManager: [1/5] Exportiere 10 Rollouts (asynchron)...
LogTemp: PyTorchExporter: Bulk-Export gestartet - 10 Rollouts werden asynchron exportiert
LogTemp: RacingTrainingManager: [1/5] Export-Wartezeit abgeschlossen, starte Python-Training...
LogTemp: RacingTrainingManager: [2/5] Starte Python-Training asynchron: train_pytorch.py
LogTemp: PythonTrainingExecutor: Starte Training asynchron: python "train_pytorch.py" "..."
LogTemp: PythonTrainingExecutor: Training erfolgreich abgeschlossen (Exit Code: 0)
LogTemp: RacingTrainingManager: [2/5] Python-Training erfolgreich abgeschlossen
LogTemp: RacingTrainingManager: find_and_export_best_model.py erfolgreich ausgeführt
LogTemp: RacingTrainingManager: Bestes Modell gefunden: .../model_epoch_8.json
LogTemp: RacingTrainingManager: [3/5] Lade trainiertes Modell: .../model_epoch_8.json
LogTemp: RacingTrainingManager: [3/5] Modell erfolgreich geladen!
LogTemp: RacingTrainingManager: [4/5] === AUTO-TRAINING ZYKLUS ABGESCHLOSSEN ===
LogTemp: RacingTrainingManager: Training wird fortgesetzt. Nächster Auto-Training-Zyklus nach 10 weiteren Rollouts.
```

## Konfiguration

### Training Config Parameter

| Parameter | Beschreibung | Standard |
|-----------|--------------|----------|
| `bEnableAutoTraining` | Aktiviert Auto-Training | `false` |
| `AutoTrainAfterNRollouts` | Anzahl Rollouts vor Training | `10` |
| `PythonExecutablePath` | Pfad zu Python.exe | `"python"` |
| `PythonTrainingScriptPath` | Pfad zum Training-Script | `""` (Standard) |
| `AutoLoadModelEpoch` | Welche Epoche laden (0=neueste) | `0` |
| `bExportOnly` | Nur Export, kein PPO-Update | `true` |

### Python Script Anpassungen

Das `train_pytorch.py` Script:
- Liest Pfade aus Kommandozeilen-Argumenten (automatisch von Unreal übergeben)
- Falls keine Argumente: Verwendet Standard-Pfade relativ zum Script
- Auto-Export: Konvertiert letztes Modell automatisch zu JSON

## Troubleshooting

### Problem: Training startet nicht

**Lösung:**
- Prüfe ob `bEnableAutoTraining = true`
- Prüfe ob `AutoTrainAfterNRollouts > 0`
- Prüfe ob genug Rollouts gesammelt wurden (siehe Log: "Rollout gesammelt")

### Problem: Python-Training schlägt fehl

**Lösung:**
- Prüfe `PythonExecutablePath` (muss gültiger Pfad sein)
- Prüfe ob PyTorch installiert ist: `python -c "import torch"`
- Prüfe Logs für Python-Fehler

### Problem: Modell wird nicht geladen

**Lösung:**
- Prüfe ob `model_epoch_*.json` in `Saved/Training/Models/` existiert
- Prüfe ob `train_pytorch.py` Auto-Export durchführt (siehe Python-Logs)
- Prüfe ob `PyTorchImporter` initialisiert ist

### Problem: Training bleibt pausiert

**Lösung:**
- Prüfe ob Auto-Training-Zyklus abgeschlossen wurde (siehe Logs)
- Prüfe ob `bAutoTrainingInProgress = false` nach Zyklus
- Falls hängengeblieben: Stoppe Training und starte neu

### Problem: Export-Dateien werden nicht erstellt

**Lösung:**
- Prüfe ob `PyTorchExporter` initialisiert ist
- Prüfe ob `CollectedRollouts` > 0 (siehe Log: "Rollout gesammelt")
- Prüfe Schreibrechte für `Saved/Training/Exports/`

## Best Practices

1. **Starte mit wenigen Rollouts** (`AutoTrainAfterNRollouts = 5`) für schnelle Iteration
2. **Erhöhe später** auf 10-20 Rollouts für bessere Modelle
3. **Überwache Logs** während des ersten Zyklus
4. **Prüfe Modell-Qualität** mit `find_best_model.py` zwischen Zyklen
5. **Pausiere manuell** wenn nötig (Training kann während Auto-Training pausiert werden)

### Export-Dateien löschen (`bClearExportsAfterTraining`)

**Wann `true` verwenden:**
- ✅ Wenn du **frischeres Training** möchtest (nur neueste Daten)
- ✅ Wenn Agents sich **verschlechtern** (Overfitting auf alte Daten)
- ✅ Wenn du die **Trainingszeit verkürzen** möchtest (weniger Daten = schnelleres Training)

**Wann `false` verwenden (Standard):**
- ✅ Für **kontinuierliches Lernen** (alle Daten werden verwendet)
- ✅ Wenn Agents sich **stetig verbessern** sollen
- ✅ Für **bessere Generalisierung** (mehr Daten = bessere Modelle)

**Empfehlung:** Starte mit `false` (Standard). Wenn Agents sich nicht verbessern oder verschlechtern, wechsle zu `true`.

## Erweiterte Nutzung

### Manuelles Training zwischen Zyklen

Du kannst jederzeit:
- Training pausieren
- Manuell Python-Training starten
- Modell manuell laden
- Training fortsetzen

### Spezifische Epoche laden

Setze `AutoLoadModelEpoch = 5` um immer Epoche 5 zu laden (statt neueste).

### Mehrere Training-Sessions

- Alte Export-Dateien werden **nicht gelöscht** (für kontinuierliches Lernen)
- Jeder Zyklus verwendet **alle** vorhandenen Export-Dateien
- Für frisches Training: Lösche `Saved/Training/Exports/` manuell

## Zusammenfassung

✅ **Vollständig automatisiert** - Einmal starten, dann läuft es  
✅ **Non-blocking** - Training läuft weiter während Python-Training  
✅ **Kontinuierlich** - Endlosschleife solange Play Mode aktiv  
✅ **Robust** - Fehlerbehandlung und Logging  
✅ **Konfigurierbar** - Alle Parameter anpassbar  

**Viel Erfolg beim Training! 🚗🏎️**
