# Parameter Suggestion Widget - Automatische Parameter-Empfehlungen

Das `ParameterSuggestionWidget` analysiert automatisch deine Spline und CarPawn-Parameter und schlägt passende Werte für `RacingAgentComponent` vor.

## Verwendung

### Schritt 1: Widget erstellen

1. **Content Browser öffnen:**
   - Navigiere zu: `Plugins/GameFeatures/CarAI/Content/Editor/`
   - Falls der Ordner nicht existiert: Erstelle ihn

2. **Editor Utility Widget erstellen:**
   - Rechtsklick im Content Browser
   - `Editor Utilities` → `Editor Utility Widget`
   - Name: `EUW_ParameterSuggestion`

3. **Parent Class setzen:**
   - Im Blueprint Editor: Klicke auf `Class Settings` (oben)
   - Unter `Parent Class`: Wähle `Parameter Suggestion Widget`
   - Falls nicht sichtbar: Stelle sicher, dass das Projekt kompiliert wurde

4. **Blueprint speichern:**
   - `Ctrl+S` oder `File` → `Save`

### Schritt 2: Widget öffnen

1. **Content Browser:**
   - Rechtsklick auf `EUW_ParameterSuggestion`
   - `Run Editor Utility Widget`

2. **Oder über Menü:**
   - `Window` → `Parameter Suggestion` (falls registriert)

### Schritt 3: Konfiguration

Im Widget-Details Panel:

1. **CarPawnClass setzen:**
   - Wähle deinen CarPawn (z.B. `BP_CarPawn`)
   - Falls nicht gesetzt: Wird versucht, aus AgentPawnClass zu lesen

2. **Sample Counts anpassen (optional):**
   - `SplineSampleCount`: 100 (Standard) - Mehr = genauere Analyse, aber langsamer
   - `TrackWidthSampleCount`: 10 (Standard) - Mehr = genauere Breiten-Messung

### Schritt 4: Analyse starten

1. **Klicke auf "Analyze Track And Car"** (CallInEditor Button)
2. Das Widget:
   - Findet die Spline im Level (mit Tag "Track")
   - Analysiert Streckenlänge, Kurvenradien
   - Misst Streckenbreite (über Line Traces)
   - Analysiert CarPawn-Parameter
   - Berechnet empfohlene Werte

3. **Ergebnisse anzeigen:**
   - Alle Werte werden im Widget angezeigt
   - Zusätzlich in der Console (Output Log)

### Schritt 5: Export (optional)

1. **ExportToJSON:**
   - Speichert Analyse-Ergebnisse als JSON
   - Pfad: `Saved/Training/ParameterSuggestions.json`

2. **ExportToPythonScript:**
   - Erstellt ein Python-Script zum Anwenden der Werte
   - Pfad: `Saved/Training/apply_suggestions.py`

## Was wird analysiert?

### Track-Analyse

1. **Streckenlänge:**
   - Wird direkt aus Spline gelesen: `Spline->GetSplineLength()`

2. **Kurvenradien:**
   - Analysiert Spline an mehreren Punkten
   - Berechnet Krümmung über Tangenten-Vergleich
   - Findet minimalen und durchschnittlichen Radius

3. **Streckenbreite:**
   - Misst Breite an mehreren Punkten entlang der Spline
   - Verwendet Line Traces nach links/rechts
   - Berechnet Durchschnittsbreite

### Car-Analyse

1. **Max. Geschwindigkeit:**
   - Versucht, aus `ChaosWheeledVehicleMovementComponent` zu lesen
   - Falls nicht verfügbar: Verwendet Standard-Werte (45 m/s)

2. **Winkelgeschwindigkeit:**
   - Berechnet basierend auf Geschwindigkeit und Kurvenradius

## Empfohlene Werte

Das Widget berechnet automatisch:

### Observation-Normalisierung

- **TrackHalfWidthCm:** `Streckenbreite / 2`
- **SpeedNormCmPerSec:** `MaxSpeed * 1.3`
- **CurvatureNormInvCm:** `1 / (4 * min_radius)`
- **HeadingNormRad:** Basierend auf Kurvenradien (0.6-1.0)
- **AngVelNormDegPerSec:** `(Speed / Radius) * 1.5`
- **LookaheadOffsetsCm:** 2%, 6%, 12%, 20% der Streckenlänge

### Reward Weights

Basierend auf Streckenschwierigkeit:
- **Einfach** (Radius > 50m): W_Lateral = -0.10, W_Heading = -0.10
- **Standard** (Radius 10-50m): W_Lateral = -0.15, W_Heading = -0.15
- **Schwierig** (Radius 5-10m): W_Lateral = -0.25, W_Heading = -0.25
- **Sehr schwierig** (Radius < 5m): W_Lateral = -0.4, W_Heading = -0.4

