# Kompletter Training-Workflow - Alles was du wissen musst

## 📋 Übersicht: Wie funktioniert das Training?

Das Training läuft in **zwei Phasen**, die du abwechselnd wiederholst:

```
┌─────────────────────────────────────────────────────────────┐
│ PHASE 1: Daten sammeln (in Unreal)                          │
│  → Agents fahren mit aktuellem Modell                       │
│  → Observations/Actions/Rewards werden gesammelt            │
│  → Rollouts werden als JSON exportiert                      │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ PHASE 2: Modell trainieren (in Python)                      │
│  → PyTorch trainiert das Modell mit gesammelten Daten       │
│  → Mehrere Epochen werden gespeichert (epoch_1 bis epoch_10)│
│  → Bestes Modell wird als JSON exportiert                   │
└─────────────────────────────────────────────────────────────┘
                         ↓
┌─────────────────────────────────────────────────────────────┐
│ PHASE 3: Modell zurückladen (in Unreal)                     │
│  → Trainiertes Modell wird in Unreal geladen                │
│  → Agents fahren jetzt mit besserem Modell                  │
│  → Bessere Daten werden gesammelt → zurück zu Phase 1       │
└─────────────────────────────────────────────────────────────┘
```

**Wichtig:** Dies ist ein **iterativer Prozess**. Du wiederholst diese Phasen, bis die Agents gut fahren!

---

## 🚀 Phase 1: Daten in Unreal sammeln

### Vorbereitung

1. **Widget öffnen:**
   - Content Browser → `EUW_RacingTraining` → Rechtsklick → `Run Editor Utility Widget`

2. **Konfiguration prüfen:**
   - `bExportOnly = true` (WICHTIG für Datensammlung!)
   - `Agent Pawn Class`: Dein Car Pawn
   - `Num Agents`: 4-10 (mehr = schneller, aber mehr Performance nötig)

3. **RacingAgentComponent Parameter prüfen** (siehe unten)

### Workflow

1. **Play drücken** (während Widget offen ist)
2. **Spawn Agents** (falls noch nicht gespawnt)
3. **Initialize Training** (während Play)
4. **Start Training** (während Play)

### Was passiert?

- Agents fahren mit **aktuellem Modell** (beim ersten Mal: zufällig initialisiert)
- Nach jedem **Rollout** (2048 Steps) werden Daten gesammelt
- Daten werden in `Saved/Training/Exports/rollout_*.json` gespeichert
- **Kein PPO-Training** findet statt (nur Datensammlung!)

### Wann aufhören?

**Empfehlung:**
- **Beim ersten Training:** 10-20 Rollouts sammeln
- **Bei späteren Iterationen:** 5-10 Rollouts reichen (das Modell ist bereits besser)

**Stoppe das Training**, wenn:
- Du genug Rollouts gesammelt hast
- Die Agents zeigen erste Fortschritte (optional, aber ermutigend)

### ⚠️ Wichtige Frage: Alte Exports löschen?

**Kurze Antwort:** Meistens **NEIN** - behalte alte Exports!

**Das Training-Script verwendet ALLE Rollout-Dateien** im Export-Verzeichnis. Du hast zwei Optionen:

#### Option 1: Alle Exports behalten (Empfohlen für kontinuierliches Training)

**Vorteile:**
- ✅ Mehr Daten = stabileres Training
- ✅ Kontinuierliches Lernen aus allen bisherigen Daten
- ✅ Bessere Generalisierung

**Nachteile:**
- ❌ Alte Daten können veraltet sein (wenn sich Reward-Shaping geändert hat)
- ❌ Training wird langsamer (mehr Daten)

**Wann verwenden:**
- ✅ Beim ersten Training (alle Daten behalten)
- ✅ Bei späteren Iterationen (alte + neue Daten)
- ✅ Wenn sich nichts Grundlegendes geändert hat

#### Option 2: Alte Exports löschen (Für "frischen Start")

**Vorteile:**
- ✅ Nur aktuelle Daten (passen zum aktuellen Modell)
- ✅ Schnelleres Training
- ✅ Klarerer Start nach größeren Änderungen

**Nachteile:**
- ❌ Weniger Daten → möglicherweise instabileres Training
- ❌ Verliert bisheriges Wissen

**Wann verwenden:**
- ✅ Wenn du Reward-Shaping **stark** geändert hast
- ✅ Wenn sich die Strecke **stark** geändert hat
- ✅ Wenn Training zu langsam wird (zu viele Dateien)
- ✅ Wenn du einen "frischen Start" testen willst

