# Trainingszeit: Wie lange bis die Agents um die Kurven kommen?

## ⚡ Schnellübersicht (für starke Rechner mit 30-50 Agents)

**Mit 30-50 Agents bei 60 FPS:**
- **10 Rollouts:** Nur **~7-11 Sekunden!** ⚡⚡⚡
- **Komplette Iteration (10 Rollouts + Training):** ~2-3 Minuten
- **"Um die Kurven kommen":** ~1-3 Minuten (3-4 Iterationen) ⚡⚡⚡

**Das ist EXTREM schnell!** Mit einem guten Rechner kannst du in wenigen Minuten erste Erfolge sehen!

---

## 🎯 Realistische Einschätzung

### "Um die Kurven kommen" = Erste sichtbare Erfolge

Das bedeutet:
- ✅ Agents fahren **mehrere Sekunden** auf der Strecke
- ✅ Agents können **einfache Kurven** navigieren
- ✅ **Weniger sofortige Crashes** (nicht mehr nach 0.5 Sekunden von Strecke)
- ⚠️ **Noch nicht perfekt** - aber sichtbare Verbesserung!

---

## 📊 Typischer Trainingsprozess

### Iteration 1: Erste Daten sammeln

**In Unreal (Daten sammeln):**
- **10-20 Rollouts** sammeln (~5-15 Minuten)
- Jeder Rollout = 2048 Steps
- **Zeit pro Rollout:** ~30-60 Sekunden (bei 30-60 FPS)
  - Bei 60 FPS: 2048 Steps / 60 = ~34 Sekunden
  - Bei 30 FPS: 2048 Steps / 30 = ~68 Sekunden
- Bei 4 Agents = ~512 Steps pro Agent pro Rollout
- **Wichtig:** `bExportOnly = true` (nur Daten sammeln, kein Training)

**In Python (Modell trainieren):**
- **5-10 Epochen** (~1-2 Minuten)
- Modell wird gespeichert: `model_epoch_1.pt` bis `model_epoch_10.pt`

**Zurück in Unreal (Testen):**
- Modell laden und testen
- **Erwartung:** Agents sind immer noch schlecht, aber evtl. schon etwas besser

---

### Iteration 2-3: Weiter trainieren

**In Unreal:**
- **5-10 Rollouts** sammeln (~3-10 Minuten)
- **Zeit pro Rollout:** ~30-60 Sekunden
- Agents fahren jetzt mit **trainiertem Modell** (sollten besser sein)

**In Python:**
- **5-10 Epochen** (~1-2 Minuten)
- Training mit **alten + neuen Daten** (oder nur neuen, je nach Strategie)

**Zurück in Unreal:**
- Modell laden und testen
- **Erwartung:** Erste sichtbare Verbesserungen!

---

### Iteration 4-5: Optimieren

**In Unreal:**
- **5-10 Rollouts** (~3-10 Minuten)
- **Zeit pro Rollout:** ~30-60 Sekunden
- Agents sollten jetzt **besser fahren**

**In Python:**
- **5-10 Epochen** (~1-2 Minuten)

**Zurück in Unreal:**
- **Erwartung:** Agents kommen um einige Kurven! 🎉

---

## ⏱️ Zeitaufwand pro Iteration

### Unreal (Daten sammeln):

**WICHTIG:** Rollout Steps werden **über alle Agents summiert**, nicht pro Agent!

- **Rollout = 2048 Steps GESAMT** (über alle Agents)
- **Zeit pro Rollout** = 2048 Steps ÷ Anzahl Agents ÷ FPS

**Beispiele:**
- **50 Agents bei 60 FPS:** 2048 ÷ 50 ÷ 60 = **~0.68 Sekunden** pro Rollout ⚡
- **30 Agents bei 60 FPS:** 2048 ÷ 30 ÷ 60 = **~1.14 Sekunden** pro Rollout ⚡
- **10 Agents bei 60 FPS:** 2048 ÷ 10 ÷ 60 = **~3.4 Sekunden** pro Rollout
- **8 Agents bei 60 FPS:** 2048 ÷ 8 ÷ 60 = **~4.3 Sekunden** pro Rollout
- **4 Agents bei 60 FPS:** 2048 ÷ 4 ÷ 60 = **~8.5 Sekunden** pro Rollout
- **10 Agents bei 30 FPS:** 2048 ÷ 10 ÷ 30 = **~6.8 Sekunden** pro Rollout

