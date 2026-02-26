"""
Empfiehlt passende Werte für RacingAgentComponent basierend auf Spline und Auto-Konfiguration.

Verwendung:
    python suggest_agent_parameters.py
    
Oder mit Argumenten:
    python suggest_agent_parameters.py --track-width 1000 --max-speed 50 --min-radius 500
"""

import argparse
import math

# ============================================================================
# Standard-Werte (aus RacingAgentComponent.h)
# ============================================================================

DEFAULT_TRACK_HALF_WIDTH_CM = 500.0
DEFAULT_HEADING_NORM_RAD = 0.8
DEFAULT_SPEED_NORM_CM_PER_SEC = 4500.0  # ~45 m/s = ~162 km/h
DEFAULT_ANG_VEL_NORM_DEG_PER_SEC = 220.0
DEFAULT_CURVATURE_NORM_INV_CM = 0.0025  # 1/400cm = Radius ~4m
DEFAULT_LOOKAHEAD_OFFSETS_CM = [200.0, 600.0, 1200.0, 2000.0]

# ============================================================================
# Berechnungsfunktionen
# ============================================================================

def suggest_track_half_width(track_width_cm):
    """
    Empfiehlt TrackHalfWidthCm basierend auf tatsächlicher Streckenbreite.
    
    Regel: TrackHalfWidthCm sollte etwa die Hälfte der Streckenbreite sein.
    """
    if track_width_cm is None:
        return DEFAULT_TRACK_HALF_WIDTH_CM, "Standard (keine Eingabe)"
    
    # TrackHalfWidthCm ist die HALBE Breite (daher "Half")
    suggested = track_width_cm / 2.0
    
    # Mindestwert: 100cm (sehr schmale Strecke)
    suggested = max(100.0, suggested)
    
    # Maximum: 2000cm (sehr breite Strecke)
    suggested = min(2000.0, suggested)
    
    return suggested, f"Basierend auf Streckenbreite: {track_width_cm}cm"

def suggest_speed_norm(max_speed_kmh, max_speed_ms=None):
    """
    Empfiehlt SpeedNormCmPerSec basierend auf maximaler Auto-Geschwindigkeit.
    
    Regel: SpeedNorm sollte etwa 1.2-1.5x der maximalen Geschwindigkeit sein.
    """
    if max_speed_ms is None:
        if max_speed_kmh is None:
            return DEFAULT_SPEED_NORM_CM_PER_SEC, "Standard (keine Eingabe)"
        # Konvertiere km/h zu m/s
        max_speed_ms = max_speed_kmh / 3.6
    
    # Konvertiere m/s zu cm/s
    max_speed_cm_per_sec = max_speed_ms * 100.0
    
    # SpeedNorm sollte 1.3x der maximalen Geschwindigkeit sein
    # (gibt Raum für Überschreitungen während Training)
    suggested = max_speed_cm_per_sec * 1.3
    
    # Mindestwert: 1000 cm/s (10 m/s = 36 km/h)
    suggested = max(1000.0, suggested)
    
    # Maximum: 10000 cm/s (100 m/s = 360 km/h)
    suggested = min(10000.0, suggested)
    
    return suggested, f"Basierend auf max. Geschwindigkeit: {max_speed_ms:.1f} m/s ({max_speed_kmh:.0f} km/h)"

def suggest_curvature_norm(min_radius_cm, min_radius_m=None):
    """
    Empfiehlt CurvatureNormInvCm basierend auf minimalem Kurvenradius.
    
    Regel: CurvatureNormInvCm = 1 / (4 * min_radius)
    """
    if min_radius_m is None:
        if min_radius_cm is None:
            return DEFAULT_CURVATURE_NORM_INV_CM, "Standard (keine Eingabe)"
        min_radius_m = min_radius_cm / 100.0
    else:
        min_radius_cm = min_radius_m * 100.0
    
    # CurvatureNormInvCm sollte etwa 1/(4 * radius) sein
    # Das gibt einen guten Normalisierungsbereich
    suggested = 1.0 / (4.0 * min_radius_cm)
    
    # Mindestwert: 0.0001 (1/10000cm = Radius 10m)
    suggested = max(0.0001, suggested)
    
    # Maximum: 0.01 (1/100cm = Radius 1m - sehr scharfe Kurve)
    suggested = min(0.01, suggested)
    
    return suggested, f"Basierend auf min. Kurvenradius: {min_radius_m:.1f}m ({min_radius_cm:.0f}cm)"

