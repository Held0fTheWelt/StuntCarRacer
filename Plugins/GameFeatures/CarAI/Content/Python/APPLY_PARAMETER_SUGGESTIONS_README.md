# Apply Parameter Suggestions Widget - Parameter automatisch anwenden

Das `ApplyParameterSuggestionsWidget` lädt die empfohlenen Parameter aus der JSON-Datei und wendet sie automatisch auf alle `RacingAgentComponents` im Level an.

## Verwendung

### Schritt 1: Widget erstellen

1. **Content Browser öffnen:**
   - Navigiere zu: `Plugins/GameFeatures/CarAI/Content/Editor/`
   - Falls der Ordner nicht existiert: Erstelle ihn

2. **Editor Utility Widget erstellen:**
   - Rechtsklick im Content Browser
   - `Editor Utilities` → `Editor Utility Widget`
   - Name: `EUW_ApplyParameterSuggestions`

3. **Parent Class setzen:**
   - Im Blueprint Editor: Klicke auf `Class Settings` (oben)
   - Unter `Parent Class`: Wähle `Apply Parameter Suggestions Widget`
   - Falls nicht sichtbar: Stelle sicher, dass das Projekt kompiliert wurde

4. **Blueprint speichern:**
   - `Ctrl+S` oder `File` → `Save`

### Schritt 2: Parameter analysieren (falls noch nicht gemacht)

**Wichtig:** Du musst zuerst das `ParameterSuggestionWidget` verwenden, um die Parameter zu analysieren!

1. Öffne `EUW_ParameterSuggestion` (Parameter Suggestion Widget)
2. Klicke auf **"Analyze Track And Car"**
3. Klicke auf **"ExportToJSON"**
4. JSON-Datei wird gespeichert: `Saved/Training/ParameterSuggestions.json`

### Schritt 3: Parameter anwenden

1. **Öffne `EUW_ApplyParameterSuggestions`:**
   - Rechtsklick auf `EUW_ApplyParameterSuggestions`
   - `Run Editor Utility Widget`

2. **Konfiguration (optional):**
   - `SuggestionsJSONPath`: Leer lassen für automatische Suche (empfohlen)
   - `bOnlySelectedActors`: `false` = alle Components, `true` = nur ausgewählte
   - `bFindAllInLevel`: `true` = alle im Level finden

3. **Komponenten finden:**
   - Klicke auf **"Find Components"** (CallInEditor Button)
   - Zeigt an, wie viele `RacingAgentComponents` gefunden wurden

4. **Parameter anwenden:**
   - Klicke auf **"Apply Suggestions To Components"** (CallInEditor Button)
   - Oder: Klicke auf **"Load And Apply Suggestions"** (macht beides in einem Schritt)

5. **Ergebnis:**
   - Status zeigt an, wie viele Components aktualisiert wurden
   - Alle Parameter werden automatisch angewendet

## Was wird angewendet?

### Observation Normalization:
- ✅ `TrackHalfWidthCm`
- ✅ `SpeedNormCmPerSec`
- ✅ `CurvatureNormInvCm`
- ✅ `HeadingNormRad`
- ✅ `AngVelNormDegPerSec`
- ⚠️ `LookaheadOffsetsCm` (Array - wird derzeit nicht angewendet)

### Reward Config:
- ✅ `W_Progress`
- ✅ `W_Speed`
- ✅ `W_Lateral`
- ✅ `W_Heading`
- ✅ `W_AngVel`
- ✅ `W_ActionSmooth`
- ✅ `W_Airborne`
- ✅ `ProgressNormCm`
- ✅ `SpeedTargetNorm`
- ✅ `AirborneMaxSeconds`
- ✅ `OffTrackLateralNorm`
- ✅ `WrongWayMinProgressCmPerSec`
- ✅ `StuckSpeedNorm`
- ✅ `StuckTimeSeconds`
- ✅ `TerminalPenalty_AirborneLong`

### Reset Parameters:
- ✅ `LandingGraceSeconds`
- ✅ `StuckMinProgressCmPerSec`

## Beispiel-Workflow