## Beispiel-Ausgabe

```
========================================
PARAMETER SUGGESTION ANALYSIS
========================================
Track Length: 500.0 m
Track Width: 1000.0 cm
Min Curvature Radius: 5.0 m
Max Speed: 162.0 km/h

RECOMMENDED VALUES:
TrackHalfWidthCm = 500.0
SpeedNormCmPerSec = 1805.6
CurvatureNormInvCm = 0.000500
HeadingNormRad = 1.00
AngVelNormDegPerSec = 238.7
LookaheadOffsetsCm = [1000, 3000, 6000, 10000]
W_Lateral = -0.25
W_Heading = -0.25
W_Speed = 0.15
W_Progress = 1.00
========================================
```

## Wichtige Hinweise

### Streckenbreite-Messung

Die Streckenbreite wird über **Line Traces** gemessen:
- Trace nach links und rechts von der Spline
- Findet die erste Kollision (Track-Begrenzung)
- **Wichtig:** Stelle sicher, dass deine Strecke Kollisions-Geometrie hat!

**Falls Breite nicht gemessen werden kann:**
- Widget verwendet Standard-Wert (1000cm = 10m)
- Du kannst die Breite manuell im Widget setzen

### CarPawn-Parameter

**Max. Geschwindigkeit:**
- Widget versucht, aus `ChaosWheeledVehicleMovementComponent` zu lesen
- Falls nicht verfügbar: Verwendet Standard (45 m/s = 162 km/h)
- **Tipp:** Du kannst die Werte manuell im Python-Script anpassen

**Alternative:** Teste dein Auto im Spiel und passe die Werte manuell an

### Spline finden

Das Widget sucht nach:
1. Actor mit Tag **"Track"**
2. SplineComponent mit Name **"TrackSpline"** (oder erste Spline)

**Falls Spline nicht gefunden wird:**
- Füge Tag "Track" zu deinem Track-Actor hinzu
- Oder benenne SplineComponent zu "TrackSpline" um

## Troubleshooting

### "No track spline found!"

**Ursache:** Keine Spline mit Tag "Track" gefunden

**Lösung:**
1. Prüfe, ob dein Track-Actor den Tag "Track" hat
2. Prüfe, ob eine SplineComponent vorhanden ist
3. Verwende `DebugListAllActors` im CurriculumEditorWidget, um alle Splines zu sehen

### "Track Width = 0"

**Ursache:** Line Traces finden keine Kollisionen

**Lösung:**
1. Stelle sicher, dass deine Strecke Kollisions-Geometrie hat
2. Erhöhe `TrackWidthSampleCount` (mehr Samples)
3. Oder setze Breite manuell im Python-Script

### "Max Speed = 0"

**Ursache:** CarPawn-Parameter konnten nicht gelesen werden

**Lösung:**
1. Stelle sicher, dass `CarPawnClass` gesetzt ist
2. Teste dein Auto im Spiel und notiere die max. Geschwindigkeit
3. Passe die Werte manuell im Python-Script an

### "Analysis takes too long"

**Ursache:** Zu viele Samples

**Lösung:**
1. Reduziere `SplineSampleCount` (z.B. 50 statt 100)
2. Reduziere `TrackWidthSampleCount` (z.B. 5 statt 10)

## Integration mit Python-Script

Das Widget kann die Ergebnisse als JSON exportieren, die dann vom Python-Script verwendet werden können:

```powershell
# 1. Analysiere in Unreal (Widget)
# 2. Exportiere zu JSON (Widget Button)
# 3. Verwende Python-Script mit den Werten
python suggest_agent_parameters.py --track-width 1000 --max-speed 50 --min-radius 5
```

Oder lade die JSON direkt:
```python
import json
with open("Saved/Training/ParameterSuggestions.json", 'r') as f:
    data = json.load(f)
    rec = data['Recommended']
    print(f"TrackHalfWidthCm = {rec['TrackHalfWidthCm']}")
```

## Nächste Schritte

1. ✅ **Analysiere Track und Car** → Widget
2. ✅ **Überprüfe empfohlene Werte** → Widget oder Console
3. ✅ **Kopiere Werte** → In RacingAgentComponent Blueprint
4. ✅ **Teste Training** → Beobachte Agent-Verhalten
5. ✅ **Passe an** → Basierend auf Training-Ergebnissen

---

Viel Erfolg! 🚗🤖