**Wie alte Exports löschen:**
```powershell
# Alle Rollout-Dateien löschen (VORSICHT!)
Remove-Item "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Exports\rollout_*.json"

# Oder nur alte Dateien (z.B. vor einem bestimmten Datum)
Get-ChildItem "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Exports\rollout_*.json" | 
    Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) } | 
    Remove-Item
```

**Empfehlung:**
- **Beim ersten Training:** Alle Exports behalten
- **Bei späteren Iterationen:** Alte Exports behalten (es sei denn, du hast etwas Grundlegendes geändert)
- **Bei Problemen:** Teste einen "frischen Start" (alte Exports löschen)

---

## 🐍 Phase 2: Python-Training

### Vorbereitung

1. **Prüfe, dass Rollouts existieren:**
   ```
   Saved/Training/Exports/rollout_*.json
   ```

2. **Python-Script starten:**
   ```powershell
   cd "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Plugins\GameFeatures\CarAI\Content\Python"
   python train_pytorch.py
   ```

### Was passiert?

- Script liest alle `rollout_*.json` Dateien
- Trainiert ein PPO-Modell über **10 Epochen** (standardmäßig)
- Speichert nach jeder Epoche: `model_epoch_1.pt`, `model_epoch_2.pt`, ..., `model_epoch_10.pt`
- Dateien werden in `Saved/Training/Models/` gespeichert

### Welche Epoche soll ich wählen?

**Empfehlung für Anfänger:**
- Nimm **immer die letzte Epoche** (`model_epoch_10.pt`)
- Warum? Meistens ist das die am besten trainierte Version

**Für Fortgeschrittene:**
- Schau dir die **Loss-Werte** im Training an:
  - **Value Loss** sollte sinken (z.B. von 1.5 auf 0.5)
  - **Policy Loss** sollte stabil bleiben oder leicht sinken
- Falls Loss-Werte später wieder steigen → **Overfitting**! Nimm eine frühere Epoche (z.B. `epoch_7`)
- Falls Loss-Werte kontinuierlich sinken → Nimm die letzte Epoche (`epoch_10`)

**Wann verschiedene Epochen testen?**
- Wenn das Modell nach dem Laden **schlechter** wird → Teste frühere Epochen
- Wenn das Training **instabil** war → Teste mittlere Epochen (z.B. `epoch_5`)

---

## 📥 Phase 3: Modell zurück in Unreal laden

### Schritt 1: Modell zu JSON exportieren

```powershell
cd "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Plugins\GameFeatures\CarAI\Content\Python"
python export_model_for_unreal.py "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Models\model_epoch_10.pt" "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Saved\Training\Models\model_epoch_10.json"
```

### Schritt 2: In Unreal laden

1. **Widget öffnen** (falls nicht offen)
2. **Checkpoint Filename:** `model_epoch_10.json`
3. **Load Checkpoint** klicken
4. Du solltest sehen: "Checkpoint loaded: ..."

### Was passiert?

- Das trainierte Modell wird in die `PolicyNetwork` geladen
- Agents verwenden jetzt dieses Modell für ihre Actions
- Beim nächsten Training werden **bessere Daten** gesammelt

---

## 🔄 Der komplette Zyklus (Beispiel)

### Iteration 1: Erstes Training

1. **Daten sammeln:** 20 Rollouts (Agents fahren zufällig/schlecht)
2. **Python-Training:** `train_pytorch.py` → `model_epoch_10.pt`
3. **Modell laden:** `model_epoch_10.json` in Unreal
4. **Ergebnis:** Agents fahren etwas besser, aber noch nicht gut

### Iteration 2: Zweites Training

1. **Daten sammeln:** 10 Rollouts (Agents fahren mit Iteration-1-Modell)
2. **Python-Training:** Neue Daten + alte Daten → `model_epoch_10.pt` (v2)
3. **Modell laden:** Neues Modell in Unreal
4. **Ergebnis:** Agents fahren deutlich besser

### Iteration 3, 4, 5...: Weiter verbessern

- Wiederhole den Zyklus
- Mit jedem Zyklus werden die Agents besser
- Irgendwann: Agents können die ganze Strecke fahren!

---

## ⚙️ RacingAgentComponent: Welche Parameter muss ich setzen?

### ❌ NICHT dynamisch (musst du manuell setzen):

