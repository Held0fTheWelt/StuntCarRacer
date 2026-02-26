# Auto-Training - Vollautomatischer Trainingszyklus

Das Auto-Training-System automatisiert den gesamten Trainingszyklus: Daten sammeln → Export → Python-Training → Modell laden → Training fortsetzen.

## 🎯 Übersicht

Mit aktiviertem Auto-Training:
1. ✅ **Daten werden automatisch gesammelt** während des Trainings
2. ✅ **Nach N Rollouts wird automatisch exportiert**
3. ✅ **Python-Training wird automatisch gestartet**
4. ✅ **Trainiertes Modell wird automatisch geladen**
5. ✅ **Training wird automatisch fortgesetzt**

**Alles läuft automatisch im Hintergrund!** Du musst nur Play drücken und zuschauen! 🚀

---

## ⚙️ Konfiguration

### Im Editor Widget (RacingTrainingEditorWidget)

Im **Training Config** Bereich:

1. **`bEnableAutoTraining`**: ✅ **Aktivieren** (Checkbox)
2. **`AutoTrainAfterNRollouts`**: Anzahl Rollouts bis zum automatischen Training (Standard: `10`)
3. **`PythonTrainingScriptPath`**: Pfad zum Training-Script (leer = automatisch `train_pytorch.py`)
4. **`PythonExecutablePath`**: Python-Executable (Standard: `python`)
5. **`AutoLoadModelEpoch`**: Welches Modell laden? (0 = beste Epoche automatisch, >0 = spezifische Epoche)
6. **`bClearExportsAfterTraining`**: Export-Dateien nach Training löschen? (Standard: `false`)
   - `false`: Alle Exporte werden behalten (für kontinuierliches Lernen)
   - `true`: Exporte werden nach jedem Training gelöscht (für frischeres Training)

### Beispiel-Konfiguration

```
bEnableAutoTraining = true
AutoTrainAfterNRollouts = 10
PythonTrainingScriptPath = (leer = automatisch)
PythonExecutablePath = python
AutoLoadModelEpoch = 0  // Beste Epoche automatisch
bClearExportsAfterTraining = false  // Exporte behalten für kontinuierliches Lernen
```

---

## 🚀 Verwendung

### Schritt 1: Auto-Training aktivieren

1. **Editor Widget öffnen:**
   - Content Browser → `EUW_RacingTraining` → Rechtsklick → `Run Editor Utility Widget`

2. **Training Config konfigurieren:**
   - `bEnableAutoTraining` = ✅ **true**
   - `AutoTrainAfterNRollouts` = `10` (oder mehr/weniger)
   - `bExportOnly` = ✅ **true** (wichtig für PyTorch-Training!)

3. **Weitere Einstellungen:**
   - `Agent Pawn Class`: Dein Car Pawn
   - `Num Agents`: 30-50 (für schnellstes Training)

### Schritt 2: Play drücken und Training starten

**Reihenfolge ist wichtig:**

1. **Widget öffnen** (falls noch nicht geöffnet)
2. **Spawn Agents** (Button im Widget - **VOR Play!**)
3. **Play drücken** ⏯️ (PIE starten)
4. **Initialize Training** (Button im Widget - **nach Play!**)
5. **Start Training** (Button im Widget)

**Das war's!** 🎉 Der Rest läuft automatisch.

### Schritt 3: Automatischer Zyklus

**Was passiert automatisch:**

1. **Daten sammeln:**
   - Agents fahren und sammeln Daten
   - Nach jedem Rollout werden Daten gesammelt
   - **Kein manuelles Eingreifen nötig!**

2. **Nach N Rollouts (z.B. 10):**
   - ✅ Training wird automatisch **pausiert**
   - ✅ **Keine neuen Rollouts werden gesammelt** (damit Daten nicht obsolet werden)
   - ✅ Rollouts werden automatisch exportiert
   - ✅ Python-Training wird automatisch gestartet
   - ✅ Beste Epoche wird automatisch gefunden und geladen
   - ✅ Training wird automatisch fortgesetzt

3. **Zyklus wiederholt sich:**
   - Nach weiteren N Rollouts wird der Zyklus wiederholt
   - **Endlos-Training ohne manuelles Eingreifen!**

---

## 📊 Was passiert während Auto-Training?

### Console-Logs zeigen den Fortschritt:

```
RacingTrainingManager: Auto-Training Trigger! 10 neue Rollouts seit letztem Export. Starte Auto-Training-Zyklus...
RacingTrainingManager: === AUTO-TRAINING ZYKLUS STARTET ===
RacingTrainingManager: [1/3] Exportiere 10 Rollouts...
PyTorchExporter: Bulk-Export gestartet - 10 Rollouts werden asynchron exportiert
RacingTrainingManager: [1/3] Export abgeschlossen
RacingTrainingManager: [2/3] Starte Python-Training: train_pytorch.py
PythonTrainingExecutor: Starte Training: python "train_pytorch.py"
PythonTrainingExecutor: Training erfolgreich abgeschlossen (Exit Code: 0)
RacingTrainingManager: [2/3] Python-Training erfolgreich abgeschlossen
RacingTrainingManager: [3/3] Lade trainiertes Modell: Saved/Training/Models/model_epoch_10.json
PyTorchImporter: Successfully loaded model from ...
RacingTrainingManager: [3/3] Modell erfolgreich geladen!
RacingTrainingManager: === AUTO-TRAINING ZYKLUS ABGESCHLOSSEN ===
```

---

## ⚙️ Konfigurations-Optionen

### `AutoTrainAfterNRollouts`

