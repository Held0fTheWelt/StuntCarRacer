# Racing AI Training - Komplette Dokumentation

**Vollständiger Leitfaden für das Training von Racing AI Agents mit Unreal Engine und PyTorch**

---

## 📚 Inhaltsverzeichnis

1. [Schnellstart](#schnellstart) - Loslegen in 3 Schritten
2. [Auto-Training](#auto-training) - Vollautomatisches Training (Empfohlen)
3. [Manuelles Training](#manuelles-training) - Schritt-für-Schritt Workflow
4. [Parameter-Konfiguration](#parameter-konfiguration) - Agent-Parameter optimieren
5. [Modell-Management](#modell-management) - Modelle analysieren und laden
6. [Konzepte](#konzepte) - Verständnis von Rollouts, Episoden, etc.
7. [Troubleshooting](#troubleshooting) - Häufige Probleme und Lösungen

---

## 🚀 Schnellstart

### Auto-Training (Empfohlen)

**In 3 Schritten zum automatisierten Training:**

1. **Konfiguration:** Im `RacingTrainingEditorWidget`:
   - `bEnableAutoTraining = true`
   - `AutoTrainAfterNRollouts = 10`
   - `bExportOnly = true`
   - `AgentPawnClass` setzen

2. **Start (Reihenfolge beachten!):**
   - Spawn Agents (Button - **VOR Play!**)
   - Play drücken ⏯️
   - Initialize Training (Button - **nach Play!**)
   - Start Training (Button)

3. **Fertig!** Der Rest läuft automatisch:
   - Daten sammeln
   - Export zu JSON
   - Python-Training
   - Bestes Modell laden
   - Training fortsetzen

➡️ **[Detaillierte Auto-Training Anleitung →](AUTO_TRAINING_README.md)**

---

## 🤖 Auto-Training

Das Auto-Training-System automatisiert den gesamten Trainingszyklus:

1. ✅ **Daten sammeln** während des Trainings
2. ✅ **Export zu JSON** nach N Rollouts
3. ✅ **Python-Training** automatisch starten
4. ✅ **Beste Epoche finden** und automatisch laden
5. ✅ **Training fortsetzen** mit verbessertem Modell
6. ✅ **Endlos-Zyklus** solange Play Mode aktiv ist

### Dokumentation

- **[Auto-Training Schnellstart](AUTO_TRAINING_SCHNELLSTART.md)** - Kurzanleitung
- **[Auto-Training README](AUTO_TRAINING_README.md)** - Detaillierte Anleitung
- **[Auto-Training Komplett](AUTO_TRAINING_KOMPLETT.md)** - Vollständige Dokumentation

### Konfiguration

```cpp
bEnableAutoTraining = true
AutoTrainAfterNRollouts = 10
PythonExecutablePath = "python"  // oder vollständiger Pfad
AutoLoadModelEpoch = 0  // 0 = beste Epoche automatisch
bClearExportsAfterTraining = false  // Exporte behalten (true = löschen für frischeres Training)
```

---

## 📖 Manuelles Training

Falls du den Workflow manuell steuern möchtest:

### Workflow

1. **Daten sammeln** in Unreal (Training starten)
2. **Export** zu JSON (beim Stoppen)
3. **Python-Training** manuell starten
4. **Modell analysieren** (`find_best_model.py`)
5. **Modell exportieren** zu JSON (`export_model_for_unreal.py`)
6. **Modell laden** in Unreal
7. **Wiederholen**

➡️ **[Kompletter Training-Workflow →](TRAINING_WORKFLOW_KOMPLETT.md)**

### Schritt-für-Schritt

- **[Schritt-für-Schritt Training](SCHRITT_FUER_SCHRITT_TRAINING.md)** - Detaillierte Anleitung
- **[Training Guide](TRAINING_GUIDE.md)** - Praktische Tipps

---

## ⚙️ Parameter-Konfiguration

### Parameter-Empfehlungen

**Widget-basiert (Empfohlen):**
- **[Parameter Suggestion Widget](PARAMETER_SUGGESTION_WIDGET_README.md)** - Analysiert Track und Car automatisch
- **[Apply Parameter Suggestions Widget](APPLY_PARAMETER_SUGGESTIONS_README.md)** - Wendet Empfehlungen automatisch an

**Python-Script:**
- **[Suggest Parameters Script](SUGGEST_PARAMETERS_README.md)** - Python-basierte Empfehlungen

### Reward-Shaping

Wichtige Parameter im `RacingAgentComponent`:
- `W_Lateral` - Strafe für seitliche Abweichung
- `W_Heading` - Belohnung für korrekte Ausrichtung
- `W_Speed` - Belohnung für Geschwindigkeit
- `W_Progress` - Belohnung für Fortschritt
- `TerminalPenalty_OffTrack` - Strafe für Off-Track

➡️ **[Details im Training-Workflow →](TRAINING_WORKFLOW_KOMPLETT.md#-racingagentcomponent-welche-parameter-muss-ich-setzen)**

---

## 📦 Modell-Management

### Bestes Modell finden

Das Auto-Training-System findet automatisch die beste Epoche. Für manuelle Analyse:

- **[Find Best Model Script](FIND_BEST_MODEL_README.md)** - Analysiert alle trainierten Modelle

```bash
python find_best_model.py --evaluate
```

### Modell importieren

**Auto-Training:** Automatisch

**Manuell:**
1. Bestes Modell identifizieren (`find_best_model.py`)
2. Zu JSON exportieren (`export_model_for_unreal.py`)
3. In Unreal laden (Widget oder Blueprint)

➡️ **[Modell-Import Anleitung →](MODELL_IMPORT_ANLEITUNG.md)** (für manuelles Training)

---

## 🎓 Konzepte

### Was ist ein Rollout?

Ein **Rollout** ist eine Sammlung von 2048 Steps (gesamt über alle Agents). Nach jedem Rollout wird ein PPO-Update durchgeführt (oder Daten exportiert).

➡️ **[Was ist ein Rollout? →](WAS_IST_EIN_ROLLOUT.md)**

### Training-Zeit-Einschätzung

Wie lange dauert es, bis Agents um die Kurven kommen?

➡️ **[Trainingszeit-Schätzung →](TRAINING_ZEIT_SCHAETZUNG.md)**

---

## 🔧 Troubleshooting

### Auto-Training

**Problem:** Auto-Training wird nicht getriggert
- ✅ Prüfe `bEnableAutoTraining = true`
- ✅ Prüfe `AutoTrainAfterNRollouts > 0`
- ✅ Prüfe Console-Logs

**Problem:** Python-Training schlägt fehl
- ✅ Prüfe Python-Installation: `python --version`
- ✅ Prüfe PyTorch: `python -c "import torch"`
- ✅ Prüfe `PythonExecutablePath`

➡️ **[Auto-Training Troubleshooting →](AUTO_TRAINING_README.md#-troubleshooting)**

### Training allgemein

**Problem:** Agents schaffen keine Kurven
- ✅ Erhöhe `W_Lateral` und `W_Heading`
- ✅ Prüfe Track-Parameter (Normierung)
- ✅ Prüfe Reward-Shaping

➡️ **[Training-Workflow Troubleshooting →](TRAINING_WORKFLOW_KOMPLETT.md#-häufige-probleme-und-lösungen)**

---

## 📁 Dokumentations-Struktur

### Hauptdokumente (Aktuell)

- **`README.md`** (Diese Datei) - Übersicht und Index
- **Auto-Training:**
  - `AUTO_TRAINING_SCHNELLSTART.md` - Schnellstart
  - `AUTO_TRAINING_README.md` - Hauptdokumentation
  - `AUTO_TRAINING_KOMPLETT.md` - Vollständige Dokumentation
- **Training:**
  - `TRAINING_WORKFLOW_KOMPLETT.md` - Kompletter Workflow (inkl. Reward-Shaping)
  - `TRAINING_ZEIT_SCHAETZUNG.md` - Zeitschätzungen
- **Parameter:**
  - `PARAMETER_SUGGESTION_WIDGET_README.md` - Widget-Dokumentation
  - `APPLY_PARAMETER_SUGGESTIONS_README.md` - Apply Widget
- **Modelle:**
  - `FIND_BEST_MODEL_README.md` - Bestes Modell finden
- **Konzepte:**
  - `WAS_IST_EIN_ROLLOUT.md` - Rollout-Erklärung

### Veraltete Dokumente (können gelöscht werden)

Diese Dokumente sind durch Auto-Training obsolet geworden:

- ~~`ANLEITUNG_TRAINING.md`~~ → Ersetzt durch Auto-Training-Docs
- ~~`ANLEITUNG_TRAINING_WORKFLOW.md`~~ → Ersetzt durch Auto-Training-Docs
- ~~`ANLEITUNG_EDITOR_WIDGET.md`~~ → Ersetzt durch Auto-Training-Docs
- ~~`WIDGET_PLAY_MODE.md`~~ → In Auto-Training-Docs integriert
- ~~`QUICKSTART.md`~~ → Ersetzt durch Auto-Training Schnellstart
- ~~`README_PYTORCH.md`~~ → Ersetzt durch diese README
- ~~`TRAINING_GUIDE.md`~~ → Ersetzt durch TRAINING_WORKFLOW_KOMPLETT.md
- ~~`SCHRITT_FUER_SCHRITT_TRAINING.md`~~ → Für manuelles Training, aber veraltet
- ~~`MODELL_IMPORT_ANLEITUNG.md`~~ → Auto-Training macht das automatisch
- ~~`SUGGEST_PARAMETERS_README.md`~~ → Ersetzt durch Widget-Docs

---

## 🎯 Empfohlener Workflow

### Für Neueinsteiger

1. ✅ Lies [Auto-Training Schnellstart](AUTO_TRAINING_SCHNELLSTART.md)
2. ✅ Konfiguriere Auto-Training
3. ✅ Starte Training und lass es laufen
4. ✅ Bei Problemen: [Auto-Training README](AUTO_TRAINING_README.md) → Troubleshooting

### Für Fortgeschrittene

1. ✅ Nutze [Parameter Suggestion Widget](PARAMETER_SUGGESTION_WIDGET_README.md) für optimale Einstellungen
2. ✅ Konfiguriere Auto-Training mit passenden Parametern
3. ✅ Überwache Training und passe Reward-Shaping an
4. ✅ Analysiere Modelle mit `find_best_model.py` zwischen Zyklen

---

## 📞 Support

Bei Fragen oder Problemen:

1. Prüfe die entsprechenden Dokumentationsdateien
2. Prüfe Console-Logs für Fehlermeldungen
3. Prüfe Troubleshooting-Abschnitte

---

**Viel Erfolg beim Training! 🚗🤖⚡**