### Kompletter Workflow:

```
1. ParameterSuggestionWidget öffnen
   → "Analyze Track And Car" klicken
   → "ExportToJSON" klicken
   → JSON-Datei wird erstellt: Saved/Training/ParameterSuggestions.json

2. ApplyParameterSuggestionsWidget öffnen
   → "Load And Apply Suggestions" klicken
   → Alle RacingAgentComponents werden automatisch aktualisiert! ✅
```

## Wichtige Hinweise

### JSON-Datei muss existieren

**Voraussetzung:** Die JSON-Datei muss existieren!

- Automatischer Pfad: `Saved/Training/ParameterSuggestions.json`
- Oder: Setze `SuggestionsJSONPath` manuell

**Wenn JSON-Datei fehlt:**
- Status: `"JSON file not found! Run ParameterSuggestionWidget first."`
- Lösung: Führe zuerst `ParameterSuggestionWidget` aus und exportiere zu JSON

### Welche Components werden aktualisiert?

**Standard (bFindAllInLevel = true):**
- Alle `RacingAgentComponents` im Level
- In allen Levels des World

**Nur ausgewählte (bOnlySelectedActors = true):**
- Nur Components von ausgewählten Actors
- Nützlich, um nur bestimmte Components zu aktualisieren

### Undo/Redo Support

- Alle Änderungen werden als **Undo/Redo-Events** markiert
- Du kannst mit `Ctrl+Z` / `Ctrl+Y` Änderungen rückgängig machen

### LookaheadOffsetsCm

**Hinweis:** `LookaheadOffsetsCm` ist ein Array und wird derzeit nicht automatisch angewendet.

**Manuelle Anpassung:**
1. Öffne das Blueprint mit dem `RacingAgentComponent`
2. Setze `LookaheadOffsetsCm` manuell auf die empfohlenen Werte

**Empfohlene Werte** findest du in der JSON-Datei unter `Recommended.LookaheadOffsetsCm`:
```json
"LookaheadOffsetsCm": [200.0, 600.0, 1200.0, 2000.0]
```

## Troubleshooting

### "No RacingAgentComponents found!"

**Ursache:** Keine Components im Level gefunden

**Lösung:**
1. Stelle sicher, dass du im richtigen Level bist
2. Prüfe, ob `RacingAgentComponents` existieren (z.B. in CarPawns)
3. Stelle sicher, dass `bFindAllInLevel = true` ist

### "JSON file not found!"

**Ursache:** JSON-Datei existiert nicht

**Lösung:**
1. Führe zuerst `ParameterSuggestionWidget` aus
2. Klicke auf "ExportToJSON"
3. Oder: Setze `SuggestionsJSONPath` manuell auf den korrekten Pfad

### "No values found in JSON!"

**Ursache:** JSON-Datei hat kein "Recommended" Feld

**Lösung:**
1. Stelle sicher, dass die JSON-Datei vom `ParameterSuggestionWidget` erstellt wurde
2. Prüfe die JSON-Datei manuell (sollte "Recommended" Feld enthalten)

### "Failed to parse JSON"

**Ursache:** JSON-Datei ist beschädigt oder ungültig

**Lösung:**
1. Öffne die JSON-Datei in einem Text-Editor
2. Prüfe auf Syntax-Fehler (z.B. fehlende Klammern)
3. Führe `ParameterSuggestionWidget` erneut aus und exportiere neu

## Integration mit ParameterSuggestionWidget

Das `ApplyParameterSuggestionsWidget` ist die Ergänzung zum `ParameterSuggestionWidget`:

**ParameterSuggestionWidget:**
- ✅ Analysiert Track und Car
- ✅ Berechnet empfohlene Werte
- ✅ Exportiert zu JSON

**ApplyParameterSuggestionsWidget:**
- ✅ Lädt JSON-Datei
- ✅ Findet alle RacingAgentComponents
- ✅ Wendet Werte automatisch an

**Zusammen:** Vollständiger Workflow von Analyse bis Anwendung! 🎉

---

Viel Erfolg! 🚗🤖