Diese Werte sind **statisch** und müssen im Blueprint oder Component gesetzt werden:

#### **Racing|Wiring:**
- **Track Provider:** Muss auf `TrackFrameProviderComponent` zeigen
- **Curriculum Asset:** Optional, für Curriculum Learning

#### **Racing|Obs (Observation-Normalisierung):**
- **Track Half Width Cm:** `500.0` (Standard) - Breite der Strecke
- **Heading Norm Rad:** `0.8` (Standard) - Normalisierung für Heading
- **Speed Norm Cm Per Sec:** `4500.0` (Standard) - Normalisierung für Geschwindigkeit
- **Ang Vel Norm Deg Per Sec:** `220.0` (Standard) - Normalisierung für Winkelgeschwindigkeit
- **Curvature Norm Inv Cm:** `0.0025` (Standard) - Normalisierung für Kurvenradius
- **Lookahead Offsets Cm:** `[200, 600, 1200, 2000]` (Standard) - Lookahead-Punkte

**Wann ändern?**
- Nur wenn deine Strecke **sehr anders** ist (z.B. viel schmaler/breiter)
- Standard-Werte funktionieren für die meisten Fälle

#### **Racing|Reward (Reward-Shaping):**
- **RewardCfg:** Komplette Reward-Konfiguration (siehe unten)

**Wann ändern?**
- Wenn Agents **zu aggressiv** fahren → Erhöhe `W_Lateral` (negativer = mehr Bestrafung für seitliches Abweichen)
- Wenn Agents **zu langsam** fahren → Erhöhe `W_Speed`
- Wenn Agents **vom Track fliegen** → Erhöhe `W_Lateral` und `W_Heading`

#### **Racing|Reset (Auto-Reset):**
- **Stuck Min Progress Cm Per Sec:** `120.0` (Standard) - Wenn Agent langsamer → Reset
- **Stuck Time Seconds:** `1.25` (Standard) - Wie lange "stuck" sein, bevor Reset
- **Landing Grace Seconds:** `0.6` (Standard) - Grace-Period nach Landung
- **Teleport Grace Seconds:** `0.6` (Standard) - Grace-Period nach Teleport

**Wann ändern?**
- Wenn Agents zu früh resettet werden → Erhöhe `Stuck Time Seconds`
- Wenn Agents zu lange "stuck" bleiben → Verringere `Stuck Time Seconds`

### ✅ Dynamisch (werden automatisch angepasst):

Diese Werte werden **vom Training Manager** gesetzt:

- **Actions** (Steer, Throttle, Brake) - Kommen vom Neural Network
- **Observations** - Werden automatisch aus Car-State berechnet
- **Rewards** - Werden automatisch basierend auf `RewardCfg` berechnet

**Du musst diese NICHT setzen!**

---

## 🎯 Reward-Shaping: Die wichtigsten Parameter

Die `RewardCfg` im `RacingAgentComponent` bestimmt, **was** die Agents lernen sollen:

### ⚙️ Wie funktionieren die Werte?

**Wichtig zu verstehen:**
- `W_Lateral` und `W_Heading` sind **negativ** (Bestrafung)
- Die tatsächliche Bestrafung wird **quadriert** berechnet: `W_Lateral * (LateralNorm * LateralNorm)`
- Das bedeutet: **Doppelte Abweichung = 4x Bestrafung** (exponentiell!)
- Daher reichen schon moderate Werte (-0.25 bis -0.4) für die meisten Fälle

**Beispiel:**
- `W_Lateral = -0.25` mit `LateralNorm = 0.5` → Bestrafung = -0.25 * 0.25 = **-0.0625**
- `W_Lateral = -0.25` mit `LateralNorm = 1.0` → Bestrafung = -0.25 * 1.0 = **-0.25** (4x stärker!)

### Standard-Werte (empfohlen für den Start):

```cpp
// Terminal Penalties (wenn Episode endet)
TerminalPenalty_OffTrack = -2.0      // Strafe für "von Strecke fliegen"
TerminalPenalty_WrongWay = -2.0      // Strafe für "falsche Richtung"
TerminalPenalty_Stuck = -2.0         // Strafe für "stecken bleiben"

// Reward Weights (kontinuierliche Belohnungen)
W_Progress = 1.0                     // Belohnung für Fortschritt (HOCH!)
W_Speed = 0.2                        // Belohnung für Geschwindigkeit
W_Lateral = -0.15                    // Bestrafung für seitliches Abweichen
W_Heading = -0.15                    // Bestrafung für falsche Ausrichtung
W_AngVel = -0.05                     // Bestrafung für schnelle Rotation
W_ActionSmooth = -0.02               // Belohnung für sanfte Actions
W_Airborne = -0.05                   // Bestrafung für "in der Luft"
```

