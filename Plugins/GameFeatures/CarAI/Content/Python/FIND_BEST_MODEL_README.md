# Bestes Modell finden - Script-Anleitung

Das Script `find_best_model.py` analysiert alle trainierten Modelle und identifiziert das beste basierend auf verschiedenen Metriken.

## Verwendung

### Basis-Verwendung (nur Loss-Analyse)

```powershell
cd "C:\Users\YvesT\Documents\Unreal Projects\StuntCarRacer\Plugins\GameFeatures\CarAI\Content\Python"
python find_best_model.py
```

Das Script:
- Findet alle `model_epoch_*.pt` Dateien im Models-Verzeichnis
- Analysiert die Loss-Werte aus der Training History
- Erstellt Rankings basierend auf verschiedenen Kriterien
- Gibt eine Empfehlung aus

### Mit Evaluation auf Test-Daten

```powershell
python find_best_model.py --evaluate
```

Das Script:
- Führt zusätzlich eine Evaluation auf Test-Daten durch
- Verwendet 20% der Export-Daten als Test-Set (standardmäßig)
- Berechnet Value Error und andere Metriken

### Optionen

```powershell
python find_best_model.py --help
```

**Verfügbare Optionen:**
- `--model-dir PATH`: Verzeichnis mit Modellen (Standard: `Saved/Training/Models`)
- `--export-dir PATH`: Verzeichnis mit Export-Daten (Standard: `Saved/Training/Exports`)
- `--evaluate`: Führe Evaluation auf Test-Daten durch
- `--test-split FLOAT`: Anteil für Test-Set (0.0-1.0, Standard: 0.2)

## Was wird analysiert?

### 1. Loss-Werte
- **Value Loss Final**: Finaler Value Loss (niedriger = besser)
- **Value Loss Min**: Minimaler Value Loss während Training
- **Value Loss Mean**: Durchschnittlicher Value Loss
- **Policy Loss Final**: Finaler Policy Loss
- **Total Loss**: Kombinierter Loss

### 2. Trends
- **Sinkend** 📉: Loss sinkt kontinuierlich (gut!)
- **Stabil** ➡️: Loss bleibt konstant
- **Steigend** 📈: Loss steigt (Overfitting möglich!)

### 3. Rankings

Das Script erstellt mehrere Rankings:

1. **Value Loss Final**: Niedrigster finaler Value Loss
2. **Value Loss Min**: Niedrigster minimaler Value Loss
3. **Total Loss Final**: Niedrigster finaler Total Loss
4. **Trend**: Bestes Trend-Verhalten
5. **Kombiniert**: Gewichteter Score aus allen Metriken

## Ausgabe

### Konsolen-Ausgabe

```
================================================================================
MODELL-ANALYSE: Beste Generation finden
================================================================================

📊 Gefundene Modelle: 10
--------------------------------------------------------------------------------

1. model_epoch_1.pt (Epoche 1)
   Value Loss (final): 1.234567
   Value Loss (min):   1.123456
   Trend: 📉 Sinkend (gut!)

...

🏆 RANKINGS
================================================================================

🥇 Bestes Modell (kombinierter Score):
   model_epoch_8.pt (Epoche 8)
   Score: 0.456789 (niedriger = besser)

💡 EMPFEHLUNG
================================================================================

✅ Empfohlenes Modell: model_epoch_8.pt
   Epoche: 8

📝 Nächste Schritte:
   1. Exportiere Modell zu JSON:
      python export_model_for_unreal.py "...\model_epoch_8.pt" "...\model_epoch_8.json"
   2. Lade Modell in Unreal Editor Widget
   3. Teste das Modell im Spiel
```

### JSON-Ausgabe

Das Script speichert auch eine detaillierte Analyse als JSON:

```
Saved/Training/Models/model_analysis.json
```

Enthält:
- Alle Metriken für jedes Modell
- Rankings (Top 5 für jede Kategorie)
- Evaluation-Ergebnisse (falls `--evaluate` verwendet)

## Interpretation der Ergebnisse

### Bestes Modell finden

**Standard-Empfehlung:**
- Das Script empfiehlt das Modell mit dem **niedrigsten kombinierten Score**
- Dieser Score berücksichtigt:
  - Value Loss Final (40% Gewicht)
  - Value Loss Min (30% Gewicht)
  - Trend (30% Gewicht)

### Overfitting erkennen

**Warnzeichen:**
- Value Loss steigt in späteren Epochen (Trend = 📈)
- Script warnt: "⚠️ WARNUNG: Value Loss steigt (Overfitting möglich!)"

**Lösung:**
- Teste eine frühere Epoche (z.B. `epoch_7` statt `epoch_10`)
- Oder verwende das Modell mit dem niedrigsten Value Loss Min

### Beispiel-Interpretation

```
Epoche 1: Value Loss = 1.5  (hoch, normal am Anfang)
Epoche 5: Value Loss = 0.8  (sinkt, gut!)
Epoche 8: Value Loss = 0.5  (niedrigster Wert!)
Epoche 10: Value Loss = 0.7 (steigt wieder, Overfitting!)
```

**Empfehlung:** Epoche 8 (niedrigster Value Loss)

## Häufige Fragen

### Q: Warum wird nicht immer die letzte Epoche empfohlen?

**A:** Spätere Epochen können **Overfitting** zeigen (Loss steigt wieder). Das Script erkennt das automatisch und empfiehlt die beste Epoche.

### Q: Was ist der Unterschied zwischen "Value Loss Final" und "Value Loss Min"?

**A:**
- **Final**: Der Loss am Ende der Epoche (kann höher sein durch Overfitting)
- **Min**: Der niedrigste Loss während der Epoche (beste Performance)

### Q: Soll ich `--evaluate` verwenden?

**A:** Optional, aber empfohlen! Die Evaluation testet die Modelle auf ungesehenen Daten und gibt zusätzliche Metriken.

### Q: Was mache ich mit der Empfehlung?

**A:**
1. Exportiere das empfohlene Modell zu JSON (siehe Ausgabe)
2. Lade es in Unreal Editor Widget
3. Teste es im Spiel
4. Falls nicht zufrieden: Teste die Top 3 Modelle manuell

## Troubleshooting

### "Keine Modelle gefunden"

**Ursache:** Keine `model_epoch_*.pt` Dateien im Models-Verzeichnis

**Lösung:**
- Stelle sicher, dass `train_pytorch.py` erfolgreich gelaufen ist
- Prüfe Verzeichnis: `Saved/Training/Models/`

### "Fehler beim Laden von model_epoch_X.pt"

**Ursache:** Beschädigte Modell-Datei

**Lösung:**
- Überspringe diese Datei (Script macht das automatisch)
- Trainiere das Modell neu

### "Keine Export-Dateien gefunden für Evaluation"

**Ursache:** Keine Rollout-Dateien im Export-Verzeichnis

**Lösung:**
- Das ist OK - Evaluation ist optional
- Script funktioniert auch ohne Evaluation (nur Loss-Analyse)

## Tipps

1. **Regelmäßig ausführen:** Nach jedem Training das Script ausführen, um das beste Modell zu finden

2. **Mehrere Metriken prüfen:** Nicht nur auf "Value Loss Final" schauen - auch "Trend" und "Min" berücksichtigen

3. **Manuell testen:** Die Top 3 Modelle manuell in Unreal testen - manchmal fühlt sich ein Modell besser an, auch wenn die Metriken anders sind

4. **Evaluation verwenden:** `--evaluate` gibt zusätzliche Sicherheit, dass das Modell auch auf neuen Daten gut funktioniert

---

Viel Erfolg beim Finden des besten Modells! 🚗🤖
