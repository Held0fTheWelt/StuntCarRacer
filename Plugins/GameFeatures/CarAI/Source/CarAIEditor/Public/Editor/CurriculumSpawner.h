#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "CurriculumSpawner.generated.h"

class USplineComponent;

USTRUCT(BlueprintType)
struct FCurriculumSpawnCandidate
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	float DistanceAlongSpline = 0.f;

	UPROPERTY(VisibleAnywhere)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere)
	FVector SurfaceNormal = FVector::UpVector;

	/** Rechts-Vektor für lateralen Offset */
	UPROPERTY(VisibleAnywhere)
	FVector RightVector = FVector::RightVector;

	UPROPERTY(VisibleAnywhere)
	float SpawnScore01 = 0.f;

	UPROPERTY(VisibleAnywhere)
	bool bValidSurface = false;
};

UCLASS()
class CARAIEDITOR_API UCurriculumSpawner : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Spawnt Autos entlang der Spline mit intelligenter Verteilung basierend auf Spawn-Scores (asynchron).
	 *
	 * @param World - Die Welt in der gespawnt wird
	 * @param Spline - Die Track-Spline
	 * @param PawnClass - Die Klasse der zu spawnenden Pawns
	 * @param NumCars - Anzahl der zu spawnenden Autos
	 * @param MinSpawnScore - Minimaler SpawnScore (0-1), Punkte darunter werden ignoriert
	 * @param SpawnHeightOffset - Höhenoffset über der Oberfläche
	 * @param bDistributeEvenly - Wenn true, verteilt Autos gleichmäßig; wenn false, bevorzugt hohe Scores
	 * @param bUseLateralOffset - Wenn true, werden Autos seitlich versetzt
	 * @param MaxLateralOffsetCm - Maximaler seitlicher Offset
	 * @param RandomSeed - Seed für Zufallsverteilung (0 = zufällig)
	 * @param bDebugDraw - Debug-Visualisierung
	 * @param OnComplete - Callback wenn Spawn abgeschlossen ist (wird auf Game-Thread aufgerufen)
	 */
	void SpawnCurriculumCarsAsync(
		UWorld* World,
		USplineComponent* Spline,
		TSubclassOf<APawn> PawnClass,
		int32 NumCars,
		TFunction<void(int32 SpawnedCount)> OnComplete = nullptr,
		float MinSpawnScore = 0.5f,
		float SpawnHeightOffset = 50.f,
		bool bDistributeEvenly = true,
		bool bUseLateralOffset = true,
		float MaxLateralOffsetCm = 300.f,
		int32 RandomSeed = 0,
		bool bDebugDraw = false
	);

	/**
	 * Synchron-Version (für Kompatibilität, blockiert!)
	 */
	void SpawnCurriculumCars(
		UWorld* World,
		USplineComponent* Spline,
		TSubclassOf<APawn> PawnClass,
		int32 NumCars,
		float MinSpawnScore = 0.5f,
		float SpawnHeightOffset = 50.f,
		bool bDistributeEvenly = true,
		bool bUseLateralOffset = true,
		float MaxLateralOffsetCm = 300.f,
		int32 RandomSeed = 0,
		bool bDebugDraw = false
	);

	/** Prüft ob ein Spawn gerade läuft */
	bool IsSpawnInProgress() const { return bSpawnInProgress; }

	// ----- Konfiguration für Spawn-Score Berechnung -----

	/** Sample-Abstand entlang der Spline in cm */
	UPROPERTY(EditAnywhere, Category = "Sampling")
	float SampleStepCm = 200.f;

	// Surface Trace
	UPROPERTY(EditAnywhere, Category = "Surface Trace")
	float TraceUpCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "Surface Trace")
	float TraceDownCm = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Surface Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// SpawnScore Parameter (wie in RacingCurriculumDebugActor)
	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float SurfaceNormalUpMin = 0.75f;

	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float PitchBadDeg = 8.f;

	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float PitchExponent = 1.4f;

	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float CurvatureWindowCm = 300.f;

	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float CurvatureBadInvCm = 1.f / 1200.f;

	UPROPERTY(EditAnywhere, Category = "SpawnScore")
	float CurvatureExponent = 1.6f;

private:
	/** Sammelt alle möglichen Spawn-Kandidaten entlang der Spline */
	void BuildSpawnCandidates(
		UWorld* World,
		USplineComponent* Spline,
		TArray<FCurriculumSpawnCandidate>& OutCandidates
	);

	/** Wählt die besten N Kandidaten aus, mit Mindestabstand */
	void SelectBestCandidates(
		const TArray<FCurriculumSpawnCandidate>& AllCandidates,
		int32 NumToSelect,
		float MinScore,
		float MinDistanceCm,
		bool bDistributeEvenly,
		TArray<FCurriculumSpawnCandidate>& OutSelected
	);

	/** Berechnet den SpawnScore für einen Punkt */
	float ComputeSpawnScore(
		bool bHasSurface,
		const FVector& SurfaceNormal,
		float CurvatureInvCm,
		float PitchDeg
	) const;

	/** Berechnet die Krümmung an einer Position */
	float ComputeCurvatureInvCm(
		USplineComponent* Spline,
		float DistanceCm,
		float WindowCm
	) const;

	/** Berechnet den Pitch-Winkel aus der Forward-Richtung */
	static float ComputePitchDegFromForward(const FVector& Forward);

	/** Surface Trace an einer Spline-Position */
	bool TraceSurface(
		UWorld* World,
		USplineComponent* Spline,
		float DistanceCm,
		FVector& OutLocation,
		FVector& OutNormal
	) const;

	/** Prüft ob ein Punkt in einer NoSpawnZone liegt */
	bool IsInAnyNoSpawnZone(UWorld* World, const FVector& WorldPoint) const;

	/** Führt BuildSpawnCandidates asynchron aus */
	void BuildSpawnCandidatesAsync(
		UWorld* World,
		USplineComponent* Spline,
		TFunction<void(TArray<FCurriculumSpawnCandidate>)> OnComplete
	);

	/** Thread-safe Flag für laufenden Spawn */
	FThreadSafeBool bSpawnInProgress = false;
};