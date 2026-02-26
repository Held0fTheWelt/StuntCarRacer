# Kurven-Training: Tipps und Lösungen

## Problem: Agent lernt Kurven nicht vernünftig

Wenn dein Agent in Kurven Probleme hat (z.B. von der Strecke fährt, zu langsam ist, oder unsauber fährt), gibt es mehrere Ansätze:

---

## 1. Kurzfristig: Reward-Parameter anpassen

Falls der Agent in Kurven von der Strecke fährt, erhöhe die Bestrafungen für seitliche Abweichung:

### Im `RacingAgentComponent` → `RewardCfg`:

**Leichtes Problem (Agent weicht gelegentlich ab):**
- `W_Lateral`: von `-0.15` auf `-0.25` erhöhen
- `W_Heading`: von `-0.15` auf `-0.25` erhöhen  
- `TerminalPenalty_OffTrack`: von `-2.0` auf `-3.0` erhöhen

**Mittleres Problem (Agent fliegt regelmäßig ab):**
- `W_Lateral`: auf `-0.30` oder `-0.35` erhöhen
- `W_Heading`: auf `-0.30` oder `-0.35` erhöhen
- `TerminalPenalty_OffTrack`: auf `-4.0` oder `-5.0` erhöhen

**Starkes Problem (Agent fliegt sehr oft ab):**
- `W_Lateral`: auf `-0.40` erhöhen
- `W_Heading`: auf `-0.40` erhöhen
- `TerminalPenalty_OffTrack`: auf `-5.0` oder `-6.0` erhöhen

### ⚠️ Wichtig zu verstehen:

Die Reward-Werte werden **quadriert** verwendet (`LateralNorm * LateralNorm`), daher wirken schon moderate Erhöhungen sehr stark:

- `W_Lateral = -0.15` mit `LateralNorm = 0.5` → Bestrafung = -0.15 * 0.25 = **-0.0375**
- `W_Lateral = -0.25` mit `LateralNorm = 0.5` → Bestrafung = -0.25 * 0.25 = **-0.0625** (1.67x stärker!)
- `W_Lateral = -0.25` mit `LateralNorm = 1.0` → Bestrafung = -0.25 * 1.0 = **-0.25** (4x stärker bei doppelter Abweichung!)

---

## 2. Mittelfristig: Training beobachten

### Prüfe die Trends:

- **Steigt `RewardMA100` kontinuierlich?**
  - ✅ Ja → Agent lernt, weiter trainieren
  - ❌ Nein → Parameter anpassen oder mehr Daten sammeln

- **Steigt `ProgressMA100`?**
  - ✅ Ja → Agent kommt weiter, wird besser
  - ❌ Nein → Agent lernt keine neuen Bereiche

- **Werden Episoden länger?**
  - ✅ Ja → Agent überlebt länger, gut!
  - ❌ Nein → Agent wird nicht besser

- **Wie viele "OffTrack" Terminierungen?**
  - Weniger wird besser → Agent bleibt öfter auf der Strecke
  - Mehr wird schlechter → Reward-Parameter anpassen

### Falls sich nichts verbessert:

- **Parameter anpassen** (siehe Punkt 1)
- **Mehr Daten sammeln** (mehr Rollouts)
- **Schauen, ob Agent überhaupt besser wird** (Trends prüfen)

---

## 3. Langfristig: Mehr Training

### Mehr Rollouts sammeln:

- **20+ Rollouts** vor Python-Training (statt 10)
- Mehr Erfahrungen = besseres Training

### Mehr Epochen im Python-Training:

- **Standard:** 10 Epochen
- **Empfohlen:** 15-20 Epochen für schwierige Strecken
- Mehr Epochen = Agent lernt komplexere Muster

### Mehr Iterationen:

1. **Unreal:** Daten sammeln (Rollouts)
2. **Python:** Modell trainieren
3. **Unreal:** Neues Modell laden und testen
4. **Wiederholen:** Mehr Iterationen = bessere Performance

---

## 4. Diagnose-Tipps

### Debug-Visualisierung aktivieren:

Im `RacingAgentComponent`:
- **`bDrawObservationDebug = true`**
  - Zeigt Lookahead-Punkte in der Welt
  - Hilft zu sehen, was der Agent "sieht"

- **`bDrawRewardHUD = true`**
  - Zeigt aktuelle Rewards während des Fahrens
  - Hilft zu verstehen, warum Agent bestimmte Actions wählt

### Logs prüfen:

In der **Output Log**:
- **Suche nach "OffTrack"**
  - Wie oft passiert das?
  - In welchen Bereichen?

- **Episode-Längen prüfen**
  - Wie viele Steps pro Episode?
  - Werden Episoden länger?

---

## 5. Empfehlung: Schrittweise vorgehen

### Schritt 1: Jetzt (schnell):

1. `W_Lateral` und `W_Heading` auf `-0.25` erhöhen
2. Training weiterlaufen lassen
3. Beobachten, ob Agent besser wird

### Schritt 2: Kurzfristig (nächste Iteration):

1. **10-20 Rollouts** sammeln
2. **Python-Training:** 10-15 Epochen
3. **Neues Modell laden** und testen
4. **Prüfen:** Wird Agent besser?

### Schritt 3: Falls nicht besser:

1. **Noch stärkere Reward-Parameter** (`W_Lateral = -0.30`, `W_Heading = -0.30`)
2. **Noch mehr Training** (20+ Rollouts, 15-20 Epochen)
3. **Schrittweise anpassen** - nicht alles auf einmal ändern!

---

## 6. Wichtig zu wissen

### Kurven sind schwieriger:

- **Geraden:** Einfach zu lernen (geradeaus fahren)
- **Kurven:** Komplexer (Speed anpassen, Lenken, Timing)
- **Schärfere Kurven:** Noch schwieriger

Der Agent braucht **mehr Beispiele für Kurven**, um sie zu verstehen. Daher:

- ✅ **Geduld haben** - Kurven brauchen mehr Training
- ✅ **Schrittweise anpassen** - Nicht alles auf einmal ändern
- ✅ **Trends beobachten** - Steigt der Reward? Wird Agent besser?
- ✅ **Iterativ vorgehen** - Mehr Iterationen = bessere Performance

---

## 7. Typische Werte für gute Performance

### Reward-Parameter (je nach Strecken-Schwierigkeit):

**Einfache Strecke (wenig Kurven):**
- `W_Lateral = -0.15`
- `W_Heading = -0.15`
- `TerminalPenalty_OffTrack = -2.0`

**Mittlere Strecke (einige Kurven):**
- `W_Lateral = -0.25`
- `W_Heading = -0.25`
- `TerminalPenalty_OffTrack = -3.0`

**Schwierige Strecke (viele/scharfe Kurven):**
- `W_Lateral = -0.30` bis `-0.40`
- `W_Heading = -0.30` bis `-0.40`
- `TerminalPenalty_OffTrack = -4.0` bis `-5.0`

---

## Zusammenfassung

**Kurven-Training braucht:**
1. ✅ **Geduld** - Mehr Training als Geraden
2. ✅ **Richtige Reward-Parameter** - Nicht zu schwach, nicht zu stark
3. ✅ **Schrittweise Anpassung** - Nicht alles auf einmal ändern
4. ✅ **Trends beobachten** - Wird Agent besser?
5. ✅ **Mehr Iterationen** - Unreal → Python → Unreal → Python → ...

**Falls Agent nicht besser wird:**
- Parameter anpassen (siehe Punkt 1)
- Mehr Training (mehr Rollouts, mehr Epochen)
- Geduld haben - Kurven sind schwierig!
