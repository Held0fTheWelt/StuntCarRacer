# Auto-Training Schnellstart 🚀

## In 3 Schritten zum automatisierten Training

### ✅ Schritt 1: Konfiguration (einmalig)

Im **RacingTrainingEditorWidget**:

1. `bEnableAutoTraining` = ✅ **true**
2. `AutoTrainAfterNRollouts` = `10` (oder mehr)
3. `bExportOnly` = ✅ **true** (wichtig!)
4. `AgentPawnClass` = Dein Car Pawn
5. `NumAgents` = 30-50 (für schnelles Training)

### ✅ Schritt 2: Training starten

**Reihenfolge beachten:**

1. 🔲 **Spawn Agents** (Button im Widget - **VOR Play!**)
2. ⏯️ **Play drücken** (im Editor)
3. ⚙️ **Initialize Training** (Button im Widget - **nach Play!**)
4. ▶️ **Start Training** (Button im Widget)

### ✅ Schritt 3: Zuschauen! 👀

**Das war's!** Ab jetzt läuft alles automatisch:

- ✅ Daten werden gesammelt
- ✅ Nach N Rollouts wird automatisch exportiert
- ✅ Python-Training startet automatisch
- ✅ Beste Epoche wird automatisch gefunden und geladen
- ✅ Training wird automatisch fortgesetzt
- ✅ Zyklus wiederholt sich endlos

**Kein manuelles Eingreifen mehr nötig!** 🎉

---

## ⚠️ Wichtige Hinweise

### Reihenfolge ist wichtig!

**Falsch:**
```
Play → Spawn Agents → Initialize Training → Start Training ❌
```

**Richtig:**
```
Spawn Agents → Play → Initialize Training → Start Training ✅
```

**Warum?** 
- Agents müssen **vor Play** gespawnt werden (im Editor-World)
- Die `RacingAgentComponent` wird erst **während PIE** von Game Features hinzugefügt
- Deshalb muss Play **vor** Initialize Training laufen (damit Components verfügbar sind)

### Was passiert wenn Play nicht läuft?

Wenn du "Initialize Training" **ohne Play** klickst, siehst du:
```
ERROR: Play muss laufen! Starte Play (PIE) und klicke dann auf 'Initialize Training'.
```

**Lösung:** Einfach Play drücken und dann nochmal "Initialize Training" klicken.

---

## 🔍 Checkliste

Vor dem ersten Start:

- [ ] Python installiert (`python --version`)
- [ ] PyTorch installiert (`python -c "import torch"`)
- [ ] `bEnableAutoTraining = true` gesetzt
- [ ] `bExportOnly = true` gesetzt
- [ ] `PythonExecutablePath` korrekt (falls Python nicht im PATH)
- [ ] AgentPawnClass gesetzt
- [ ] Track-Spline im Level vorhanden (mit "Track" Tag)

---

## 📊 Was du sehen solltest

Nach "Start Training" solltest du in den Console-Logs sehen:

```
LogTemp: RacingTrainingManager: Training started with X agents
LogTemp: RacingTrainingManager: Auto-Training aktiv - wird nach 10 Rollouts getriggert
```

Während des Trainings:

```
LogTemp: PyTorchExporter: Rollout gesammelt (1234 experiences, 5 Rollouts insgesamt)
```

Nach N Rollouts (Auto-Training-Zyklus):

```
LogTemp: RacingTrainingManager: === AUTO-TRAINING ZYKLUS STARTET ===
LogTemp: RacingTrainingManager: [1/5] Exportiere 10 Rollouts (asynchron)...
LogTemp: RacingTrainingManager: [2/5] Starte Python-Training asynchron: train_pytorch.py
...
LogTemp: RacingTrainingManager: [3/5] Lade trainiertes Modell: .../model_epoch_8.json
LogTemp: RacingTrainingManager: [4/5] === AUTO-TRAINING ZYKLUS ABGESCHLOSSEN ===
```

---

## 💡 Tipps

### Optimales Setup

- **30-50 Agents** für schnellstes Training
- **AutoTrainAfterNRollouts = 10** für häufige Updates
- **AutoLoadModelEpoch = 0** für automatische beste Epoche

### Performance

- Training wird während Auto-Training-Zyklus pausiert (normal!)
- Python-Training läuft im Hintergrund (nicht blockierend)
- Unreal bleibt reaktionsfähig

### Monitoring

- **Console-Logs** zeigen Fortschritt
- **Widget** zeigt Stats (Episoden, Rewards, etc.)
- **CollectedRollouts** Counter zeigt gesammelte Rollouts

---

## ❓ Häufige Fragen

**Q: Muss ich nach jedem Zyklus etwas machen?**  
A: Nein! Der Zyklus wiederholt sich automatisch endlos.

**Q: Wie lange dauert ein Zyklus?**  
A: Abhängig von:
- Anzahl Rollouts (10 = ~1-2 Min Datensammlung)
- Python-Training (~1-5 Minuten für 10 Epochen)
- Gesamt: ~2-7 Minuten pro Zyklus

**Q: Kann ich das Training pausieren?**  
A: Ja, "Pause Training" Button im Widget.

**Q: Wie stoppe ich das Training?**  
A: "Stop Training" Button. Alle gesammelten Rollouts werden dann exportiert.

**Q: Welches Modell wird geladen?**  
A: Standardmäßig die **beste Epoche** (automatisch ermittelt). Falls `AutoLoadModelEpoch > 0`: Spezifische Epoche.

---

**Viel Erfolg! 🚗🤖⚡**
