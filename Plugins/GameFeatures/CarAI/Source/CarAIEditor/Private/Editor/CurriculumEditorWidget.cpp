#include "Editor/CurriculumEditorWidget.h"
#include "Editor/CurriculumSpawner.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// ============================================================================
// Helper
// ============================================================================

UWorld* UCurriculumEditorWidget::GetEditorWorld() const
{
	if (!GEditor)
	{
		return nullptr;
	}

	return GEditor->GetEditorWorldContext().World();
}

// ============================================================================
// Find Track Spline
// ============================================================================

USplineComponent* UCurriculumEditorWidget::FindTrackSpline()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("FindTrackSpline: No editor world!"));
		return nullptr;
	}

	CachedTrackActor = nullptr;

	AActor* FallbackActor = nullptr;
	USplineComponent* FallbackSpline = nullptr;

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;

		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor)) continue;

			// Suche SplineComponent mit Name "TrackSpline"
			USplineComponent* Spline = nullptr;

			TArray<USplineComponent*> Splines;
			Actor->GetComponents<USplineComponent>(Splines);

			for (USplineComponent* S : Splines)
			{
				if (S && S->GetName().Contains(TEXT("TrackSpline")))
				{
					Spline = S;
					break;
				}
			}

			// Fallback: erste Spline nehmen
			if (!Spline && Splines.Num() > 0)
			{
				Spline = Splines[0];
			}

			if (!Spline) continue;

			bool bHasTag = Actor->ActorHasTag(FName("Track"));

			UE_LOG(LogTemp, Log, TEXT("FindTrackSpline: Found %s (Tag: %s, SplineLen: %.0f)"),
				*Actor->GetName(), bHasTag ? TEXT("Y") : TEXT("N"), Spline->GetSplineLength());

			if (bHasTag)
			{
				CachedTrackActor = Actor;
				return Spline;
			}

			if (!FallbackActor)
			{
				FallbackActor = Actor;
				FallbackSpline = Spline;
			}
		}
	}

	if (FallbackActor && FallbackSpline)
	{
		CachedTrackActor = FallbackActor;
		UE_LOG(LogTemp, Log, TEXT("FindTrackSpline: Using fallback %s"), *FallbackActor->GetName());
		return FallbackSpline;
	}

	UE_LOG(LogTemp, Error, TEXT("FindTrackSpline: Nothing found!"));
	return nullptr;
}

// ============================================================================
// Actions
// ============================================================================

void UCurriculumEditorWidget::FindTrack()
{
	CachedTrackActor = nullptr;

	USplineComponent* Spline = FindTrackSpline();
	if (Spline && CachedTrackActor)
	{
		LastStatus = FString::Printf(TEXT("Found: %s (Spline: %.0f m)"),
			*CachedTrackActor->GetName(),
			Spline->GetSplineLength() / 100.f);
	}
	else
	{
		LastStatus = TEXT("No track found!");
	}
}

void UCurriculumEditorWidget::SpawnCurriculumCars()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		LastStatus = TEXT("ERROR: No editor world!");
		return;
	}

	USplineComponent* Spline = FindTrackSpline();
	if (!Spline)
	{
		LastStatus = TEXT("ERROR: No track found!");
		return;
	}

	if (!CarPawnClass)
	{
		LastStatus = TEXT("ERROR: No CarPawnClass set!");
		return;
	}

	// Clear existing first
	ClearAllCars();

	// Erstelle Spawner und konfiguriere ihn
	UCurriculumSpawner* Spawner = NewObject<UCurriculumSpawner>(this);

	// Übertrage Einstellungen
	Spawner->SampleStepCm = SampleStepCm;
	Spawner->TraceUpCm = TraceUpCm;
	Spawner->TraceDownCm = TraceDownCm;
	Spawner->SurfaceNormalUpMin = SurfaceNormalUpMin;
	Spawner->PitchBadDeg = PitchBadDeg;
	Spawner->PitchExponent = PitchExponent;
	Spawner->CurvatureWindowCm = CurvatureWindowCm;
	Spawner->CurvatureBadInvCm = CurvatureBadInvCm;
	Spawner->CurvatureExponent = CurvatureExponent;

	// Spawne mit lateralem Offset (asynchron)
	LastStatus = TEXT("Spawning cars (async)...");
	
	// Kopiere Member-Variablen für Lambda-Capture
	TSubclassOf<APawn> LocalPawnClass = CarPawnClass;
	
	Spawner->SpawnCurriculumCarsAsync(
		World,
		Spline,
		LocalPawnClass,
		NumCars,
		[this, LocalPawnClass](int32 SpawnedCount)
		{
			// Callback auf Game-Thread
			SpawnedCarCount = SpawnedCount;
			LastStatus = FString::Printf(TEXT("Spawned %d cars (async)"), SpawnedCount);
		},
		MinSpawnScore,
		SpawnHeightOffsetCm,
		bDistributeEvenly,
		bUseLateralOffset,
		MaxLateralOffsetCm,
		RandomSeed,
		bDebugDraw
	);

	LastStatus = FString::Printf(TEXT("Spawned %d cars (Lateral: %s, MaxOffset: %.0f cm)"),
		SpawnedCarCount,
		bUseLateralOffset ? TEXT("On") : TEXT("Off"),
		MaxLateralOffsetCm);
}

