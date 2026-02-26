#include "Editor/CurriculumSpawner.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Async/Async.h"
#include "EngineUtils.h"

// NoSpawnZone support
#include "Actors/NoSpawnZoneActor.h"

// ============================================================================
// Helpers
// ============================================================================

float UCurriculumSpawner::ComputePitchDegFromForward(const FVector& Forward)
{
	FVector F = Forward;
	if (!F.Normalize()) return 0.f;
	const float XY = FVector2D(F.X, F.Y).Size();
	return FMath::RadiansToDegrees(FMath::Atan2(F.Z, XY));
}

// ============================================================================
// Surface Trace
// ============================================================================

bool UCurriculumSpawner::TraceSurface(
	UWorld* World,
	USplineComponent* Spline,
	float DistanceCm,
	FVector& OutLocation,
	FVector& OutNormal
) const
{
	if (!World || !Spline) return false;

	const FVector SplinePos = Spline->GetLocationAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World);
	const FVector UpDir = Spline->GetUpVectorAtDistanceAlongSpline(DistanceCm, ESplineCoordinateSpace::World).GetSafeNormal();

	const FVector Start = SplinePos + UpDir * TraceUpCm;
	const FVector End = SplinePos - UpDir * TraceDownCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CurriculumSpawner), true);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	if (bHit)
	{
		OutLocation = Hit.ImpactPoint;
		OutNormal = Hit.ImpactNormal.GetSafeNormal();
		return true;
	}

	return false;
}

// ============================================================================
// Curvature
// ============================================================================

float UCurriculumSpawner::ComputeCurvatureInvCm(
	USplineComponent* Spline,
	float DistanceCm,
	float WindowCm
) const
{
	if (!Spline) return 0.f;

	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 1.f) return 0.f;

	const float W = FMath::Max(10.f, WindowCm);
	const bool bLoop = Spline->IsClosedLoop();

	auto WrapDist = [SplineLength, bLoop](float S) -> float
		{
			if (bLoop)
			{
				float X = FMath::Fmod(S, SplineLength);
				if (X < 0.f) X += SplineLength;
				return X;
			}
			return FMath::Clamp(S, 0.f, SplineLength);
		};

	const float S0 = WrapDist(DistanceCm - W);
	const float S1 = WrapDist(DistanceCm + W);

	FVector F0 = Spline->GetDirectionAtDistanceAlongSpline(S0, ESplineCoordinateSpace::World);
	FVector F1 = Spline->GetDirectionAtDistanceAlongSpline(S1, ESplineCoordinateSpace::World);

	if (!F0.Normalize()) F0 = FVector::ForwardVector;
	if (!F1.Normalize()) F1 = FVector::ForwardVector;

	const float Dot = FMath::Clamp(FVector::DotProduct(F0, F1), -1.f, 1.f);
	const float AngleRad = FMath::Acos(Dot);
	const float Ds = FMath::Max(1.f, 2.f * W);

	return AngleRad / Ds; // 1/cm
}

// ============================================================================
// Spawn Score (wie in RacingCurriculumDebugActor)
// ============================================================================

float UCurriculumSpawner::ComputeSpawnScore(
	bool bHasSurface,
	const FVector& SurfaceNormal,
	float CurvatureInvCm,
	float PitchDeg
) const
{
	if (!bHasSurface)
	{
		return -1.f;
	}

	// Check ob Oberfläche zu steil (Wand)
	const float UpDot = FVector::DotProduct(SurfaceNormal.GetSafeNormal(), FVector::UpVector);
	if (UpDot < SurfaceNormalUpMin)
	{
		return 0.05f;
	}

	// Pitch-Faktor (bergauf = schlecht, bergab = gut)
	const float PitchNorm = (PitchBadDeg > 0.f)
		? FMath::Clamp(PitchDeg / PitchBadDeg, -1.f, 1.f)
		: 0.f;

	float PitchFactor;
	if (PitchNorm > 0.f)
	{
		// Bergauf = schlecht
		PitchFactor = 1.f - PitchNorm;
	}
	else
	{
		// Bergab = gut
		PitchFactor = 1.f;
	}

	// Curvature-Faktor (hohe Krümmung = schlecht)
	const float CurvNorm = (CurvatureBadInvCm > 0.f)
		? FMath::Clamp(CurvatureInvCm / CurvatureBadInvCm, 0.f, 1.f)
		: 0.f;

	const float CurvFactor = 1.f - CurvNorm;

	return FMath::Clamp(
		FMath::Pow(CurvFactor, CurvatureExponent) *
		FMath::Pow(PitchFactor, PitchExponent),
		0.05f, 1.f
	);
}