**10 Rollouts:**
- Bei 50 Agents / 60 FPS: 10 × 0.68 Sek = **~7 Sekunden** ⚡⚡⚡
- Bei 30 Agents / 60 FPS: 10 × 1.14 Sek = **~11 Sekunden** ⚡⚡
- Bei 10 Agents / 60 FPS: 10 × 3.4 Sek = **~34 Sekunden** (~0.5 Minuten!)
- Bei 8 Agents / 60 FPS: 10 × 4.3 Sek = **~43 Sekunden** (~0.7 Minuten!)
- Bei 4 Agents / 60 FPS: 10 × 8.5 Sek = **~85 Sekunden** (~1.4 Minuten!)

**5 Rollouts:**
- Bei 50 Agents / 60 FPS: 5 × 0.68 Sek = **~3.4 Sekunden** ⚡⚡⚡
- Bei 30 Agents / 60 FPS: 5 × 1.14 Sek = **~5.7 Sekunden** ⚡⚡
- Bei 10 Agents / 60 FPS: 5 × 3.4 Sek = **~17 Sekunden**
- Bei 8 Agents / 60 FPS: 5 × 4.3 Sek = **~22 Sekunden**
- Bei 4 Agents / 60 FPS: 5 × 8.5 Sek = **~43 Sekunden**

- **Hängt ab von:**
  - **Anzahl Agents** (mehr Agents = viel schneller!)
  - **FPS/Performance** (30 FPS = doppelte Zeit vs 60 FPS)
  - Streckenschwierigkeit (affectiert Episodenlänge, aber nicht Rollout-Zeit)

### Python (Training):
- **10 Epochen:** ~1-2 Minuten
- **Hängt ab von:**
  - Anzahl Rollout-Dateien
  - Netzwerk-Größe
  - CPU/GPU Performance

### Testen & Anpassen:
- **Modell laden & testen:** ~2-5 Minuten
- **Parameter anpassen (falls nötig):** ~5-10 Minuten

**Gesamt pro Iteration: ~15-40 Minuten**

---

## 🎯 Realistische Zeitschätzung: "Um die Kurven kommen"

### Optimistisch (einfache Strecke, gute Parameter):

**Iteration 1:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~1-2 Minuten (0.5-1 Min Rollouts + 1 Min PyTorch + 0.5 Min Test)
- Bei 10 Agents: Rollouts in ~34 Sekunden!
- Bei 30 Agents: Rollouts in ~11 Sekunden! ⚡
- Bei 50 Agents: Rollouts in ~7 Sekunden! ⚡⚡⚡
- Ergebnis: Meist noch schlecht, aber erste Tendenzen sichtbar

**Iteration 2:** 5 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~1-1.5 Minuten (0.3 Min Rollouts + 1 Min PyTorch + 0.5 Min Test)
- Bei 30 Agents: Rollouts in ~6 Sekunden! ⚡
- Bei 50 Agents: Rollouts in ~3.4 Sekunden! ⚡⚡⚡
- Ergebnis: Erste Verbesserungen!

**Iteration 3:** 5 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~1-1.5 Minuten
- Ergebnis: **Agents kommen um einfache Kurven!** ✅

**Gesamt: ~3-5 Minuten** (bei 10 Agents, 60 FPS)
**Gesamt: ~1.5-2.5 Minuten** (bei 30 Agents, 60 FPS) ⚡⚡
**Gesamt: ~1-2 Minuten** (bei 50 Agents, 60 FPS) ⚡⚡⚡

---

### Realistisch (normale Strecke, Standard-Parameter):

**Iteration 1:** 15 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~10-12 Minuten (9 Min Rollouts + 1 Min PyTorch + 2 Min Test)
- Ergebnis: Meist noch schlecht

**Iteration 2:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~7-9 Minuten (6 Min Rollouts + 1 Min PyTorch + 2 Min Test)
- Ergebnis: Erste Verbesserungen

**Iteration 3:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~7-9 Minuten
- Ergebnis: Bessere Fortschritte

**Iteration 4:** 5 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~5-7 Minuten
- Ergebnis: **Agents kommen um einige Kurven!** ✅

**Gesamt: ~29-37 Minuten (30-40 Minuten)**

---