void UCurriculumEditorWidget::ClearAllCars()
{
	UWorld* World = GetEditorWorld();
	if (!World || !CarPawnClass)
	{
		LastStatus = TEXT("ERROR: No world or CarPawnClass!");
		return;
	}

	TArray<AActor*> Found;
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;
		for (AActor* Actor : Level->Actors)
		{
			if (IsValid(Actor) && Actor->IsA(CarPawnClass))
			{
				Found.Add(Actor);
			}
		}
	}

	int32 Destroyed = 0;
	for (AActor* Actor : Found)
	{
		if (Actor)
		{
			Actor->Destroy();
			Destroyed++;
		}
	}

	SpawnedCarCount = 0;
	LastStatus = FString::Printf(TEXT("Cleared %d cars"), Destroyed);
}

// ============================================================================
// Debug Actions
// ============================================================================

void UCurriculumEditorWidget::DebugListAllActors()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("DEBUG: No editor world!"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("==========================================="));
	UE_LOG(LogTemp, Warning, TEXT("DEBUG: Scanning all actors in %s"), *World->GetName());
	UE_LOG(LogTemp, Warning, TEXT("==========================================="));

	int32 Total = 0;
	int32 WithSpline = 0;

	for (ULevel* Level : World->GetLevels())
	{
		if (!Level) continue;

		for (AActor* Actor : Level->Actors)
		{
			if (!IsValid(Actor)) continue;
			Total++;

			USplineComponent* Spline = Actor->FindComponentByClass<USplineComponent>();
			if (!Spline) continue;

			WithSpline++;

			bool bHasTag = Actor->ActorHasTag(FName("Track"));

			UE_LOG(LogTemp, Warning, TEXT("  [SPLINE] %s"), *Actor->GetName());
			UE_LOG(LogTemp, Warning, TEXT("           Class: %s"), *Actor->GetClass()->GetName());
			UE_LOG(LogTemp, Warning, TEXT("           Tag 'Track': %s"), bHasTag ? TEXT("YES") : TEXT("NO"));
			UE_LOG(LogTemp, Warning, TEXT("           SplineLength: %.0f cm (%.0f m)"), Spline->GetSplineLength(), Spline->GetSplineLength() / 100.f);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("==========================================="));
	UE_LOG(LogTemp, Warning, TEXT("Total: %d actors, %d with SplineComponent"), Total, WithSpline);
	UE_LOG(LogTemp, Warning, TEXT("==========================================="));

	LastStatus = FString::Printf(TEXT("Debug: %d actors, %d with spline - check Output Log!"), Total, WithSpline);
}

void UCurriculumEditorWidget::DebugShowSpawnCandidates()
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		LastStatus = TEXT("ERROR: No editor world!");
		return;
	}

	USplineComponent* Spline = FindTrackSpline();
	if (!Spline)
	{
		LastStatus = TEXT("ERROR: No track found!");
		return;
	}

	const float SplineLength = Spline->GetSplineLength();
	const float Step = FMath::Max(50.f, SampleStepCm);
	const int32 NumSamples = FMath::CeilToInt(SplineLength / Step);

	int32 ValidCount = 0;
	int32 GoodCount = 0;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float S = i * Step;

		const FVector SplinePos = Spline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
		const FVector UpDir = Spline->GetUpVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal();

		const FVector Start = SplinePos + UpDir * TraceUpCm;
		const FVector End = SplinePos - UpDir * TraceDownCm;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility);

		FVector DrawPos = bHit ? Hit.ImpactPoint : SplinePos;
		DrawPos += FVector(0, 0, 20.f);

		// Einfache Score-Berechnung für Visualisierung
		float Score = 0.f;
		if (bHit)
		{
			const float UpDot = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector);
			if (UpDot >= SurfaceNormalUpMin)
			{
				Score = UpDot;
				ValidCount++;
				if (Score >= MinSpawnScore)
				{
					GoodCount++;
				}
			}
		}

		// Farbe basierend auf Score
		FColor Color;
		if (!bHit)
			Color = FColor::Black;
		else if (Score >= 0.8f)
			Color = FColor::Green;
		else if (Score >= 0.55f)
			Color = FColor::Yellow;
		else if (Score >= 0.25f)
			Color = FColor::Orange;
		else
			Color = FColor::Red;

		DrawDebugSphere(World, DrawPos, 30.f, 8, Color, false, 10.f);
	}

	LastStatus = FString::Printf(TEXT("Candidates: %d total, %d valid, %d good (Score >= %.2f)"),
		NumSamples, ValidCount, GoodCount, MinSpawnScore);
}