### Wenn Agents von der Strecke fliegen:

**Problem:** Agents verlassen die Strecke zu oft

**Wichtig:** Die Werte werden **quadriert** verwendet (`LateralNorm * LateralNorm`), daher steigt die Bestrafung exponentiell mit der Abweichung.

**Schrittweise Anpassung (empfohlen):**

#### Schritt 1: Leichtes Problem (Agents weichen gelegentlich ab)
```cpp
W_Lateral = -0.25                    // Standard: -0.15 → erhöhe auf -0.25
W_Heading = -0.25                    // Standard: -0.15 → erhöhe auf -0.25
TerminalPenalty_OffTrack = -3.0      // Standard: -2.0 → erhöhe auf -3.0
```

#### Schritt 2: Mittleres Problem (Agents fliegen regelmäßig ab)
```cpp
W_Lateral = -0.4                     // Erhöhe auf -0.4
W_Heading = -0.4                     // Erhöhe auf -0.4
TerminalPenalty_OffTrack = -5.0     // Erhöhe auf -5.0
```

#### Schritt 3: Starkes Problem (Agents fliegen sehr oft ab)
```cpp
W_Lateral = -0.6                     // Erhöhe auf -0.6
W_Heading = -0.6                     // Erhöhe auf -0.6
TerminalPenalty_OffTrack = -8.0     // Erhöhe auf -8.0
```

#### Schritt 4: Extremes Problem (Agents schaffen keine Kurve)
```cpp
W_Lateral = -0.8                     // Erhöhe auf -0.8 (Maximum empfohlen)
W_Heading = -0.8                     // Erhöhe auf -0.8 (Maximum empfohlen)
TerminalPenalty_OffTrack = -10.0    // Erhöhe auf -10.0
```

**⚠️ Wichtig:**
- **Nicht zu hoch setzen!** Werte über -1.0 können das Training instabil machen
- **Schrittweise erhöhen:** Starte mit Schritt 1, teste, dann weiter
- **Balance beachten:** Wenn `W_Lateral`/`W_Heading` zu hoch sind, werden Agents zu vorsichtig und fahren zu langsam

### Wenn Agents zu langsam fahren:

**Problem:** Agents fahren zu vorsichtig

**Lösung:**
```cpp
W_Speed = 0.4                        // Erhöhe (mehr Belohnung für Geschwindigkeit)
W_Progress = 1.5                     // Erhöhe (mehr Belohnung für Fortschritt)
```

### Wenn Agents zu aggressiv fahren:

**Problem:** Agents crashen zu oft

**Lösung:**
```cpp
W_ActionSmooth = -0.05               // Erhöhe (negativer = mehr Bestrafung für abrupte Actions)
W_AngVel = -0.1                      // Erhöhe (negativer = mehr Bestrafung für schnelle Rotation)
```

---

## 🚗 Warum schaffen die Autos keine Kurven?

### Mögliche Ursachen:

1. **Zu wenig Training:**
   - Lösung: Mehr Iterationen durchführen (3-5 Zyklen)
   - Mehr Rollouts sammeln (20+ beim ersten Mal)

2. **Reward-Shaping nicht optimal:**
   - Lösung: Passe `W_Lateral` und `W_Heading` an (siehe oben)

3. **Observation-Normalisierung falsch:**
   - Lösung: Prüfe `LookaheadOffsetsCm` - sollten 4 Werte sein: `[200, 600, 1200, 2000]`

4. **Modell noch nicht gut genug:**
   - Lösung: Trainiere länger (mehr Epochen in Python)
   - Sammle mehr Daten

5. **Action Noise zu hoch:**
   - Lösung: Prüfe `ActionNoise` im Training Manager (sollte mit der Zeit sinken)

### Debugging-Tipps:

1. **Aktiviere Debug-Visualisierung:**
   - `RacingAgentComponent` → `bDrawObservationDebug = true`
   - Zeigt Lookahead-Punkte und Observations

2. **Prüfe Rewards:**
   - `RacingAgentComponent` → `bDrawRewardHUD = true`
   - Zeigt aktuelle Rewards während Fahren