def suggest_heading_norm(track_type="standard"):
    """
    Empfiehlt HeadingNormRad basierend auf Streckentyp.
    
    Regel: 
    - Standard: 0.8 rad (~45°)
    - Scharfe Kurven: 1.0 rad (~57°)
    - Sanfte Kurven: 0.6 rad (~34°)
    """
    suggestions = {
        "standard": (0.8, "Standard-Strecke"),
        "sharp": (1.0, "Scharfe Kurven"),
        "gentle": (0.6, "Sanfte Kurven"),
    }
    
    return suggestions.get(track_type, suggestions["standard"])

def suggest_ang_vel_norm(max_speed_ms, min_radius_m):
    """
    Empfiehlt AngVelNormDegPerSec basierend auf Auto und Strecke.
    
    Regel: AngVel = (Speed / Radius) in rad/s, dann zu deg/s
    """
    if max_speed_ms is None or min_radius_m is None:
        return DEFAULT_ANG_VEL_NORM_DEG_PER_SEC, "Standard (keine Eingabe)"
    
    # Maximale Winkelgeschwindigkeit = v / r (in rad/s)
    max_ang_vel_rad_per_sec = max_speed_ms / min_radius_m
    
    # Konvertiere zu deg/s
    max_ang_vel_deg_per_sec = math.degrees(max_ang_vel_rad_per_sec)
    
    # Norm sollte 1.5x der maximalen Winkelgeschwindigkeit sein
    suggested = max_ang_vel_deg_per_sec * 1.5
    
    # Mindestwert: 50 deg/s
    suggested = max(50.0, suggested)
    
    # Maximum: 500 deg/s
    suggested = min(500.0, suggested)
    
    return suggested, f"Basierend auf v={max_speed_ms:.1f}m/s, r={min_radius_m:.1f}m"

def suggest_lookahead_offsets(track_length_m, track_length_cm=None):
    """
    Empfiehlt LookaheadOffsetsCm basierend auf Streckenlänge.
    
    Regel: Lookahead sollte 2%, 6%, 12%, 20% der Streckenlänge sein (min/max begrenzt)
    """
    if track_length_cm is None:
        if track_length_m is None:
            return DEFAULT_LOOKAHEAD_OFFSETS_CM, "Standard (keine Eingabe)"
        track_length_cm = track_length_m * 100.0
    else:
        track_length_m = track_length_cm / 100.0
    
    # Prozentuale Offsets
    percentages = [0.02, 0.06, 0.12, 0.20]
    suggested = [track_length_cm * p for p in percentages]
    
    # Begrenze auf sinnvolle Werte
    # Minimum: 100cm, 300cm, 600cm, 1000cm
    # Maximum: 500cm, 1500cm, 3000cm, 5000cm
    min_offsets = [100.0, 300.0, 600.0, 1000.0]
    max_offsets = [500.0, 1500.0, 3000.0, 5000.0]
    
    for i in range(len(suggested)):
        suggested[i] = max(min_offsets[i], min(suggested[i], max_offsets[i]))
    
    return suggested, f"Basierend auf Streckenlänge: {track_length_m:.1f}m ({track_length_cm:.0f}cm)"

def suggest_reward_weights(track_difficulty="medium"):
    """
    Empfiehlt Reward-Weights basierend auf Streckenschwierigkeit.
    """
    suggestions = {
        "easy": {
            "W_Lateral": -0.10,
            "W_Heading": -0.10,
            "W_Speed": 0.3,
            "W_Progress": 1.2,
            "note": "Einfache Strecke - weniger Bestrafung für Abweichungen"
        },
        "medium": {
            "W_Lateral": -0.15,
            "W_Heading": -0.15,
            "W_Speed": 0.2,
            "W_Progress": 1.0,
            "note": "Standard-Strecke - ausgewogene Gewichte"
        },
        "hard": {
            "W_Lateral": -0.25,
            "W_Heading": -0.25,
            "W_Speed": 0.15,
            "W_Progress": 1.0,
            "note": "Schwierige Strecke - mehr Bestrafung für Abweichungen"
        },
        "very_hard": {
            "W_Lateral": -0.4,
            "W_Heading": -0.4,
            "W_Speed": 0.1,
            "W_Progress": 1.0,
            "note": "Sehr schwierige Strecke - starke Bestrafung für Abweichungen"
        }
    }
    
    return suggestions.get(track_difficulty, suggestions["medium"])

# ============================================================================
# Ausgabe
# ============================================================================

