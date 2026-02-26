# Export-Dateien Limit (MaxExportFiles)

## Übersicht

Mit steigender Anzahl von Export-Dateien wird das Python-Training zunehmend langsamer, da alle Rollout-Dateien geladen und verarbeitet werden müssen. Die Option `MaxExportFiles` ermöglicht es, automatisch Export-Dateien zu löschen und nur die besten/neuesten N Dateien zu behalten.

**Wichtig:** Du kannst wählen, ob Dateien nach **Alter** (ByAge) oder nach **Qualität** (ByQuality) gefiltert werden sollen!

## Konfiguration

### MaxExportFiles (Standard: 0 = kein Limit)

- **0**: Kein Limit - alle Export-Dateien werden behalten und für Training verwendet
- **>0**: Nur die neuesten N Dateien werden behalten, ältere werden automatisch gelöscht

**Wichtig:** Die Funktion `LimitExportFiles()` wird **vor jedem Python-Training** aufgerufen und löscht automatisch die ältesten Dateien, wenn das Maximum überschritten wird.

## Empfehlungen

### Wann `MaxExportFiles = 0` verwenden (kein Limit)?

✅ **Empfohlen für:**
- Erstes Training (alle Daten behalten)
- Wenn Training schnell genug ist (weniger als 50-100 Dateien)
- Wenn du kontinuierliches Lernen möchtest (alle bisherigen Daten)
- Wenn sich nichts Grundlegendes geändert hat (Reward-Shaping, Strecke, etc.)

### Wann `MaxExportFiles > 0` verwenden?

✅ **Empfohlen für:**
- Wenn Training zu langsam wird (mehr als 50-100 Dateien)
- Wenn Speicherplatz begrenzt ist
- Wenn alte Daten veraltet sind (nach großen Änderungen)

### ByAge vs. ByQuality - Welche Filterung ist besser?

#### ByAge (Nach Alter filtern)

✅ **Vorteile:**
- Einfach und schnell (keine Datei-Analyse nötig)
- Behält die neuesten Daten (passen zum aktuellen Modell)
- Geeignet wenn das Modell kontinuierlich besser wird

❌ **Nachteile:**
- Löscht möglicherweise gute alte Daten
- Ignoriert Qualität der Daten

**Empfehlung:** Verwende `ByAge`, wenn du mit dem aktuellen Modell trainierst und alte Daten veraltet sind.

#### ByQuality (Nach Qualität filtern) ⭐ **OFT BESSER!**

✅ **Vorteile:**
- Behält die **besseren** Daten (höhere Rewards)
- Löscht **schlechte** Daten (niedrige Rewards, viele Crashes)
- Kann auch gute alte Daten behalten
- **Oft besseres Training**, da nur hochwertige Daten verwendet werden

❌ **Nachteile:**
- Etwas langsamer (muss Dateien analysieren)
- Benötigt gültige JSON-Dateien

**Empfehlung:** Verwende `ByQuality`, wenn du die **besten** Daten behalten möchtest, unabhängig vom Alter! Das ist oft die bessere Wahl für das Training.

### Praktisches Beispiel

**Szenario:** Du hast 150 Export-Dateien, möchtest aber nur 100 behalten.

**ByAge:**
- Löscht die 50 ältesten Dateien
- Behält die 50 neuesten Dateien
- Problem: Neue Dateien könnten schlecht sein (wenn Modell gerade schlechter wurde)

**ByQuality:**
- Analysiert alle 150 Dateien
- Behält die 100 Dateien mit dem **höchsten durchschnittlichen Reward**
- Vorteil: Auch gute alte Dateien werden behalten!

### Typische Werte

| Anzahl Export-Dateien | Empfohlener MaxExportFiles Wert | Ungefähre Trainingszeit* |
|----------------------|--------------------------------|--------------------------|
| < 50 | 0 (kein Limit) | < 2 Minuten |
| 50-100 | 50-75 | 2-5 Minuten |
| 100-200 | 75-100 | 5-10 Minuten |
| 200+ | 100-150 | 10-30+ Minuten |

*Abhängig von Hardware, Epochen-Anzahl, und Dataset-Größe

### Praktische Beispiele