**Empfehlungen:**
- **10 Rollouts**: Schnelles Training, häufige Updates
- **20 Rollouts**: Ausgewogen (empfohlen)
- **30+ Rollouts**: Mehr Daten, selteneres Training

**Bei 30-50 Agents:**
- 10 Rollouts = ~1-2 Minuten Datensammlung
- 20 Rollouts = ~2-3 Minuten Datensammlung

### `AutoLoadModelEpoch`

- **0** (Standard): Lädt das **neueste Modell** (empfohlen)
- **>0**: Lädt eine **spezifische Epoche** (z.B. `10` = `model_epoch_10.json`)

**Empfehlung:** `0` verwenden (neuestes Modell ist meist das beste)

### `PythonExecutablePath`

- **`python`** (Standard): Verwendet Python aus PATH
- **Vollständiger Pfad**: z.B. `C:/Python39/python.exe`

**Tipp:** Falls Python nicht gefunden wird, gib den vollständigen Pfad an.

---

## 🔍 Troubleshooting

### "Python-Training fehlgeschlagen (Exit Code: X)"

**Ursache:** Python-Script hat einen Fehler oder Python ist nicht gefunden

**Lösung:**
1. Prüfe, ob Python installiert ist: `python --version`
2. Prüfe, ob PyTorch installiert ist: `python -c "import torch; print(torch.__version__)"`
3. Prüfe `PythonExecutablePath` (vielleicht vollständigen Pfad angeben)
4. Teste manuell: `python train_pytorch.py`

### "Kein Modell gefunden!"

**Ursache:** Python-Training hat kein Modell erstellt oder Export fehlgeschlagen

**Lösung:**
1. Prüfe `Saved/Training/Models/` - gibt es `.json` Dateien?
2. Prüfe Python-Output (sollte "Modell exportiert" zeigen)
3. Prüfe, ob `export_model_for_unreal.py` verfügbar ist

### "Export fehlgeschlagen"

**Ursache:** Zu wenig Speicherplatz oder Dateisystem-Fehler

**Lösung:**
1. Prüfe verfügbaren Speicherplatz
2. Prüfe `Saved/Training/Exports/` - werden Dateien erstellt?
3. Prüfe Dateiberechtigungen

### "Training pausiert, startet nicht wieder"

**Ursache:** Auto-Training-Zyklus hängt

**Lösung:**
1. Prüfe Console-Logs für Fehlermeldungen
2. Stoppe Training manuell und starte neu
3. Prüfe, ob Python-Prozess läuft (Task Manager)

### "Auto-Training wird nicht getriggert"

**Ursache:** `bEnableAutoTraining` ist nicht aktiviert oder `AutoTrainAfterNRollouts` ist 0

**Lösung:**
1. Prüfe `bEnableAutoTraining` = `true`
2. Prüfe `AutoTrainAfterNRollouts` > 0
3. Prüfe Console-Logs: "Auto-Training aktiv - wird nach X Rollouts getriggert"

---

## 💡 Tipps

### Optimales Setup

**Für schnellstes Training:**
- **30-50 Agents** (wenn Performance erlaubt)
- **AutoTrainAfterNRollouts = 10** (häufige Updates)
- **AutoLoadModelEpoch = 0** (neuestes Modell)

**Für stabiles Training:**
- **10-20 Agents**
- **AutoTrainAfterNRollouts = 20** (mehr Daten pro Zyklus)
- **AutoLoadModelEpoch = 0** (neuestes Modell)

### Performance

**Während Auto-Training:**
- Training wird **pausiert** (Agents fahren nicht)
- Python-Training läuft **im Hintergrund**
- Unreal bleibt **reaktionsfähig** (keine Blockierung)

**Nach Auto-Training:**
- Training wird **automatisch fortgesetzt**
- Agents verwenden **trainiertes Modell**
- **Kein manuelles Eingreifen nötig!**

### Monitoring

**Console-Logs zeigen:**
- Wann Auto-Training getriggert wird
- Fortschritt jedes Schritts (1/3, 2/3, 3/3)
- Erfolg/Fehler jedes Schritts

**Training Widget zeigt:**
- Anzahl gesammelter Rollouts
- Training-Status (Running/Paused)
- Stats (Rewards, Progress, etc.)

---

## 🎯 Workflow-Zusammenfassung

```
1. Widget öffnen
2. bEnableAutoTraining = true setzen (in Training Config)
3. Spawn Agents (Button) ← VOR Play!
4. Play drücken ⏯️
5. Initialize Training (Button) ← WICHTIG: Nach Play!
6. Start Training (Button)
7. ⚡ AUTOMATISCH:
   → Daten sammeln (N Rollouts)
   → Exportieren
   → Python-Training
   → Beste Epoche finden und laden
   → Training fortsetzen
8. Wiederhole Schritt 7 endlos! 🔄
```

**Das war's! Nach Schritt 6 läuft alles automatisch!** 🎉

**Wichtig:** 
- Agents müssen **vor Play** gespawnt werden (im Editor-World)
- "Initialize Training" **muss nach Play** kommen, da die RacingAgentComponent nur während PIE (Play In Editor) verfügbar ist!

---

## 📝 Wichtige Hinweise

1. **`bExportOnly` muss `true` sein** für PyTorch-Training
2. **Python muss im PATH sein** (oder vollständigen Pfad angeben)
3. **PyTorch muss installiert sein** (`pip install torch`)
4. **Export-Module müssen verfügbar sein** (`export_model_for_unreal.py`)
5. **Training wird während Auto-Training pausiert** (normal!)

---

Viel Erfolg mit dem automatisierten Training! 🚗🤖⚡