def print_recommendations(args):
    """Drucke alle Empfehlungen"""
    
    print("\n" + "="*80)
    print("RACING AGENT COMPONENT - PARAMETER-EMPFEHLUNGEN")
    print("="*80)
    
    print("\n📊 Eingaben:")
    print(f"   Streckenbreite: {args.track_width_cm or 'Nicht angegeben'} cm")
    print(f"   Max. Geschwindigkeit: {args.max_speed_kmh or 'Nicht angegeben'} km/h")
    print(f"   Min. Kurvenradius: {args.min_radius_m or 'Nicht angegeben'} m")
    print(f"   Streckenlänge: {args.track_length_m or 'Nicht angegeben'} m")
    print(f"   Streckentyp: {args.track_type}")
    print(f"   Schwierigkeit: {args.difficulty}")
    
    print("\n" + "="*80)
    print("🎯 EMPFOHLENE WERTE")
    print("="*80)
    
    # TrackHalfWidthCm
    track_half_width, track_note = suggest_track_half_width(args.track_width_cm)
    print(f"\n1. TrackHalfWidthCm")
    print(f"   Empfohlen: {track_half_width:.1f}")
    print(f"   Standard:  {DEFAULT_TRACK_HALF_WIDTH_CM:.1f}")
    print(f"   Grund:     {track_note}")
    
    # SpeedNormCmPerSec
    speed_norm, speed_note = suggest_speed_norm(args.max_speed_kmh, args.max_speed_ms)
    print(f"\n2. SpeedNormCmPerSec")
    print(f"   Empfohlen: {speed_norm:.1f}")
    print(f"   Standard:  {DEFAULT_SPEED_NORM_CM_PER_SEC:.1f}")
    print(f"   Grund:     {speed_note}")
    print(f"   (Entspricht {speed_norm/100:.1f} m/s = {speed_norm*0.036:.0f} km/h)")
    
    # CurvatureNormInvCm
    curvature_norm, curvature_note = suggest_curvature_norm(None, args.min_radius_m)
    print(f"\n3. CurvatureNormInvCm")
    print(f"   Empfohlen: {curvature_norm:.6f}")
    print(f"   Standard:  {DEFAULT_CURVATURE_NORM_INV_CM:.6f}")
    print(f"   Grund:     {curvature_note}")
    if args.min_radius_m:
        equivalent_radius = 1.0 / (4.0 * curvature_norm) / 100.0
        print(f"   (Entspricht Radius ~{equivalent_radius:.1f}m)")
    
    # HeadingNormRad
    heading_norm, heading_note = suggest_heading_norm(args.track_type)
    print(f"\n4. HeadingNormRad")
    print(f"   Empfohlen: {heading_norm:.2f} rad ({math.degrees(heading_norm):.1f}°)")
    print(f"   Standard:  {DEFAULT_HEADING_NORM_RAD:.2f} rad ({math.degrees(DEFAULT_HEADING_NORM_RAD):.1f}°)")
    print(f"   Grund:     {heading_note}")
    
    # AngVelNormDegPerSec
    ang_vel_norm, ang_vel_note = suggest_ang_vel_norm(args.max_speed_ms, args.min_radius_m)
    print(f"\n5. AngVelNormDegPerSec")
    print(f"   Empfohlen: {ang_vel_norm:.1f}")
    print(f"   Standard:  {DEFAULT_ANG_VEL_NORM_DEG_PER_SEC:.1f}")
    print(f"   Grund:     {ang_vel_note}")
    
    # LookaheadOffsetsCm
    lookahead_offsets, lookahead_note = suggest_lookahead_offsets(args.track_length_m)
    print(f"\n6. LookaheadOffsetsCm")
    print(f"   Empfohlen: [{', '.join(f'{x:.0f}' for x in lookahead_offsets)}]")
    print(f"   Standard:  [{', '.join(f'{x:.0f}' for x in DEFAULT_LOOKAHEAD_OFFSETS_CM)}]")
    print(f"   Grund:     {lookahead_note}")
    
    # Reward Weights
    reward_weights = suggest_reward_weights(args.difficulty)
    print(f"\n7. RewardCfg Weights")
    print(f"   W_Lateral:  {reward_weights['W_Lateral']:.2f} (Standard: -0.15)")
    print(f"   W_Heading:  {reward_weights['W_Heading']:.2f} (Standard: -0.15)")
    print(f"   W_Speed:    {reward_weights['W_Speed']:.2f} (Standard: 0.2)")
    print(f"   W_Progress: {reward_weights['W_Progress']:.2f} (Standard: 1.0)")
    print(f"   Grund:     {reward_weights['note']}")
    
    print("\n" + "="*80)
    print("📝 UNREAL ENGINE SETUP")
    print("="*80)
    
    print("\nKopiere diese Werte in dein RacingAgentComponent Blueprint:")
    print(f"\n// Racing|Obs")
    print(f"TrackHalfWidthCm = {track_half_width:.1f}")
    print(f"SpeedNormCmPerSec = {speed_norm:.1f}")
    print(f"CurvatureNormInvCm = {curvature_norm:.6f}")
    print(f"HeadingNormRad = {heading_norm:.2f}")
    print(f"AngVelNormDegPerSec = {ang_vel_norm:.1f}")
    print(f"LookaheadOffsetsCm = [{', '.join(f'{x:.0f}' for x in lookahead_offsets)}]")
    
    print(f"\n// Racing|Reward")
    print(f"RewardCfg.W_Lateral = {reward_weights['W_Lateral']:.2f}")
    print(f"RewardCfg.W_Heading = {reward_weights['W_Heading']:.2f}")
    print(f"RewardCfg.W_Speed = {reward_weights['W_Speed']:.2f}")
    print(f"RewardCfg.W_Progress = {reward_weights['W_Progress']:.2f}")
    
    print("\n" + "="*80)
    print("💡 HINWEISE")
    print("="*80)
    
    print("\n1. Diese Werte sind STARTWERTE - passe sie nach Bedarf an!")
    print("2. Teste das Training und beobachte das Agent-Verhalten")
    print("3. Falls Agents von der Strecke fliegen → Erhöhe W_Lateral und W_Heading")
    print("4. Falls Agents zu langsam fahren → Erhöhe W_Speed und W_Progress")
    print("5. Die Lookahead-Offsets sollten zur Streckenlänge passen")
    
    print("\n" + "="*80)

# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Empfiehlt passende Werte für RacingAgentComponent',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Beispiele:
  # Basis-Verwendung (interaktiv)
  python suggest_agent_parameters.py
  
  # Mit allen Parametern
  python suggest_agent_parameters.py --track-width 1000 --max-speed 50 --min-radius 5 --track-length 500
  
  # Nur Geschwindigkeit und Radius
  python suggest_agent_parameters.py --max-speed 80 --min-radius 8
        """
    )
    
    parser.add_argument('--track-width', type=float, dest='track_width_cm',
                        help='Streckenbreite in cm (z.B. 1000 für 10m)')
    parser.add_argument('--max-speed', type=float, dest='max_speed_kmh',
                        help='Maximale Auto-Geschwindigkeit in km/h (z.B. 50)')
    parser.add_argument('--max-speed-ms', type=float, dest='max_speed_ms',
                        help='Maximale Auto-Geschwindigkeit in m/s (Alternative zu --max-speed)')
    parser.add_argument('--min-radius', type=float, dest='min_radius_m',
                        help='Minimaler Kurvenradius in m (z.B. 5 für 5m Radius)')
    parser.add_argument('--track-length', type=float, dest='track_length_m',
                        help='Streckenlänge in m (z.B. 500 für 500m)')
    parser.add_argument('--track-type', type=str, choices=['standard', 'sharp', 'gentle'],
                        default='standard',
                        help='Streckentyp: standard (Standard), sharp (scharfe Kurven), gentle (sanfte Kurven)')
    parser.add_argument('--difficulty', type=str, choices=['easy', 'medium', 'hard', 'very_hard'],
                        default='medium',
                        help='Streckenschwierigkeit: easy, medium, hard, very_hard')
    
    args = parser.parse_args()
    
    # Interaktive Eingabe, falls keine Argumente
    if not any([args.track_width_cm, args.max_speed_kmh, args.max_speed_ms, 
                args.min_radius_m, args.track_length_m]):
        print("\n" + "="*80)
        print("RACING AGENT COMPONENT - PARAMETER-EMPFEHLUNGEN")
        print("="*80)
        print("\nKeine Parameter angegeben. Interaktive Eingabe:")
        print("(Drücke Enter, um Standard-Werte zu verwenden)\n")
        
        try:
            track_width_input = input("Streckenbreite in cm (z.B. 1000 für 10m): ").strip()
            args.track_width_cm = float(track_width_input) if track_width_input else None
            
            max_speed_input = input("Max. Geschwindigkeit in km/h (z.B. 50): ").strip()
            args.max_speed_kmh = float(max_speed_input) if max_speed_input else None
            
            min_radius_input = input("Min. Kurvenradius in m (z.B. 5): ").strip()
            args.min_radius_m = float(min_radius_input) if min_radius_input else None
            
            track_length_input = input("Streckenlänge in m (z.B. 500): ").strip()
            args.track_length_m = float(track_length_input) if track_length_input else None
            
            track_type_input = input("Streckentyp [standard/sharp/gentle] (Standard: standard): ").strip()
            if track_type_input:
                args.track_type = track_type_input
            
            difficulty_input = input("Schwierigkeit [easy/medium/hard/very_hard] (Standard: medium): ").strip()
            if difficulty_input:
                args.difficulty = difficulty_input
        except (ValueError, KeyboardInterrupt):
            print("\n❌ Ungültige Eingabe oder abgebrochen. Verwende Standard-Werte.")
            args.track_width_cm = None
            args.max_speed_kmh = None
            args.min_radius_m = None
            args.track_length_m = None
    
    print_recommendations(args)

if __name__ == "__main__":
    main()
