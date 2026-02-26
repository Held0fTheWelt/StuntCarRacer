#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "CurriculumEditorWidget.generated.h"

class USplineComponent;

UCLASS()
class CARAIEDITOR_API UCurriculumEditorWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:

	/* ---------- Status ---------- */

	UPROPERTY(VisibleAnywhere, Category = "Status")
	TObjectPtr<AActor> CachedTrackActor;

	UPROPERTY(VisibleAnywhere, Category = "Status")
	FString LastStatus = TEXT("Ready");

	UPROPERTY(VisibleAnywhere, Category = "Status")
	int32 SpawnedCarCount = 0;

	/* ---------- Curriculum Setup ---------- */

	UPROPERTY(EditAnywhere, Category = "Curriculum")
	TSubclassOf<APawn> CarPawnClass;

	UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = 1))
	int32 NumCars = 5;

	/* ---------- Spawn Distribution ---------- */

	/** Minimaler SpawnScore (0-1). Punkte mit niedrigerem Score werden ignoriert. */
	UPROPERTY(EditAnywhere, Category = "Spawn Distribution", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MinSpawnScore = 0.5f;

	/** Wenn true, werden Autos gleichmäßig verteilt. Wenn false, werden hohe Scores bevorzugt. */
	UPROPERTY(EditAnywhere, Category = "Spawn Distribution")
	bool bDistributeEvenly = true;

	/** Höhenoffset über der Oberfläche beim Spawnen */
	UPROPERTY(EditAnywhere, Category = "Spawn Distribution", meta = (ClampMin = 0.0))
	float SpawnHeightOffsetCm = 50.f;

	/* ---------- Lateral Offset (Seitliche Versetzung) ---------- */

	/** Wenn true, werden Autos zufällig seitlich versetzt */
	UPROPERTY(EditAnywhere, Category = "Lateral Offset")
	bool bUseLateralOffset = true;

	/** Maximaler seitlicher Offset in cm (links/rechts von der Mittellinie) */
	UPROPERTY(EditAnywhere, Category = "Lateral Offset", meta = (ClampMin = 0.0))
	float MaxLateralOffsetCm = 300.f;

	/** Seed für reproduzierbare Zufallsverteilung (0 = zufällig) */
	UPROPERTY(EditAnywhere, Category = "Lateral Offset")
	int32 RandomSeed = 0;

	/* ---------- Sampling ---------- */

	/** Sample-Abstand entlang der Spline für Spawn-Kandidaten */
	UPROPERTY(EditAnywhere, Category = "Sampling", meta = (ClampMin = 50.0))
	float SampleStepCm = 200.f;

	/* ---------- Surface Trace ---------- */

	UPROPERTY(EditAnywhere, Category = "Surface Trace")
	float TraceUpCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "Surface Trace")
	float TraceDownCm = 1500.f;

	/* ---------- SpawnScore Tuning ---------- */

	/** Minimum Up-Dot für gültige Oberflächen (0.75 = max 41° Neigung) */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float SurfaceNormalUpMin = 0.75f;

	/** Ab diesem Pitch-Winkel (bergauf) sinkt der Score */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 1.0, ClampMax = 45.0))
	float PitchBadDeg = 8.f;

	/** Exponent für Pitch-Penalty */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 0.5, ClampMax = 3.0))
	float PitchExponent = 1.4f;

	/** Fenster für Krümmungsberechnung */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 50.0))
	float CurvatureWindowCm = 300.f;

	/** Krümmung ab der der Score sinkt (1/cm). Kleinere Werte = empfindlicher */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 0.0001, ClampMax = 0.01))
	float CurvatureBadInvCm = 0.000833f; // ~1/1200cm = Radius 12m

	/** Exponent für Krümmungs-Penalty */
	UPROPERTY(EditAnywhere, Category = "SpawnScore Tuning", meta = (ClampMin = 0.5, ClampMax = 3.0))
	float CurvatureExponent = 1.6f;

	/* ---------- Debug ---------- */

	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugDraw = true;

	/* ---------- Actions ---------- */

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Actions")
	void FindTrack();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Actions")
	void SpawnCurriculumCars();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Actions")
	void ClearAllCars();

	/* ---------- Debug Actions ---------- */

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Debug Actions")
	void DebugListAllActors();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Debug Actions")
	void DebugShowSpawnCandidates();

private:
	UWorld* GetEditorWorld() const;
	USplineComponent* FindTrackSpline();
};