### Pessimistisch (schwierige Strecke, Suboptimale Parameter):

**Iteration 1:** 20 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~13-15 Minuten (12 Min Rollouts + 1 Min PyTorch + 2 Min Test)
- Ergebnis: Noch schlecht, Parameter müssen angepasst werden

**Parameter anpassen:** `W_Lateral`, `W_Heading` erhöhen
- Zeit: ~5 Minuten

**Iteration 2:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~7-9 Minuten
- Ergebnis: Besser, aber noch nicht gut

**Iteration 3:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~7-9 Minuten
- Ergebnis: Erste Erfolge

**Iteration 4:** 10 Rollouts → PyTorch (10 Epochen) → Test
- Zeit: ~7-9 Minuten
- Ergebnis: **Agents kommen um Kurven!** ✅

**Gesamt: ~39-47 Minuten (40-50 Minuten)**

---

## 📈 Wichtige Faktoren, die die Zeit beeinflussen

### 1. **Anzahl Agents** (WICHTIGSTER FAKTOR!)
- **4 Agents:** Standard (empfohlen für Start)
- **8 Agents:** 2x schneller, aber mehr Performance nötig
- **10 Agents:** 2.5x schneller als 4 Agents
- **30 Agents:** 7.5x schneller als 4 Agents! ⚡
- **50 Agents:** 12.5x schneller als 4 Agents! ⚡⚡⚡
- **Mehr Agents = exponentiell schnelleres Training!**

### 2. **Streckenschwierigkeit**
- **Einfache Strecke** (große Kurvenradien): Schnelleres Lernen
- **Schwierige Strecke** (scharfe Kurven): Braucht mehr Training
- **Viele Sprünge:** Zusätzliche Komplexität

### 3. **Parameter (Reward-Shaping)**
- **Gute Parameter:** Schnelleres Lernen
- **Schlechte Parameter:** Braucht mehr Iterationen
- **Tipp:** Verwende das `ParameterSuggestionWidget`!

### 4. **Anzahl Rollouts**
- **Weniger Rollouts (5-10):** Schneller, aber weniger stabil
- **Mehr Rollouts (15-20):** Langsamer, aber stabiler

### 5. **Python Training (Epochen)**
- **5 Epochen:** Schneller, kann unterfitten
- **10 Epochen:** Guter Kompromiss (empfohlen)
- **20 Epochen:** Langsamer, kann overfitten

---

## ✅ Checkliste: Wann sind die Agents "gut genug"?

### Zeichen für erste Erfolge:

- ✅ **Progress MA100** steigt (Agents kommen weiter)
- ✅ **Reward MA100** steigt (höhere Belohnungen)
- ✅ **Episoden dauern länger** (nicht mehr sofort Crashes)
- ✅ **Weniger "OffTrack" Terminierungen** (Agents bleiben auf Strecke)
- ✅ **Sichtbar:** Agents können **einfache Kurven** fahren
- ⚠️ **Noch nicht:** Komplette Strecke, perfektes Fahren

### Wenn es NICHT funktioniert nach 4-5 Iterationen:

1. **Parameter prüfen:**
   - `W_Lateral` und `W_Heading` erhöhen (mehr Strafe für Abweichen)
   - `TerminalPenalty_OffTrack` erhöhen

2. **Strecke prüfen:**
   - Ist die Strecke zu schwierig?
   - Gibt es Kollisions-Geometrie?
   - Ist der Track Provider gesetzt?

3. **Observations prüfen:**
   - `bDrawObservationDebug = true` aktivieren
   - Prüfen, ob Lookahead-Punkte korrekt sind

4. **Mehr Daten sammeln:**
   - Mehr Rollouts (20+)
   - Mehr Epochen in Python (15-20)

---

## 🚀 Tipps für schnelleres Training

### 1. **Starte mit vielen Agents** (BESTER TIPP!)
- **30-50 Agents** (wenn Performance erlaubt) - **extrem schnell!**
- 8-10 Agents (wenn weniger Performance verfügbar)
- **Mehr Agents = exponentiell schnelleres Training!**
- Bei 50 Agents: 10 Rollouts in nur **~7 Sekunden!** ⚡⚡⚡

### 2. **Verwende Parameter-Suggestion Widget**
- Automatische Parameter-Empfehlung
- Spart Zeit bei der Konfiguration

