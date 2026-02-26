# Was ist ein Rollout?

## Einfache Erklärung

Ein **Rollout** ist eine **Sammlung von Erfahrungen** (Experiences), die die Agents während des Trainings sammeln, bevor das Netzwerk aktualisiert wird.

## Konkret

### Was passiert in einem Rollout?

1. **Agents sammeln Erfahrungen:**
   - Jeder Agent fährt und sammelt:
     - **Observations** (was der Agent sieht)
     - **Actions** (was der Agent tut)
     - **Rewards** (wie gut die Aktion war)
     - **Values** (Schätzung des zukünftigen Werts)

2. **Nach X Steps:**
   - Standard: Nach **2048 Steps** (kann konfiguriert werden)
   - Alle gesammelten Erfahrungen werden zu einem **Rollout** zusammengefasst

3. **PPO Update:**
   - Das Netzwerk wird mit den Rollout-Daten trainiert
   - Die Gewichte werden aktualisiert
   - Neue Policy wird verwendet

4. **Export:**
   - Der Rollout wird als JSON-Datei exportiert
   - Datei: `rollout_0_YYYYMMDD_HHMMSS.json`

## Beispiel

### Rollout Steps = 2048

```
Step 1: Agent sieht Observation → wählt Action → bekommt Reward
Step 2: Agent sieht Observation → wählt Action → bekommt Reward
Step 3: ...
...
Step 2048: Agent sieht Observation → wählt Action → bekommt Reward

→ Rollout abgeschlossen!
→ PPO Update mit allen 2048 Experiences
→ Export als JSON-Datei
→ Neuer Rollout startet
```

## Warum Rollouts?

### Vorteile:

1. **Stabilität:**
   - PPO braucht viele Erfahrungen für stabile Updates
   - Einzelne Steps sind zu "noisy"

2. **Effizienz:**
   - Batch-Training ist effizienter als Step-by-Step
   - GPU/CPU kann viele Daten parallel verarbeiten

3. **GAE (Generalized Advantage Estimation):**
   - Braucht mehrere Steps, um Advantages zu berechnen
   - Rollout liefert die nötige Sequenz

## Rollout vs Episode

### Rollout:
- **Technische Einheit** für Training
- Feste Anzahl Steps (z.B. 2048)
- Kann mehrere Episoden enthalten
- Wird für PPO Update verwendet

### Episode:
- **Inhaltliche Einheit** (eine "Runde")
- Endet, wenn Agent fertig ist (z.B. von Strecke, Ziel erreicht)
- Variable Länge
- Wird für Reward-Berechnung verwendet

### Beispiel:

```
Rollout 1 (2048 Steps):
  Episode 1: Steps 1-150 (Agent fährt von Strecke)
  Episode 2: Steps 151-300 (Agent fährt von Strecke)
  Episode 3: Steps 301-450 (Agent fährt von Strecke)
  ...
  Episode 10: Steps 1950-2048 (Agent fährt von Strecke)

→ Rollout 1 abgeschlossen
→ PPO Update
→ Rollout 2 startet
```

## Rollout Steps - Was bedeutet das?

### Rollout Steps = 2048 (Standard)

- **Gesamt-Steps** über alle Agents
- Bei 4 Agents: Jeder Agent sammelt ~512 Steps
- Bei 8 Agents: Jeder Agent sammelt ~256 Steps

### Mehr Rollout Steps:

**Vorteile:**
- Mehr Daten pro Update = stabileres Training
- Bessere GAE-Berechnung

**Nachteile:**
- Länger bis zum ersten Update
- Mehr Memory nötig

### Weniger Rollout Steps:

**Vorteile:**
- Schnellere Updates
- Mehr Rollouts in gleicher Zeit

**Nachteile:**
- Weniger stabil
- Mehr Varianz

## Empfehlungen

### Für schnelles Experimentieren:
```
Rollout Steps: 1024
→ Schnellere Updates, aber weniger stabil
```

### Für stabiles Training (Standard):
```
Rollout Steps: 2048
→ Guter Kompromiss
```

### Für sehr stabiles Training:
```
Rollout Steps: 4096
→ Sehr stabil, aber langsam
```

## In der Praxis

### Was du siehst:

1. **Im Widget:**
   - "Total Steps" steigt kontinuierlich
   - Nach 2048 Steps: PPO Update
   - Neue Datei in `Saved/Training/Exports/`

2. **In der Console:**
   ```
   PPO Update: PolicyLoss=0.0234, ValueLoss=0.0123
   PyTorchExporter: Exported 2048 experiences to rollout_0_20241225_143022.json
   ```

3. **Im Export-Verzeichnis:**
   - `rollout_0_20241225_143022.json`
   - `rollout_1_20241225_143105.json`
   - `rollout_2_20241225_143148.json`
   - etc.

## Zusammenfassung

**Rollout = Sammlung von 2048 Steps Erfahrungen, die für ein PPO Update verwendet werden**

- **1 Rollout** = 2048 Steps Erfahrungen
- **Nach jedem Rollout** = PPO Update + Export
- **Mehr Rollouts** = Mehr Daten für PyTorch-Training

## Nächste Schritte

1. **Sammle 5-10 Rollouts** (ca. 10.000-20.000 Steps)
2. **Starte PyTorch-Training** mit den exportierten Rollouts
3. **Importiere trainiertes Modell** zurück nach Unreal
4. **Teste und iteriere**