// ============================================================================
// Build Spawn Candidates
// ============================================================================

void UCurriculumSpawner::BuildSpawnCandidates(
	UWorld* World,
	USplineComponent* Spline,
	TArray<FCurriculumSpawnCandidate>& OutCandidates
)
{
	OutCandidates.Reset();

	if (!World || !Spline) return;

	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 1.f) return;

	const float Step = FMath::Max(50.f, SampleStepCm);
	const int32 NumSamples = FMath::CeilToInt(SplineLength / Step);

	OutCandidates.Reserve(NumSamples);

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float S = i * Step;

		FCurriculumSpawnCandidate Candidate;
		Candidate.DistanceAlongSpline = S;

		// Surface Trace
		FVector SurfaceLocation, SurfaceNormal;
		const bool bHasSurface = TraceSurface(World, Spline, S, SurfaceLocation, SurfaceNormal);

		Candidate.bValidSurface = bHasSurface;

		if (bHasSurface)
		{
			Candidate.Location = SurfaceLocation;
			Candidate.SurfaceNormal = SurfaceNormal;
		}
		else
		{
			// Fallback auf Spline-Position
			Candidate.Location = Spline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
			Candidate.SurfaceNormal = FVector::UpVector;
		}

		// Rotation und RightVector aus Spline
		const FVector Tangent = Spline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
		Candidate.Rotation = Tangent.Rotation();
		Candidate.RightVector = Spline->GetRightVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);

		// SpawnScore berechnen
		const float CurvInvCm = ComputeCurvatureInvCm(Spline, S, CurvatureWindowCm);
		const float PitchDeg = ComputePitchDegFromForward(Tangent);

		float SpawnScore = ComputeSpawnScore(bHasSurface, Candidate.SurfaceNormal, CurvInvCm, PitchDeg);
		
		// Setze Score auf 0, wenn in NoSpawnZone
		if (SpawnScore >= 0.f && IsInAnyNoSpawnZone(World, Candidate.Location))
		{
			SpawnScore = 0.f; // Komplett verboten
		}
		
		Candidate.SpawnScore01 = SpawnScore;

		OutCandidates.Add(Candidate);
	}
}

// ============================================================================
// NoSpawnZone Helper
// ============================================================================

bool UCurriculumSpawner::IsInAnyNoSpawnZone(UWorld* World, const FVector& WorldPoint) const
{
	if (!World)
	{
		return false;
	}

	for (TActorIterator<ANoSpawnZoneActor> It(World); It; ++It)
	{
		ANoSpawnZoneActor* Zone = *It;
		if (!IsValid(Zone))
		{
			continue;
		}

		// Use the ContainsPoint method from NoSpawnZoneActor
		if (Zone->ContainsPoint(WorldPoint))
		{
			return true;
		}
	}

	return false;
}

// ============================================================================
// Select Best Candidates (vereinfacht - kein Mindestabstand mehr)
// ============================================================================