### 3. **Sammle viele Rollouts beim ersten Training**
- 15-20 Rollouts für erste Iteration
- Bessere Start-Position

### 4. **Teste regelmäßig**
- Nach jeder Iteration das Modell testen
- Erkenne Probleme früh

### 5. **Behalte alte Daten**
- Kontinuierliches Lernen aus allen Daten
- Stabileres Training

---

## 📊 Zusammenfassung

**Realistische Zeitschätzung: "Um die Kurven kommen"**

**Bei 50 Agents / 60 FPS (EXTREM SCHNELL!):**
- **Optimistisch:** ~1-2 Minuten (3 Iterationen) ⚡⚡⚡
- **Realistisch:** ~2-3 Minuten (4 Iterationen) ⚡⚡
- **Pessimistisch:** ~3-5 Minuten (4-5 Iterationen + Parameter-Anpassung) ⚡

**Bei 30 Agents / 60 FPS (SEHR SCHNELL!):**
- **Optimistisch:** ~1.5-2.5 Minuten (3 Iterationen) ⚡⚡
- **Realistisch:** ~2.5-4 Minuten (4 Iterationen) ⚡
- **Pessimistisch:** ~4-6 Minuten (4-5 Iterationen + Parameter-Anpassung)

**Bei 10 Agents / 60 FPS (schnell):**
- **Optimistisch:** ~3-5 Minuten (3 Iterationen)
- **Realistisch:** ~5-8 Minuten (4 Iterationen)
- **Pessimistisch:** ~8-12 Minuten (4-5 Iterationen + Parameter-Anpassung)

**Bei 4 Agents / 60 FPS (langsamer):**
- **Optimistisch:** ~5-8 Minuten (3 Iterationen)
- **Realistisch:** ~8-12 Minuten (4 Iterationen)
- **Pessimistisch:** ~12-15 Minuten (4-5 Iterationen + Parameter-Anpassung)

**Wichtig:** 
- **Mehr Agents = exponentiell schneller!** (50 Agents = 12.5x schneller als 4 Agents!)
- Bei **30 FPS verdoppelt** sich die Zeit für Rollouts
- PyTorch-Training dauert ~1 Minute (unabhängig von Agents)
- **Bei 30-50 Agents: Rollouts sind praktisch sofort fertig!** ⚡

**Wichtig:** 
- Dies ist ein **iterativer Prozess**
- **Ein Rollout dauert nur ~30-60 Sekunden** (bei 30-60 FPS)
- **Erste Erfolge** sind nach 30-50 Minuten realistisch
- **Perfektes Fahren** braucht deutlich länger (viele weitere Iterationen)
- **Performance ist wichtig:** 60 FPS = doppelt so schnell wie 30 FPS!

**Nach 4-5 Iterationen solltest du definitiv sehen, dass die Agents um einfache Kurven kommen!** 🎉

### ⚡ Performance-Tipp (WICHTIGSTER FAKTOR!)

**Für schnellstes Training:**
- Verwende **30-50 Agents** (wenn Performance erlaubt) - **das ist der größte Faktor!**
- Stelle sicher, dass Unreal mit **60 FPS** läuft
- Reduziere Grafik-Einstellungen für höhere FPS
- **Bei 50 Agents / 60 FPS:** 10 Rollouts = **~7 Sekunden!** ⚡⚡⚡
- **Bei 30 Agents / 60 FPS:** 10 Rollouts = **~11 Sekunden!** ⚡⚡
- **Bei 10 Agents / 60 FPS:** 10 Rollouts = **~34 Sekunden!** ⚡
- **Bei 4 Agents / 60 FPS:** 10 Rollouts = ~85 Sekunden
- **Bei 10 Agents / 30 FPS:** 10 Rollouts = ~68 Sekunden
- **Bei 4 Agents / 30 FPS:** 10 Rollouts = ~170 Sekunden (fast 3x langsamer!)

**Der größte Einflussfaktor ist die Anzahl Agents!** 
- **50 Agents = 12.5x schneller als 4 Agents!**
- **30 Agents = 7.5x schneller als 4 Agents!**
- **Mehr Agents = exponentiell schnelleres Training!**

**Mit 30-50 Agents: Eine komplette Iteration (10 Rollouts + Training) dauert nur ~2-3 Minuten!** ⚡⚡⚡

---

Viel Erfolg beim Training! 🚗🤖
