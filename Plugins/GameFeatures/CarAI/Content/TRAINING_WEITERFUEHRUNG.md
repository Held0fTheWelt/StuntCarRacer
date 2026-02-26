# Training weiterführen: Wichtig zu wissen!

## ⚠️ WICHTIG: Training wird NICHT automatisch weitergeführt!

### Wenn du die Anwendung neu startest:

**Nein, das Training wird NICHT automatisch weitergeführt!**

- Beim Neustart wird ein **NEUES Modell** mit zufälligen Gewichten erstellt
- Vorher trainierte Epochen werden **NICHT automatisch geladen**
- Du musst das Modell **manuell laden** über "Load Checkpoint"

---

## ✅ So funktioniert es richtig:

### 1. **Wenn du pausierst und später weitermachen willst:**

**Während der gleichen Session (Unreal Editor offen):**
- ✅ **Pause Training** → Training pausiert, Modell bleibt geladen
- ✅ **Resume Training** → Training setzt fort mit demselben Modell
- ✅ **Modell bleibt im Speicher** → Keine Probleme

**Zwischen Sessions (Unreal Editor geschlossen/neu gestartet):**
- ❌ **Modell ist NICHT mehr geladen** → Wird nicht automatisch geladen
- ✅ **Du musst das Modell manuell laden:**
  1. Widget öffnen
  2. **Checkpoint Filename:** z.B. `model_epoch_10.json` eingeben
  3. **Load Checkpoint** klicken
  4. Dann **Start Training** klicken

---

## 🔄 Der typische Workflow:

### Session 1: Erstes Training

1. **Daten sammeln:**
   - Agents spawnen
   - Initialize Training
   - Start Training
   - 20+ Rollouts sammeln
   - Stop Training

2. **Python-Training:**
   - `train_pytorch.py` ausführen
   - Modell wird trainiert: `model_epoch_1.pt` bis `model_epoch_10.pt`

3. **Modell zu JSON exportieren:**
   - `export_model_for_unreal.py` ausführen
   - Erstellt: `model_epoch_10.json` (oder das beste Modell)

### Session 2: Weiter-Training (NACH Neustart)

1. **Modell laden:**
   - Widget öffnen
   - **Checkpoint Filename:** `model_epoch_10.json` (oder das beste Modell)
   - **Load Checkpoint** klicken
   - ✅ Status: "Checkpoint loaded: ..."

2. **Weitere Daten sammeln:**
   - Agents spawnen (falls nötig)
   - Initialize Training
   - Start Training
   - **Jetzt verwendet Agent das geladene Modell!**
   - Weitere Rollouts sammeln
   - Stop Training

3. **Python-Training (weiter trainieren):**
   - `train_pytorch.py` ausführen
   - **WICHTIG:** Alte Rollout-Dateien sind noch im Export-Verzeichnis
   - Script trainiert mit **ALLEM** (alten + neuen Rollouts)
   - Neues Modell: `model_epoch_10.pt` (v2)

4. **Neues Modell laden:**
   - Neues Modell zu JSON exportieren
   - In Unreal laden
   - Wiederholen...

---

## ❓ Häufige Fragen:

### Q: Wird das Modell automatisch geladen beim Start Training?

**A: NEIN!** Du musst es **manuell laden** über "Load Checkpoint".

### Q: Werden alte Rollout-Dateien weiterverwendet?

**A: JA!** Wenn du `train_pytorch.py` ausführst, werden **ALLE** Rollout-Dateien im Export-Verzeichnis verwendet:
- Alte Rollouts (vorherige Sessions)
- Neue Rollouts (aktuelle Session)

Das bedeutet: Das Modell wird mit **ALLEM** trainiert, nicht nur mit neuen Daten!

### Q: Soll ich alte Rollout-Dateien löschen?

**A: Das hängt ab:**

**Behalten (empfohlen für Weiter-Training):**
- ✅ Mehr Daten = besseres Training
- ✅ Agent lernt von allen bisherigen Erfahrungen
- ✅ Gut für kontinuierliche Verbesserung