**Beispiel 1: Erstes Training (kontinuierliches Lernen)**
```
MaxExportFiles = 0  // Alle Daten behalten
bClearExportsAfterTraining = false
```
→ Alle Export-Dateien werden behalten und für Training verwendet

**Beispiel 2: Schnelles Training (nur neueste Daten)**
```
MaxExportFiles = 50  // Nur neueste 50 Dateien
bClearExportsAfterTraining = false
```
→ Nur die neuesten 50 Export-Dateien werden für Training verwendet

**Beispiel 3: Sehr schnelles Training (frischer Start nach jedem Training)**
```
MaxExportFiles = 0  // Keine Begrenzung
bClearExportsAfterTraining = true  // Alle löschen nach Training
```
→ Alle Export-Dateien werden nach jedem Training gelöscht (frischer Start)

## Funktionsweise

1. **Vor jedem Python-Training:**
   - `LimitExportFiles()` wird automatisch aufgerufen
   - Alle Export-Dateien werden nach Änderungsdatum sortiert
   - Wenn `MaxExportFiles > 0` und Anzahl Dateien > MaxExportFiles:
     - Die ältesten Dateien werden gelöscht
     - Nur die neuesten `MaxExportFiles` Dateien bleiben erhalten

2. **Nach Python-Training (optional):**
   - Wenn `bClearExportsAfterTraining = true`:
     - Alle Export-Dateien werden gelöscht (unabhängig von `MaxExportFiles`)

## Vergleich: MaxExportFiles vs. bClearExportsAfterTraining

| Option | Wann ausgeführt | Was wird gelöscht |
|--------|----------------|-------------------|
| `MaxExportFiles > 0` | Vor jedem Training | Älteste Dateien (behält neueste N) |
| `bClearExportsAfterTraining = true` | Nach jedem Training | Alle Dateien (komplett) |

## Kombinationen

### Kombination 1: Kontinuierliches Lernen mit Limit
```
MaxExportFiles = 100
bClearExportsAfterTraining = false
```
→ Behält immer die neuesten 100 Dateien, löscht alte automatisch

### Kombination 2: Frischer Start nach jedem Training
```
MaxExportFiles = 0  // Wird ignoriert wenn bClearExportsAfterTraining = true
bClearExportsAfterTraining = true
```
→ Löscht alle Dateien nach jedem Training (frischer Start)

### Kombination 3: Kein Limit (Standard für kontinuierliches Training)
```
MaxExportFiles = 0
bClearExportsAfterTraining = false
```
→ Alle Dateien werden behalten und verwendet

## Performance-Überlegungen

### Trainingszeit wächst linear mit Anzahl Dateien

- **10 Dateien:** ~30-60 Sekunden
- **50 Dateien:** ~2-5 Minuten
- **100 Dateien:** ~5-10 Minuten
- **200 Dateien:** ~10-20 Minuten
- **500+ Dateien:** ~30+ Minuten (sehr langsam)

### Empfehlung

**Für beste Performance:** Halte `MaxExportFiles` zwischen 50-150, je nach verfügbarer Zeit und Hardware.

**Für kontinuierliches Lernen:** Verwende `MaxExportFiles = 0`, aber beachte, dass Training mit der Zeit langsamer wird.

## Zusammenfassung

- **`MaxExportFiles = 0`**: Kein Limit, alle Dateien behalten (für kontinuierliches Lernen)
- **`MaxExportFiles > 0`**: Automatisches Löschen von Dateien, nur N behalten (für Performance)
- **`ExportFileFilterMode = ByAge`**: Löscht älteste Dateien (behält neueste N)
- **`ExportFileFilterMode = ByQuality`**: Löscht schlechteste Dateien (behält beste N) ⭐ **OFT BESSER!**
- **Standard:** `MaxExportFiles = 0`, `ExportFileFilterMode = ByAge`
- **Empfehlung:** 
  - **Für Performance:** `MaxExportFiles = 50-150`, `ExportFileFilterMode = ByQuality`
  - **Für kontinuierliches Lernen:** `MaxExportFiles = 0`
  - **Für bestes Training:** `MaxExportFiles = 75-100`, `ExportFileFilterMode = ByQuality` ⭐