void UCurriculumSpawner::SelectBestCandidates(
	const TArray<FCurriculumSpawnCandidate>& AllCandidates,
	int32 NumToSelect,
	float MinScore,
	float MinDistanceCm, // wird ignoriert - für Kompatibilität behalten
	bool bDistributeEvenly,
	TArray<FCurriculumSpawnCandidate>& OutSelected
)
{
	(void)MinDistanceCm; // Nicht mehr verwendet - Autos sind Geister

	OutSelected.Reset();

	if (AllCandidates.Num() == 0 || NumToSelect <= 0) return;

	// Filtere Kandidaten mit ausreichendem Score
	TArray<FCurriculumSpawnCandidate> ValidCandidates;
	for (const FCurriculumSpawnCandidate& C : AllCandidates)
	{
		if (C.bValidSurface && C.SpawnScore01 >= MinScore)
		{
			ValidCandidates.Add(C);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d von %d Kandidaten haben Score >= %.2f"),
		ValidCandidates.Num(), AllCandidates.Num(), MinScore);

	if (ValidCandidates.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CurriculumSpawner: Keine Kandidaten mit Score >= %.2f gefunden! Fallback..."), MinScore);

		// Fallback: nimm alle mit validem Surface
		for (const FCurriculumSpawnCandidate& C : AllCandidates)
		{
			if (C.bValidSurface && C.SpawnScore01 > 0.f)
			{
				ValidCandidates.Add(C);
			}
		}

		if (ValidCandidates.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("CurriculumSpawner: Überhaupt keine validen Spawn-Punkte gefunden!"));
			return;
		}
	}

	const float FirstDist = ValidCandidates[0].DistanceAlongSpline;
	const float LastDist = ValidCandidates.Last().DistanceAlongSpline;
	const float TotalLength = LastDist - FirstDist;

	if (bDistributeEvenly)
	{
		// Gleichmäßige Verteilung: Wähle Kandidaten in regelmäßigen Abständen
		const float Spacing = TotalLength / FMath::Max(1, NumToSelect);

		UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: Gleichmäßige Verteilung - %.0f m Strecke, %d Autos, %.0f cm Abstand"),
			TotalLength / 100.f, NumToSelect, Spacing);

		for (int32 i = 0; i < NumToSelect; ++i)
		{
			const float TargetDist = FirstDist + i * Spacing;

			// Finde nächsten Kandidaten zur Zieldistanz
			const FCurriculumSpawnCandidate* Best = nullptr;
			float BestDelta = FLT_MAX;

			for (const FCurriculumSpawnCandidate& C : ValidCandidates)
			{
				const float Delta = FMath::Abs(C.DistanceAlongSpline - TargetDist);
				if (Delta < BestDelta)
				{
					BestDelta = Delta;
					Best = &C;
				}
			}

			if (Best)
			{
				OutSelected.Add(*Best);
			}
		}
	}
	else
	{
		// Score-basierte Auswahl: Sortiere nach Score und nimm die besten
		TArray<FCurriculumSpawnCandidate> Sorted = ValidCandidates;
		Sorted.Sort([](const FCurriculumSpawnCandidate& A, const FCurriculumSpawnCandidate& B)
			{
				return A.SpawnScore01 > B.SpawnScore01;
			});

		const int32 Count = FMath::Min(NumToSelect, Sorted.Num());
		for (int32 i = 0; i < Count; ++i)
		{
			OutSelected.Add(Sorted[i]);
		}
	}

	// Sortiere Ergebnis nach Distanz für konsistente Reihenfolge
	OutSelected.Sort([](const FCurriculumSpawnCandidate& A, const FCurriculumSpawnCandidate& B)
		{
			return A.DistanceAlongSpline < B.DistanceAlongSpline;
		});

	UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d Kandidaten ausgewählt"), OutSelected.Num());
}

// ============================================================================
// Main Spawn Function
// ============================================================================