3. **Prüfe Console-Logs:**
   - Suche nach "Reward", "OffTrack", "Stuck"
   - Zeigt, warum Episoden enden

---

## 📊 Wann ist das Training gut genug?

### Gute Zeichen:

- ✅ Agents fahren **länger** auf der Strecke
- ✅ **Reward MA100** steigt kontinuierlich
- ✅ **Progress MA100** steigt (Agents kommen weiter)
- ✅ **Weniger "OffTrack" Terminierungen**
- ✅ Agents können **einfache Kurven** fahren

### Schlechte Zeichen:

- ❌ Reward MA100 **stagniert** oder sinkt
- ❌ Agents werden **nicht besser** nach mehreren Iterationen
- ❌ **Value Loss** steigt im Python-Training (Overfitting!)

### Wann stoppen?

- Wenn Agents die **ganze Strecke** schaffen
- Wenn Agents **konsistent gut** fahren (über mehrere Iterationen)
- Wenn du mit dem Ergebnis **zufrieden** bist

---

## 🔧 Häufige Probleme und Lösungen

### Problem: "Agents fahren gar nicht"

**Ursachen:**
- `bEnableDriving = false` im RacingAgentComponent
- Policy Network nicht initialisiert
- Actions werden nicht angewendet

**Lösung:**
- Prüfe `bEnableDriving = true`
- Prüfe, ob Training initialisiert wurde
- Prüfe Console für Fehler

### Problem: "Agents fahren nur geradeaus"

**Ursachen:**
- Action Noise zu niedrig
- Modell noch nicht trainiert (erste Iteration)
- Observations nicht korrekt

**Lösung:**
- Prüfe Action Noise (sollte beim ersten Training hoch sein: 0.5-1.0)
- Warte auf mehr Training-Iterationen
- Aktiviere `bDrawObservationDebug` und prüfe Observations

### Problem: "Training ist zu langsam"

**Ursachen:**
- Zu viele Agents
- Rollout Steps zu hoch
- DeltaTime zu niedrig

**Lösung:**
- Reduziere Anzahl Agents (4-6 statt 10)
- Reduziere Rollout Steps (1024 statt 2048)
- Erhöhe Tick-Rate (falls möglich)

---

## 📝 Checkliste für jeden Trainings-Zyklus

### Vor dem Training:

- [ ] Widget geöffnet und konfiguriert
- [ ] `bExportOnly = true` (für Datensammlung)
- [ ] RacingAgentComponent Parameter geprüft
- [ ] Track Provider gesetzt
- [ ] Reward-Konfiguration angepasst (falls nötig)

### Während des Trainings:

- [ ] Play gestartet
- [ ] Agents gespawnt
- [ ] Training initialisiert
- [ ] Training gestartet
- [ ] Rollouts werden gesammelt (Logs prüfen)

### Nach dem Training:

- [ ] Training gestoppt
- [ ] Rollout-Dateien vorhanden (`Saved/Training/Exports/`)
- [ ] Python-Training gestartet
- [ ] Modell exportiert (`export_model_for_unreal.py`)
- [ ] Modell in Unreal geladen
- [ ] Ergebnis getestet (Play → beobachten)

---

## 🎓 Zusammenfassung: Die wichtigsten Punkte

1. **Training ist iterativ:** Unreal sammelt Daten → Python trainiert → Modell zurück → besser Daten sammeln → ...

2. **Erste Iteration ist am wichtigsten:** Beim ersten Mal fahren Agents zufällig - sammle viele Rollouts (20+)

3. **Nimm die letzte Epoche:** `model_epoch_10.pt` ist meistens das beste Modell

4. **RacingAgentComponent Parameter:**
   - **Reward-Konfiguration** ist wichtig (passe `W_Lateral`, `W_Heading`, `W_Speed` an)
   - **Observation-Normalisierung** meist Standard (nur bei sehr unterschiedlichen Strecken ändern)
   - **Reset-Parameter** können angepasst werden (falls Agents zu früh/spät resettet werden)

5. **Reward-Shaping ist wichtig:**
   - Wenn Agents von Strecke fliegen → Erhöhe `W_Lateral` und `W_Heading` (negativer)
   - Wenn Agents zu langsam → Erhöhe `W_Speed` und `W_Progress`

6. **Geduld:** RL-Training braucht Zeit - mehrere Iterationen sind normal!

---

Viel Erfolg beim Training! 🚗🤖