**Löschen (nur wenn nötig):**
- ❌ Wenn du von vorne starten willst
- ❌ Wenn alte Daten "schlecht" sind (Agent hat schlechte Gewohnheiten gelernt)
- ❌ Wenn du einen "frischen Start" willst

**Wie löschen:**
- `Saved/Training/Exports/` Ordner leeren
- Oder: Einzelne Rollout-Dateien löschen

### Q: Wird das Training weitergeführt wenn ich pausiere?

**A: Innerhalb derselben Session:**
- ✅ **Pause Training** → Modell bleibt geladen
- ✅ **Resume Training** → Setzt fort mit demselben Modell
- ✅ Training-Stats (Episodes, Steps, etc.) bleiben erhalten

**Nach Neustart:**
- ❌ **Nichts bleibt erhalten** → Alles wird zurückgesetzt
- ❌ Modell muss manuell geladen werden
- ❌ Stats werden zurückgesetzt

### Q: Wie lade ich das beste Modell automatisch?

**A: Aktuell: Manuell über "Load Checkpoint"**

Du musst das beste Modell manuell identifizieren:
1. Verwende `find_best_model.py` um das beste Modell zu finden
2. Exportiere es zu JSON
3. Lade es in Unreal über "Load Checkpoint"

**Tipp:** Das beste Modell wird jetzt automatisch von `train_pytorch.py` exportiert!

---

## 📝 Checkliste für Weiter-Training:

### Vor dem Weiter-Training:

- [ ] **Modell geladen?** → "Load Checkpoint" geklickt?
- [ ] **Checkpoint Filename** korrekt? → z.B. `model_epoch_10.json`
- [ ] **Status zeigt:** "Checkpoint loaded: ..."?
- [ ] **Agents gespawnt?** → Falls nötig
- [ ] **Training initialisiert?** → "Initialize Training" geklickt

### Während des Trainings:

- [ ] **Training läuft?** → Status: "Training started..."
- [ ] **Rollouts werden gesammelt?** → Prüfe Logs
- [ ] **Agent fährt besser?** → Beobachte im Viewport
- [ ] **Reward steigt?** → Prüfe "Reward MA100" im Widget

### Nach dem Training:

- [ ] **Training gestoppt?** → "Stop Training" geklickt
- [ ] **Rollouts exportiert?** → Prüfe `Saved/Training/Exports/`
- [ ] **Python-Training gestartet?** → `train_pytorch.py` ausführen
- [ ] **Bestes Modell gefunden?** → Wird automatisch von `train_pytorch.py` exportiert
- [ ] **Neues Modell geladen?** → "Load Checkpoint" für nächstes Training

---

## ⚠️ Wichtige Punkte:

1. **Modell wird NICHT automatisch gespeichert:**
   - Nur wenn du "Save Checkpoint" klickst (oder Python-Training ausführst)
   - Beim Neustart ist das Modell weg (außer du lädst es)

2. **Training-Stats werden zurückgesetzt:**
   - Episodes, Steps, etc. starten bei 0
   - Das ist normal - das Modell bleibt aber geladen (wenn du es geladen hast)

3. **Rollout-Dateien bleiben erhalten:**
   - Werden NICHT automatisch gelöscht
   - Python-Training verwendet ALLE Rollout-Dateien
   - Du musst sie manuell löschen, wenn du willst

4. **Weiter-Training funktioniert so:**
   - Alte Rollouts + Neue Rollouts → Python-Training
   - Modell lernt von ALLEM, nicht nur neuen Daten
   - Das ist gut für kontinuierliche Verbesserung!

---

## 🔄 Zusammenfassung:

**Wichtig zu verstehen:**
- ❌ Training wird **NICHT automatisch** weitergeführt
- ❌ Modell wird **NICHT automatisch** geladen
- ✅ Du musst Modell **manuell laden** über "Load Checkpoint"
- ✅ Alte Rollouts werden **automatisch** weiterverwendet (wenn nicht gelöscht)
- ✅ Weiter-Training funktioniert: Alte + Neue Rollouts → Neues Modell

**Typischer Ablauf:**
1. Modell laden (wenn nicht schon geladen)
2. Training starten
3. Rollouts sammeln
4. Python-Training (verwendet ALLE Rollouts)
5. Neues Modell laden
6. Wiederholen...