void UCurriculumSpawner::SpawnCurriculumCars(
	UWorld* World,
	USplineComponent* Spline,
	TSubclassOf<APawn> PawnClass,
	int32 NumCars,
	float MinSpawnScore,
	float SpawnHeightOffset,
	bool bDistributeEvenly,
	bool bUseLateralOffset,
	float MaxLateralOffsetCm,
	int32 RandomSeed,
	bool bDebugDraw
)
{
	if (!World || !Spline || !PawnClass || NumCars <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CurriculumSpawner: Ungültige Parameter"));
		return;
	}

	// Random Stream initialisieren
	FRandomStream Rng;
	if (RandomSeed != 0)
	{
		Rng.Initialize(RandomSeed);
	}
	else
	{
		Rng.GenerateNewSeed();
	}

	// 1. Sammle alle Spawn-Kandidaten
	TArray<FCurriculumSpawnCandidate> AllCandidates;
	BuildSpawnCandidates(World, Spline, AllCandidates);

	UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d Spawn-Kandidaten gefunden"), AllCandidates.Num());

	// 2. Wähle die Kandidaten aus
	TArray<FCurriculumSpawnCandidate> SelectedCandidates;
	SelectBestCandidates(
		AllCandidates,
		NumCars,
		MinSpawnScore,
		0.f, // Kein Mindestabstand mehr
		bDistributeEvenly,
		SelectedCandidates
	);

	// 3. Spawne die Autos
	int32 SpawnedCount = 0;
	for (const FCurriculumSpawnCandidate& Candidate : SelectedCandidates)
	{
		// Basis-Position
		FVector SpawnLocation = Candidate.Location;

		// Lateralen Offset anwenden
		float LateralOffset = 0.f;
		if (bUseLateralOffset && MaxLateralOffsetCm > 0.f)
		{
			// Zufälliger Offset zwischen -Max und +Max
			LateralOffset = Rng.FRandRange(-MaxLateralOffsetCm, MaxLateralOffsetCm);
			SpawnLocation += Candidate.RightVector * LateralOffset;
		}

		// Höhenoffset
		SpawnLocation += Candidate.SurfaceNormal * SpawnHeightOffset;

		const FRotator SpawnRotation = Candidate.Rotation;

		// Debug Draw
		if (bDebugDraw)
		{
			// Score-basierte Farbe
			FColor DebugColor;
			if (Candidate.SpawnScore01 >= 0.8f)
				DebugColor = FColor::Green;
			else if (Candidate.SpawnScore01 >= 0.55f)
				DebugColor = FColor::Yellow;
			else if (Candidate.SpawnScore01 >= 0.25f)
				DebugColor = FColor::Orange;
			else
				DebugColor = FColor::Red;

			// Sphere an Spawn-Position
			DrawDebugSphere(World, SpawnLocation, 60.f, 12, DebugColor, false, 15.f);

			// Pfeil für Richtung
			DrawDebugDirectionalArrow(
				World,
				SpawnLocation,
				SpawnLocation + Candidate.Rotation.Vector() * 200.f,
				40.f,
				FColor::White,
				false,
				15.f
			);

			// Linie von Mittellinie zur Spawn-Position (zeigt lateralen Offset)
			if (bUseLateralOffset && FMath::Abs(LateralOffset) > 1.f)
			{
				const FVector CenterPos = Candidate.Location + Candidate.SurfaceNormal * SpawnHeightOffset;
				DrawDebugLine(World, CenterPos, SpawnLocation, FColor::Cyan, false, 15.f, 0, 2.f);
			}

			// Label
			DrawDebugString(
				World,
				SpawnLocation + FVector(0, 0, 100.f),
				FString::Printf(TEXT("S:%.2f L:%.0f"), Candidate.SpawnScore01, LateralOffset),
				nullptr,
				FColor::White,
				15.f,
				false
			);
		}

		// Spawn
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		APawn* Pawn = World->SpawnActor<APawn>(
			PawnClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParams
		);

		if (Pawn)
		{
			Pawn->Tags.Add(TEXT("CurriculumCar"));
			
			// Setze Actor in "AICars" Folder im Outliner
			#if WITH_EDITOR
			if (GIsEditor)
			{
				Pawn->SetFolderPath(FName(TEXT("AICars")));
			}
			#endif
			
			SpawnedCount++;

			UE_LOG(LogTemp, Verbose, TEXT("CurriculumSpawner: Auto #%d bei S=%.0f m, Lateral=%.0f cm"),
				SpawnedCount, Candidate.DistanceAlongSpline / 100.f, LateralOffset);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d Autos erfolgreich gespawnt (Lateral: %s, MaxOffset: %.0f cm)"),
		SpawnedCount, bUseLateralOffset ? TEXT("Ja") : TEXT("Nein"), MaxLateralOffsetCm);
}

// ============================================================================
// Async Build Spawn Candidates
// ============================================================================

void UCurriculumSpawner::BuildSpawnCandidatesAsync(
	UWorld* World,
	USplineComponent* Spline,
	TFunction<void(TArray<FCurriculumSpawnCandidate>)> OnComplete
)
{
	if (!World || !Spline)
	{
		if (OnComplete)
		{
			AsyncTask(ENamedThreads::GameThread, [OnComplete]()
			{
				OnComplete(TArray<FCurriculumSpawnCandidate>());
			});
		}
		return;
	}

	// Berechne alle Distanzen vorher (schnell)
	const float SplineLength = Spline->GetSplineLength();
	if (SplineLength <= 1.f)
	{
		if (OnComplete)
		{
			OnComplete(TArray<FCurriculumSpawnCandidate>());
		}
		return;
	}

	const float Step = FMath::Max(50.f, SampleStepCm);
	const int32 NumSamples = FMath::CeilToInt(SplineLength / Step);
	
	// Verwende einen Timer-basierten Ansatz für chunked processing
	// Das ist einfacher und sicherer als Threading
	struct FChunkedBuilder
	{
		UWorld* World;
		USplineComponent* Spline;
		float Step;
		int32 NumSamples;
		int32 CurrentIndex;
		TArray<FCurriculumSpawnCandidate> Candidates;
		TFunction<void(TArray<FCurriculumSpawnCandidate>)> OnComplete;
		UCurriculumSpawner* Spawner;
		
		void ProcessChunk()
		{
			const int32 ChunkSize = 10; // Verarbeite 10 Samples pro Frame
			const int32 EndIndex = FMath::Min(CurrentIndex + ChunkSize, NumSamples);
			
			for (int32 i = CurrentIndex; i < EndIndex; ++i)
			{
				const float S = i * Step;
				
				FCurriculumSpawnCandidate Candidate;
				Candidate.DistanceAlongSpline = S;
				
				// Surface Trace
				FVector SurfaceLocation, SurfaceNormal;
				const bool bHasSurface = Spawner->TraceSurface(World, Spline, S, SurfaceLocation, SurfaceNormal);
				
				Candidate.bValidSurface = bHasSurface;
				if (bHasSurface)
				{
					Candidate.Location = SurfaceLocation;
					Candidate.SurfaceNormal = SurfaceNormal;
				}
				else
				{
					Candidate.Location = Spline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
					Candidate.SurfaceNormal = FVector::UpVector;
				}
				
				// Rotation und RightVector
				const FVector Tangent = Spline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
				Candidate.Rotation = Tangent.Rotation();
				Candidate.RightVector = Spline->GetRightVectorAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
				
				// SpawnScore
				const float CurvInvCm = Spawner->ComputeCurvatureInvCm(Spline, S, Spawner->CurvatureWindowCm);
				const float PitchDeg = ComputePitchDegFromForward(Tangent);
				Candidate.SpawnScore01 = Spawner->ComputeSpawnScore(bHasSurface, Candidate.SurfaceNormal, CurvInvCm, PitchDeg);
				
				Candidates.Add(Candidate);
			}
			
			CurrentIndex = EndIndex;
			
			if (CurrentIndex >= NumSamples)
			{
				// Fertig!
				if (OnComplete)
				{
					OnComplete(Candidates);
				}
				delete this; // Cleanup
			}
			else
			{
				// Nächster Chunk im nächsten Frame
				AsyncTask(ENamedThreads::GameThread, [this]()
				{
					ProcessChunk();
				});
			}
		}
	};
	
	FChunkedBuilder* Builder = new FChunkedBuilder();
	Builder->World = World;
	Builder->Spline = Spline;
	Builder->Step = Step;
	Builder->NumSamples = NumSamples;
	Builder->CurrentIndex = 0;
	Builder->Candidates.Reserve(NumSamples);
	Builder->OnComplete = OnComplete;
	Builder->Spawner = this;
	
	// Starte ersten Chunk
	AsyncTask(ENamedThreads::GameThread, [Builder]()
	{
		Builder->ProcessChunk();
	});
}

// ============================================================================
// Async Spawn Function
// ============================================================================

void UCurriculumSpawner::SpawnCurriculumCarsAsync(
	UWorld* World,
	USplineComponent* Spline,
	TSubclassOf<APawn> PawnClass,
	int32 NumCars,
	TFunction<void(int32 SpawnedCount)> OnComplete,
	float MinSpawnScore,
	float SpawnHeightOffset,
	bool bDistributeEvenly,
	bool bUseLateralOffset,
	float MaxLateralOffsetCm,
	int32 RandomSeed,
	bool bDebugDraw
)
{
	if (!World || !Spline || !PawnClass || NumCars <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CurriculumSpawner: Ungültige Parameter"));
		if (OnComplete)
		{
			OnComplete(0);
		}
		return;
	}

	if (bSpawnInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("CurriculumSpawner: Spawn bereits in Progress!"));
		if (OnComplete)
		{
			OnComplete(0);
		}
		return;
	}

	bSpawnInProgress = true;

	// Random Stream initialisieren
	FRandomStream Rng;
	if (RandomSeed != 0)
	{
		Rng.Initialize(RandomSeed);
	}
	else
	{
		Rng.GenerateNewSeed();
	}

	// Kopiere Parameter für Lambda
	const float MinScore = MinSpawnScore;
	const float HeightOffset = SpawnHeightOffset;
	const bool bDistEven = bDistributeEvenly;
	const bool bUseLat = bUseLateralOffset;
	const float MaxLatOffset = MaxLateralOffsetCm;
	const bool bDebug = bDebugDraw;

	// 1. Sammle alle Spawn-Kandidaten asynchron
	BuildSpawnCandidatesAsync(World, Spline, [this, World, PawnClass, NumCars, Rng, MinScore, HeightOffset,
		bDistEven, bUseLat, MaxLatOffset, bDebug, OnComplete](TArray<FCurriculumSpawnCandidate> AllCandidates) mutable
	{
		UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d Spawn-Kandidaten gefunden (async)"), AllCandidates.Num());

		// 2. Wähle die Kandidaten aus
		TArray<FCurriculumSpawnCandidate> SelectedCandidates;
		SelectBestCandidates(
			AllCandidates,
			NumCars,
			MinScore,
			0.f,
			bDistEven,
			SelectedCandidates
		);

		// 3. Spawne die Autos (muss auf Game-Thread sein)
		int32 SpawnedCount = 0;
		for (const FCurriculumSpawnCandidate& Candidate : SelectedCandidates)
		{
			FVector SpawnLocation = Candidate.Location;
			float LateralOffset = 0.f;
			if (bUseLat && MaxLatOffset > 0.f)
			{
				LateralOffset = Rng.FRandRange(-MaxLatOffset, MaxLatOffset);
				SpawnLocation += Candidate.RightVector * LateralOffset;
			}
			SpawnLocation += Candidate.SurfaceNormal * HeightOffset;
			const FRotator SpawnRotation = Candidate.Rotation;

			if (bDebug)
			{
				FColor DebugColor;
				if (Candidate.SpawnScore01 >= 0.8f)
					DebugColor = FColor::Green;
				else if (Candidate.SpawnScore01 >= 0.55f)
					DebugColor = FColor::Yellow;
				else if (Candidate.SpawnScore01 >= 0.25f)
					DebugColor = FColor::Orange;
				else
					DebugColor = FColor::Red;

				DrawDebugSphere(World, SpawnLocation, 60.f, 12, DebugColor, false, 15.f);
				DrawDebugDirectionalArrow(World, SpawnLocation, SpawnLocation + Candidate.Rotation.Vector() * 200.f,
					40.f, FColor::White, false, 15.f);
				if (bUseLat && FMath::Abs(LateralOffset) > 1.f)
				{
					const FVector CenterPos = Candidate.Location + Candidate.SurfaceNormal * HeightOffset;
					DrawDebugLine(World, CenterPos, SpawnLocation, FColor::Cyan, false, 15.f, 0, 2.f);
				}
				DrawDebugString(World, SpawnLocation + FVector(0, 0, 100.f),
					FString::Printf(TEXT("S:%.2f L:%.0f"), Candidate.SpawnScore01, LateralOffset),
					nullptr, FColor::White, 15.f, false);
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			APawn* Pawn = World->SpawnActor<APawn>(PawnClass, SpawnLocation, SpawnRotation, SpawnParams);

			if (Pawn)
			{
				Pawn->Tags.Add(TEXT("CurriculumCar"));
				#if WITH_EDITOR
				if (GIsEditor)
				{
					Pawn->SetFolderPath(FName(TEXT("AICars")));
				}
				#endif
				SpawnedCount++;
			}
		}

		bSpawnInProgress = false;

		UE_LOG(LogTemp, Log, TEXT("CurriculumSpawner: %d Autos erfolgreich gespawnt (async)"), SpawnedCount);

		if (OnComplete)
		{
			OnComplete(SpawnedCount);
		}
	});
}