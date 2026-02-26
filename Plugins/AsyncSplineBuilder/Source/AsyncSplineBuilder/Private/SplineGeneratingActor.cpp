// Yves Tanas 2025 All Rights Reserved.

#include "SplineGeneratingActor.h"

#include "ProceduralMeshComponent.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/TextRenderComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

#include "LandscapeProxy.h"
#include "LandscapeLayerInfoObject.h"

#include "SplinePointListAsset.h"

#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY(LogAsyncSplineBuilder);

/**
 * Helper macro for consistent async spline builder logging.
 */
#define ASYNC_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAsyncSplineBuilder, Verbosity, TEXT("[AsyncSpline] %s: " Format), *GetName(), ##__VA_ARGS__)

static int32 WrapPointIndex(int32 Index, int32 NumPoints)
{
	if (NumPoints <= 0) return 0;
	Index %= NumPoints;
	if (Index < 0) Index += NumPoints;
	return Index;
}

// ============================================================================
// Local conversion helpers
// ============================================================================

FVector ASplineGeneratingActor::WorldToActorLocalPos(const FVector& WorldPos) const
{
	return GetActorTransform().InverseTransformPosition(WorldPos);
}

FVector ASplineGeneratingActor::WorldToActorLocalDir(const FVector& WorldDir) const
{
	return GetActorTransform().InverseTransformVectorNoScale(WorldDir);
}

// ============================================================================
// Trace ignore + filtering helpers
// ============================================================================

void ASplineGeneratingActor::BuildTraceIgnoreActors(TArray<AActor*>& OutIgnore) const
{
	OutIgnore.Reset();

	// Always ignore self (includes most components on this actor)
	OutIgnore.Add(const_cast<ASplineGeneratingActor*>(this));

	for (AActor* A : ActorsToIgnoreForGenerationTraces)
	{
		if (IsValid(A))
		{
			OutIgnore.Add(A);
		}
	}
}

bool ASplineGeneratingActor::IsLandscapeHit(const FHitResult& Hit)
{
	AActor* A = Hit.GetActor();
	return IsValid(A) && A->IsA<ALandscapeProxy>();
}

USplineComponent* ASplineGeneratingActor::GetRoadSpline_Implementation() const
{
	return TrackSpline;
}

bool ASplineGeneratingActor::LineTraceSingleForObjectsEx(
	const FVector& StartWorld,
	FVector& OutImpactPoint,
	FVector& OutImpactNormal,
	const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	bool bLandscapeOnly
) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<AActor*> IgnoreActors;
	BuildTraceIgnoreActors(IgnoreActors);

	// Use your existing conventions:
	// - Snap trace uses SplineZOffset + 100 above the point
	const FVector TraceStart = StartWorld + FVector(0.f, 0.f, SplineZOffset + 100.f);
	const FVector TraceEnd = FVector(StartWorld.X, StartWorld.Y, LineTraceLength);

	if (!bLandscapeOnly)
	{
		FHitResult Hit;
		const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			TraceStart,
			TraceEnd,
			ObjectTypes,
			/*bTraceComplex*/ true,
			IgnoreActors,
			EDrawDebugTrace::None,
			Hit,
			/*bIgnoreSelf*/ true
		);

		if (bHit)
		{
			OutImpactPoint = Hit.ImpactPoint;
			OutImpactNormal = Hit.ImpactNormal;
			return true;
		}
		return false;
	}

	// Landscape-only: do a multi trace and pick the first Landscape hit.
	TArray<FHitResult> Hits;
	const bool bAnyHit = UKismetSystemLibrary::LineTraceMultiForObjects(
		this,
		TraceStart,
		TraceEnd,
		ObjectTypes,
		/*bTraceComplex*/ true,
		IgnoreActors,
		EDrawDebugTrace::None,
		Hits,
		/*bIgnoreSelf*/ true
	);

	if (!bAnyHit)
	{
		return false;
	}

	for (const FHitResult& H : Hits)
	{
		if (IsLandscapeHit(H))
		{
			OutImpactPoint = H.ImpactPoint;
			OutImpactNormal = H.ImpactNormal;
			return true;
		}
	}

	return false;
}

bool ASplineGeneratingActor::LineTraceHitLandscape(const FVector& StartPoint, FVector& ImpactPoint, FVector& ImpactNormal) const
{
	// Snap trace: optional Landscape-only
	return LineTraceSingleForObjectsEx(
		StartPoint,
		ImpactPoint,
		ImpactNormal,
		ObjectsToHitForLandscapeLineTrace,
		bSnapTraceLandscapeOnly
	);
}

bool ASplineGeneratingActor::LineTraceHitGroundForWalls(const FVector& StartPoint, FVector& ImpactPoint, FVector& ImpactNormal) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	TArray<AActor*> IgnoreActors;
	BuildTraceIgnoreActors(IgnoreActors);

	// Keep your existing wall trace start/end style
	const FVector TraceStart = StartPoint + FVector(0.f, 0.f, 100.f);
	const FVector TraceEnd = FVector(StartPoint.X, StartPoint.Y, GroundWallLineTraceEndWorldZ);

	FHitResult Hit;
	const bool bHit = UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		TraceStart,
		TraceEnd,
		GroundWallObjectsToHitForLineTrace,
		/*bTraceComplex*/ true,
		IgnoreActors,
		EDrawDebugTrace::None,
		Hit,
		/*bIgnoreSelf*/ true
	);

	if (bHit)
	{
		ImpactPoint = Hit.ImpactPoint;
		ImpactNormal = Hit.ImpactNormal;
		return true;
	}

	return false;
}

// ============================================================================
// Component creation helpers (robust in Construction/Editor)
// ============================================================================

UTextRenderComponent* ASplineGeneratingActor::CreateTextComponent(const FTransform& WorldTransform)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	const FName UniqueName = MakeUniqueObjectName(this, UTextRenderComponent::StaticClass(), TEXT("SplineDebugText"));
	UTextRenderComponent* Comp = NewObject<UTextRenderComponent>(this, UTextRenderComponent::StaticClass(), UniqueName, RF_Transactional);
	if (!Comp) return nullptr;

	Comp->SetupAttachment(RootComponent);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetWorldTransform(WorldTransform);
	Comp->RegisterComponent();

#if WITH_EDITOR
	AddInstanceComponent(Comp);
#endif

	return Comp;
}

UProceduralMeshComponent* ASplineGeneratingActor::CreateProcMeshComponent(FName DebugName)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	const FName UniqueName = MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), DebugName);
	UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(this, UProceduralMeshComponent::StaticClass(), UniqueName, RF_Transactional);
	if (!Comp) return nullptr;

	Comp->SetupAttachment(RootComponent);
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->bUseAsyncCooking = true;
	Comp->RegisterComponent();

#if WITH_EDITOR
	AddInstanceComponent(Comp);
#endif

	return Comp;
}

USplineMeshComponent* ASplineGeneratingActor::CreateSplineMeshComponent()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "CreateSplineMeshComponent: TrackSpline is NULL.");
		return nullptr;
	}

	USplineMeshComponent* Comp =
		Cast<USplineMeshComponent>(
			AddComponentByClass(
				USplineMeshComponent::StaticClass(),
				/*bManualAttachment*/ true,
				FTransform::Identity,
				/*bDeferredFinish*/ false));

	if (!Comp)
	{
		ASYNC_LOG(Error, "CreateSplineMeshComponent: AddComponentByClass returned NULL.");
		return nullptr;
	}

	// CRITICAL: bManualAttachment=true -> attach manually or it stays at origin
	Comp->AttachToComponent(TrackSpline, FAttachmentTransformRules::KeepRelativeTransform);
	Comp->SetRelativeTransform(FTransform::Identity);

	// Optional but recommended
	Comp->SetCanEverAffectNavigation(false);

	if (!Comp->IsRegistered())
	{
		Comp->RegisterComponent();
	}

	return Comp;
}

// ============================================================================
// Constructor
// ============================================================================

ASplineGeneratingActor::ASplineGeneratingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	TrackSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrackSpline"));
	TrackSpline->SetupAttachment(RootComponent);

	// Defaults (keep behavior)
	bEditSpline = false;
	bShowSplineVisualizationWidth = false;
	bClosedLoop = false;

	bSnapMeshesToLandscape = false;
	bSnapPointsToLandscape = false;

	bTangentPointsUpdate = false;
	bEnableCollision = false;

	bCastShadow = false;
	bCastContactShadow = false;

	bDeformLandscape = false;
	bRaiseHeights = true;
	bLowerHeights = true;

	LandscapeEditLayerName = FName("Layer");

	SplineSegments = 0;
	SplineZOffset = 0.f;
	SplineZOffsetLandscapeSnapCorrection = 100.f;

	bShowSegmentNumbers = false;
	bShowPointNumbers = false;

	bMirrorMesh = false;
	bMirrorExtraMesh = false;

	DebugTextWorldSize = 500.f;

	LineTraceLength = -100000.f;

	FallOff = 1500.f;
	NumberOfSubdivisionsForDeform = 500;

	SplineVisualizationWidth = 500.f;

	SplinePointList = nullptr;
	PaintLayer = nullptr;
	SplineListName = FName();

	TrackSplineData.Empty();

	SplinePointType = ESplinePointType::CurveCustomTangent;

	// Snap trace default: WorldStatic + WorldDynamic
	ObjectsToHitForLandscapeLineTrace.Empty();
	ObjectsToHitForLandscapeLineTrace.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	ObjectsToHitForLandscapeLineTrace.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

	// Ground wall trace default: WorldStatic
	GroundWallObjectsToHitForLineTrace.Empty();
	GroundWallObjectsToHitForLineTrace.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));

	GroundWallLineTraceEndWorldZ = LineTraceLength;

	// IMPORTANT: allow Landscape-only snapping
	bSnapTraceLandscapeOnly = false;

	// Async defaults
	bUseAsyncBuild = true;
	SegmentsPerTick = 2;
	bAutoRebuildOnConstruction = true;

	bIsBuilding = false;
	bPendingRebuild = false;
	CurrentBuildSegmentIndex = INDEX_NONE;

	LastRebuildRequestTime = 0.0;
	RebuildDelaySeconds = 0.1;

	// Tangent smoothing defaults
	TangentSmoothingTension = 1.0f;
	TangentSmoothingIterations = 1;

	// Ground walls defaults
	bGenerateGroundWalls = true;
	GroundWallSubdivisions = 64;
	GroundWallOutset = 0.f;
	GroundWallFallbackDepth = 20000.f;
	bGroundWallsDoubleSided = true;

	// UV defaults
	GroundWallUVWorldSizeU = 1000.f;
	GroundWallUVWorldSizeV = 500.f;

	DropWallUVWorldSizeU = 1000.f;
	DropWallUVWorldSizeV = 500.f;

	// New UV units (if you keep both in header)
	GroundWallUVWorldUnitsU = 1000.f;
	GroundWallUVWorldUnitsV = 300.f;

	DropWallUVWorldUnitsU = 1000.f;
	DropWallUVWorldUnitsV = 300.f;
}

// ============================================================================
// OnConstruction
// ============================================================================

void ASplineGeneratingActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "TrackSpline component is NULL. Actor cannot function.");
		return;
	}

	TrackSpline->SetClosedLoop(bClosedLoop);

#if WITH_EDITOR
	TrackSpline->bShouldVisualizeScale = bShowSplineVisualizationWidth;
	if (bShowSplineVisualizationWidth && TrackSpline->ScaleVisualizationWidth != SplineVisualizationWidth)
	{
		TrackSpline->ScaleVisualizationWidth = SplineVisualizationWidth;
	}
#endif

	if (bReadFromDataAsset)
	{
		ReadSplineFromDataAsset();
		bReadFromDataAsset = false;
	}

	if (bWriteToDataAsset)
	{
		WriteSplineToDataAsset();
		bWriteToDataAsset = false;
	}

	if (bEditSpline)
	{
		DebugTrackSpline();
		return;
	}

	if (!MainMesh)
	{
		ASYNC_LOG(Warning, "MainMesh is NULL -> build aborted (nothing to render).");
		return;
	}

#if WITH_EDITOR
	if (bAutoRebuildOnConstruction)
	{
		RequestBuild();
	}
#endif
}

// ============================================================================
// Tick (Editor async)
// ============================================================================

#if WITH_EDITOR
bool ASplineGeneratingActor::ShouldTickIfViewportsOnly() const
{
	return true;
}

void ASplineGeneratingActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GIsEditor)
		return;

	UWorld* World = GetWorld();
	if (!World || !World->IsEditorWorld())
		return;

	if (!bUseAsyncBuild)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (bPendingRebuild && !bIsBuilding)
	{
		if ((FPlatformTime::Seconds() - LastRebuildRequestTime) >= RebuildDelaySeconds)
		{
			bPendingRebuild = false;
			StartBuild_Internal();
		}
	}

	if (bIsBuilding)
	{
		BuildNextSegments(FMath::Max(1, SegmentsPerTick));
	}
}
#endif

// ============================================================================
// Manual Rebuild + Cancel
// ============================================================================

void ASplineGeneratingActor::RebuildTrack()
{
#if WITH_EDITOR
	RequestBuild();
#endif
}

void ASplineGeneratingActor::CancelAsyncBuild()
{
#if WITH_EDITOR
	if (!bIsBuilding && !bPendingRebuild)
		return;

	bIsBuilding = false;
	bPendingRebuild = false;
	SetActorTickEnabled(false);

	ASYNC_LOG(Warning, "Async track build cancelled by user.");
#endif
}

// ============================================================================
// Data Asset
// ============================================================================

FSplinePoint ASplineGeneratingActor::MakeSplinePointLocal(int32 PointIndex) const
{
	FSplinePoint SP;
	SP.InputKey = (float)PointIndex;

	if (!TrackSpline)
	{
		return SP;
	}

	const FTransform T = TrackSpline->GetTransformAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);

	SP.Position = T.GetLocation();
	SP.Rotation = T.GetRotation().Rotator();
	SP.Scale = T.GetScale3D();
	SP.Type = TrackSpline->GetSplinePointType(PointIndex);
	SP.ArriveTangent = TrackSpline->GetArriveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
	SP.LeaveTangent = TrackSpline->GetLeaveTangentAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);

	return SP;
}

void ASplineGeneratingActor::WriteSplineToDataAsset()
{
	if (!SplinePointList)
	{
		ASYNC_LOG(Warning, "WriteSplineToDataAsset: SplinePointList is NULL.");
		return;
	}
	if (!TrackSpline)
	{
		ASYNC_LOG(Warning, "WriteSplineToDataAsset: TrackSpline is NULL.");
		return;
	}

	SplinePointList->Modify();
	SplinePointList->SplineListName = SplineListName;

	const int32 Num = TrackSpline->GetNumberOfSplinePoints();
	SplinePointList->PointList.Reset(Num);

	for (int32 i = 0; i < Num; ++i)
	{
		SplinePointList->PointList.Add(MakeSplinePointLocal(i));
	}

	SplinePointList->MarkPackageDirty();
	ASYNC_LOG(Log, "WriteSplineToDataAsset: Saved %d points to DataAsset.", Num);
}

void ASplineGeneratingActor::ReadSplineFromDataAsset()
{
	if (!SplinePointList)
	{
		ASYNC_LOG(Warning, "ReadSplineFromDataAsset: SplinePointList is NULL.");
		return;
	}
	if (!TrackSpline)
	{
		ASYNC_LOG(Warning, "ReadSplineFromDataAsset: TrackSpline is NULL.");
		return;
	}

	SplineListName = SplinePointList->SplineListName;

	TrackSpline->ClearSplinePoints(false);
	for (const FSplinePoint& P : SplinePointList->PointList)
	{
		TrackSpline->AddPoint(P, false);
	}
	TrackSpline->UpdateSpline();

	ASYNC_LOG(Log, "ReadSplineFromDataAsset: Loaded %d points from DataAsset.", SplinePointList->PointList.Num());
}

// ============================================================================
// Utility / Data
// ============================================================================

void ASplineGeneratingActor::CleanData()
{
	SplineSegments = 0;
}

void ASplineGeneratingActor::BuildArrayOfSplineSegments()
{
	if (!TrackSpline)
	{
		SplineSegments = 0;
		return;
	}

	const int32 NumPoints = TrackSpline->GetNumberOfSplinePoints();
	if (NumPoints < 2)
	{
		SplineSegments = 0;
		return;
	}

	SplineSegments = bClosedLoop ? NumPoints : (NumPoints - 1);
}

void ASplineGeneratingActor::CalculateSegmentsAndSetArray()
{
	if (!TrackSpline || SplineSegments <= 0)
	{
		return;
	}

	for (int32 i = 0; i < SplineSegments; ++i)
	{
		TrackSpline->SetSplinePointType(i, SplinePointType, false);
	}

	TrackSpline->UpdateSpline();
}

// ============================================================================
// Segment calculations
// ============================================================================

float ASplineGeneratingActor::CalculateSplineSegmentLength(const int32 SegmentIndex) const
{
	if (!TrackSpline)
	{
		return 0.f;
	}

	const int32 NumPoints = TrackSpline->GetNumberOfSplinePoints();
	if (NumPoints < 2) return 0.f;

	const int32 StartPoint = SegmentIndex;
	const int32 EndPoint = bClosedLoop ? WrapPointIndex(SegmentIndex + 1, NumPoints) : (SegmentIndex + 1);

	const float D0 = TrackSpline->GetDistanceAlongSplineAtSplinePoint(StartPoint);

	float D1 = 0.f;
	if (bClosedLoop && SegmentIndex == NumPoints - 1)
	{
		D1 = TrackSpline->GetSplineLength();
	}
	else
	{
		D1 = TrackSpline->GetDistanceAlongSplineAtSplinePoint(EndPoint);
	}

	return FMath::Max(0.f, D1 - D0);
}

// ============================================================================
// Build (Sync + Async)
// ============================================================================

void ASplineGeneratingActor::AddRoadAndExtraMeshesToSpline()
{
	if (!TrackSpline || !MainMesh)
	{
		ASYNC_LOG(Warning, "AddRoadAndExtraMeshesToSpline: TrackSpline or MainMesh missing.");
		return;
	}

	TrackSpline->SetRelativeLocation(FVector(
		0.f, 0.f,
		bSnapMeshesToLandscape ? (SplineZOffset + SplineZOffsetLandscapeSnapCorrection) : SplineZOffset));

	for (int32 SegmentIndex = 0; SegmentIndex < SplineSegments; ++SegmentIndex)
	{
		BuildSplineMeshComponents(SegmentIndex);

		const int32 DataIndex = (TrackSplineData.IsValidIndex(SegmentIndex) ? SegmentIndex : 0);

		if (TrackSplineData.IsValidIndex(DataIndex) && TrackSplineData[DataIndex].ExtraMesh.Num() > 0)
		{
			for (int32 MeshIndex = 0; MeshIndex < TrackSplineData[DataIndex].ExtraMesh.Num(); ++MeshIndex)
			{
				BuildExtraSplineMeshComponent(SegmentIndex, MeshIndex);
			}
		}
	}

	bDeformLandscape = false;
	BuildDropCliffWalls();
}

void ASplineGeneratingActor::BuildSplineMeshComponents(const int32 SegmentIndex)
{
	if (!TrackSpline || !MainMesh)
	{
		ASYNC_LOG(Error, "BuildSplineMeshComponents(%d): TrackSpline or MainMesh invalid.", SegmentIndex);
		return;
	}
	if (SegmentIndex < 0 || SegmentIndex >= SplineSegments)
	{
		return;
	}

	if (TrackSplineData.Num() == 0)
	{
		TrackSplineData.AddDefaulted();
		ASYNC_LOG(Warning, "TrackSplineData was empty -> added default entry.");
	}

	const int32 DataIndex = TrackSplineData.IsValidIndex(SegmentIndex) ? SegmentIndex : 0;
	const FTrackSplineData& Data = TrackSplineData[DataIndex];

	const int32 NumPoints = TrackSpline->GetNumberOfSplinePoints();
	const float SegmentStartDist = TrackSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex);

	float SegmentEndDist = 0.f;
	if (bClosedLoop && SegmentIndex == NumPoints - 1)
	{
		SegmentEndDist = TrackSpline->GetSplineLength();
	}
	else
	{
		SegmentEndDist = TrackSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1);
	}

	const float SegmentLen = FMath::Max(0.f, SegmentEndDist - SegmentStartDist);
	if (SegmentLen <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DefaultMeshLen = FMath::Max(1.f, MainMesh->GetBoundingBox().GetSize().X);
	const float DesiredPieceLen = (Data.RoadMeshLength > 1.f) ? Data.RoadMeshLength : DefaultMeshLen;

	int32 PieceCount = Data.MeshInstances;
	if (PieceCount <= 0)
	{
		PieceCount = FMath::Max(1, FMath::RoundToInt(SegmentLen / DesiredPieceLen));
	}

	const float PieceLen = SegmentLen / (float)PieceCount;

	const bool bIsJumpGapSegment = IsSegmentInsideJumpGapByPoints(SegmentIndex);

	float SegmentDropOffset = 0.f;
	bool  bUseConstantHeight = false;
	float ConstantWorldZ = 0.f;

	const bool bHasDropInfo =
		GetDropInfoForSegmentByPoints(SegmentIndex, SegmentDropOffset, bUseConstantHeight, ConstantWorldZ);

	const bool bIsDropSegment =
		bHasDropInfo && (!FMath::IsNearlyZero(SegmentDropOffset) || bUseConstantHeight);

	const FTransform SplineWorldTM = TrackSpline->GetComponentTransform();

	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const float MeshStartDist = SegmentStartDist + PieceIndex * PieceLen;
		const float MeshEndDist = SegmentStartDist + (PieceIndex + 1) * PieceLen;

		if (bIsJumpGapSegment)
		{
			continue;
		}

		USplineMeshComponent* SplineMesh = CreateSplineMeshComponent();
		if (!SplineMesh)
		{
			ASYNC_LOG(Error, "Failed to create SplineMeshComponent.");
			continue;
		}

		GeneratedSplineMeshes.Add(SplineMesh);

		if (RoadPhysicalMaterial)
		{
			SplineMesh->SetPhysMaterialOverride(RoadPhysicalMaterial);
		}

		UStaticMesh* ChosenMesh = MainMesh;

		if (!bClosedLoop)
		{
			if (SegmentIndex == 0 && PieceIndex == 0)
			{
				ChosenMesh = StartMesh ? StartMesh : MainMesh;
			}
			else if (SegmentIndex == SplineSegments - 1 && PieceIndex == PieceCount - 1)
			{
				ChosenMesh = EndMesh ? EndMesh : MainMesh;
			}
		}
		else
		{
			if (SegmentIndex == 0 && PieceIndex == 0)
			{
				ChosenMesh = StartMesh ? StartMesh : MainMesh;
			}
		}

		SplineMesh->SetStaticMesh(ChosenMesh);

		// Road should always be traceable (Query), physics optional
		SplineMesh->SetCollisionEnabled(bEnableCollision
			? ECollisionEnabled::QueryAndPhysics
			: ECollisionEnabled::QueryOnly);

		// Make sure Visibility blocks (or set a proper profile)
		SplineMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		SplineMesh->SetCastShadow(bCastShadow);
		SplineMesh->SetCastContactShadow(bCastContactShadow);

		FVector ImpactLoc = FVector::ZeroVector;
		FVector ImpactNorm = FVector::UpVector;

		FVector StartWorld = TrackSpline->GetLocationAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World);
		FVector EndWorld = TrackSpline->GetLocationAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World);

		if (bSnapMeshesToLandscape && LineTraceHitLandscape(StartWorld, ImpactLoc, ImpactNorm))
		{
			StartWorld = ImpactLoc;
		}
		if (bSnapMeshesToLandscape && LineTraceHitLandscape(EndWorld, ImpactLoc, ImpactNorm))
		{
			EndWorld = ImpactLoc;
		}

		if (bIsDropSegment)
		{
			StartWorld.Z = bUseConstantHeight ? ConstantWorldZ : (StartWorld.Z + SegmentDropOffset);
			EndWorld.Z = bUseConstantHeight ? ConstantWorldZ : (EndWorld.Z + SegmentDropOffset);
		}

		FVector StartDirWorld = TrackSpline->GetDirectionAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World);
		FVector EndDirWorld = TrackSpline->GetDirectionAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World);

		if (bUseConstantHeight)
		{
			StartDirWorld.Z = 0.f;
			EndDirWorld.Z = 0.f;
			if (!StartDirWorld.Normalize()) StartDirWorld = FVector::ForwardVector;
			if (!EndDirWorld.Normalize()) EndDirWorld = FVector::ForwardVector;
		}

		// LOCAL to TrackSpline (and the SplineMesh is attached to TrackSpline with identity)
		const FVector StartLocal = SplineWorldTM.InverseTransformPosition(StartWorld);
		const FVector EndLocal = SplineWorldTM.InverseTransformPosition(EndWorld);

		const FVector StartTanLocal = SplineWorldTM.InverseTransformVectorNoScale(StartDirWorld * PieceLen);
		const FVector EndTanLocal = SplineWorldTM.InverseTransformVectorNoScale(EndDirWorld * PieceLen);

		SplineMesh->SetStartAndEnd(StartLocal, StartTanLocal, EndLocal, EndTanLocal, /*bUpdateMesh*/ false);

		FVector2D ScaleStart(TrackSpline->GetScaleAtDistanceAlongSpline(MeshStartDist).Y, 1.f);
		FVector2D ScaleEnd(TrackSpline->GetScaleAtDistanceAlongSpline(MeshEndDist).Y, 1.f);

		if (bMirrorMesh)
		{
			ScaleStart.X *= -1.f;
			ScaleEnd.X *= -1.f;
		}

		SplineMesh->SetStartScale(ScaleStart, false);
		SplineMesh->SetEndScale(ScaleEnd, false);

		const float StartRollRad =
			TrackSpline->GetRotationAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World).Roll / GetDivisor();

		const float EndRollRad =
			TrackSpline->GetRotationAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World).Roll / GetDivisor();

		SplineMesh->SetStartRoll(StartRollRad, false);
		SplineMesh->SetEndRoll(EndRollRad, false);

		SplineMesh->UpdateMesh();

#if WITH_EDITOR
		// (dein Landscape-Deform Block bleibt bei dir unverändert)
#endif
	}
}

// ============================================================================
// Extra Mesh Builder (FIXED SPACE: now also TrackSpline-local)
// ============================================================================

void ASplineGeneratingActor::BuildExtraSplineMeshComponent(const int32 SegmentIndex, const int32 MeshIndex)
{
	if (!TrackSpline || !MainMesh)
	{
		ASYNC_LOG(Error, "BuildExtraSplineMeshComponent: invalid TrackSpline/MainMesh.");
		return;
	}
	if (TrackSplineData.Num() == 0)
	{
		return;
	}

	const int32 DataIndex = TrackSplineData.IsValidIndex(SegmentIndex) ? SegmentIndex : 0;
	const FTrackSplineData& Data = TrackSplineData[DataIndex];

	const int32 NumPoints = TrackSpline->GetNumberOfSplinePoints();
	const float SegmentStartDist = TrackSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex);

	float SegmentEndDist = 0.f;
	if (bClosedLoop && SegmentIndex == NumPoints - 1)
	{
		SegmentEndDist = TrackSpline->GetSplineLength();
	}
	else
	{
		SegmentEndDist = TrackSpline->GetDistanceAlongSplineAtSplinePoint(SegmentIndex + 1);
	}

	const float SegmentLen = FMath::Max(0.f, SegmentEndDist - SegmentStartDist);
	if (SegmentLen <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float DefaultMeshLen = FMath::Max(1.f, MainMesh->GetBoundingBox().GetSize().X);
	const float DesiredPieceLen = (Data.RoadMeshLength > 1.f) ? Data.RoadMeshLength : DefaultMeshLen;

	int32 PieceCount = Data.MeshInstances;
	if (PieceCount <= 0)
	{
		PieceCount = FMath::Max(1, FMath::RoundToInt(SegmentLen / DesiredPieceLen));
	}

	const float PieceLen = SegmentLen / (float)PieceCount;

	const FTransform SplineWorldTM = TrackSpline->GetComponentTransform();

	for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
	{
		const float MeshStartDist = SegmentStartDist + PieceIndex * PieceLen;
		const float MeshEndDist = SegmentStartDist + (PieceIndex + 1) * PieceLen;

		USplineMeshComponent* SplineMesh = CreateSplineMeshComponent();
		if (!SplineMesh)
		{
			ASYNC_LOG(Error, "Could not create extra SplineMeshComponent.");
			continue;
		}

		GeneratedSplineMeshes.Add(SplineMesh);

		UStaticMesh* SelectedMesh = nullptr;

		if (PieceIndex == 0 && Data.ExtraMeshStart.IsValidIndex(MeshIndex))
			SelectedMesh = Data.ExtraMeshStart[MeshIndex];
		else if (PieceIndex == PieceCount - 1 && Data.ExtraMeshEnd.IsValidIndex(MeshIndex))
			SelectedMesh = Data.ExtraMeshEnd[MeshIndex];
		else if (Data.ExtraMesh.IsValidIndex(MeshIndex))
			SelectedMesh = Data.ExtraMesh[MeshIndex];

		if (!SelectedMesh)
		{
			SplineMesh->DestroyComponent();
			continue;
		}

		SplineMesh->SetStaticMesh(SelectedMesh);

		SplineMesh->SetCollisionEnabled(
			bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

		SplineMesh->SetCastShadow(bCastShadow);
		SplineMesh->SetCastContactShadow(bCastContactShadow);

		FVector ImpactLoc, ImpactNormal;

		FVector StartWorld = TrackSpline->GetLocationAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World);
		FVector EndWorld = TrackSpline->GetLocationAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World);

		if (bSnapMeshesToLandscape && LineTraceHitLandscape(StartWorld, ImpactLoc, ImpactNormal))
			StartWorld = ImpactLoc;
		if (bSnapMeshesToLandscape && LineTraceHitLandscape(EndWorld, ImpactLoc, ImpactNormal))
			EndWorld = ImpactLoc;

		FVector StartDirWorld = TrackSpline->GetDirectionAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World);
		FVector EndDirWorld = TrackSpline->GetDirectionAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World);

		const FVector StartLocal = SplineWorldTM.InverseTransformPosition(StartWorld);
		const FVector EndLocal = SplineWorldTM.InverseTransformPosition(EndWorld);

		const FVector StartTanLocal = SplineWorldTM.InverseTransformVectorNoScale(StartDirWorld * PieceLen);
		const FVector EndTanLocal = SplineWorldTM.InverseTransformVectorNoScale(EndDirWorld * PieceLen);

		SplineMesh->SetStartAndEnd(StartLocal, StartTanLocal, EndLocal, EndTanLocal, true);

		FVector2D ScaleStart(TrackSpline->GetScaleAtDistanceAlongSpline(MeshStartDist).Y, 1.f);
		FVector2D ScaleEnd(TrackSpline->GetScaleAtDistanceAlongSpline(MeshEndDist).Y, 1.f);

		if (bMirrorExtraMesh)
		{
			ScaleStart.X *= -1.f;
			ScaleEnd.X *= -1.f;
		}

		const float Offset =
			Data.ExtraMeshOffset.IsValidIndex(MeshIndex) ? Data.ExtraMeshOffset[MeshIndex] : 0.f;

		SplineMesh->SetStartScale(ScaleStart, false);
		SplineMesh->SetEndScale(ScaleEnd, false);

		SplineMesh->SetStartOffset(FVector2D(Offset, 0.f), false);
		SplineMesh->SetEndOffset(FVector2D(Offset, 0.f), false);

		const float StartRollRad =
			TrackSpline->GetRotationAtDistanceAlongSpline(MeshStartDist, ESplineCoordinateSpace::World).Roll / GetDivisor();

		const float EndRollRad =
			TrackSpline->GetRotationAtDistanceAlongSpline(MeshEndDist, ESplineCoordinateSpace::World).Roll / GetDivisor();

		SplineMesh->SetStartRoll(StartRollRad, false);
		SplineMesh->SetEndRoll(EndRollRad, false);

		SplineMesh->UpdateMesh();
	}
}

// ============================================================================
// Debug
// ============================================================================

void ASplineGeneratingActor::DebugTrackSpline()
{
	ClearDebugText();
	ShowSegmentNumbers();
}

void ASplineGeneratingActor::ShowSegmentNumbers()
{
	if (!TrackSpline) return;

	if (bShowPointNumbers)
	{
		const int32 NumPts = TrackSpline->GetNumberOfSplinePoints();
		for (int32 i = 0; i < NumPts; i++)
		{
			FVector Loc =
				TrackSpline->GetTransformAtSplinePoint(i, ESplineCoordinateSpace::World).GetLocation()
				+ FVector(0.f, 0.f, 200.f);

			FTransform T;
			T.SetLocation(Loc);
			T.SetRotation(FQuat::Identity);
			T.SetScale3D(FVector(.5f));

			UTextRenderComponent* Text = CreateTextComponent(T);
			if (Text)
			{
				GeneratedDebugText.Add(Text);
				Text->SetText(FText::FromString(FString::FromInt(i)));
				Text->SetHorizontalAlignment(EHTA_Center);
				Text->SetVerticalAlignment(EVRTA_TextCenter);
				Text->SetWorldSize(DebugTextWorldSize);
			}
		}
	}

	if (bShowSegmentNumbers)
	{
		for (int32 i = 0; i < SplineSegments; i++)
		{
			const float D0 = TrackSpline->GetDistanceAlongSplineAtSplinePoint(i);
			const float SegLen = CalculateSplineSegmentLength(i);
			const float Mid = D0 + SegLen * 0.5f;

			FVector Loc =
				TrackSpline->GetLocationAtDistanceAlongSpline(Mid, ESplineCoordinateSpace::World)
				+ FVector(0.f, 0.f, 500.f);

			FTransform T;
			T.SetLocation(Loc);
			T.SetRotation(FQuat::Identity);
			T.SetScale3D(FVector(1.f));

			UTextRenderComponent* Text = CreateTextComponent(T);
			if (Text)
			{
				GeneratedDebugText.Add(Text);
				Text->SetText(FText::FromString(FString::FromInt(i)));
				Text->SetHorizontalAlignment(EHTA_Center);
				Text->SetVerticalAlignment(EVRTA_TextCenter);
				Text->SetWorldSize(DebugTextWorldSize);
			}
		}
	}
}

// ============================================================================
// Landscape Interaction
// ============================================================================

void ASplineGeneratingActor::SnapToLandscape()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "SnapToLandscape: TrackSpline NULL.");
		return;
	}

	if (!bSnapPointsToLandscape)
	{
		return;
	}

	for (int32 i = 0; i < TrackSpline->GetNumberOfSplinePoints(); i++)
	{
		FVector ImpactLoc, ImpactNormal;
		const FVector SplineLoc = TrackSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);

		if (LineTraceHitLandscape(SplineLoc, ImpactLoc, ImpactNormal))
		{
			TrackSpline->SetLocationAtSplinePoint(i, ImpactLoc, ESplineCoordinateSpace::World, false);
			TrackSpline->SetUpVectorAtSplinePoint(i, ImpactNormal, ESplineCoordinateSpace::World, false);

			if (bTangentPointsUpdate)
			{
				const FVector Right = TrackSpline->GetRightVectorAtSplinePoint(i, ESplineCoordinateSpace::World);
				const float TangentLen = FMath::Max(50.f, CalculateSplineSegmentLength(FMath::Clamp(i, 0, SplineSegments - 1)));

				TrackSpline->SetTangentAtSplinePoint(
					i,
					UKismetMathLibrary::RotateAngleAxis(ImpactNormal, 90.f, Right) * TangentLen,
					ESplineCoordinateSpace::World,
					false);
			}
		}
	}

	TrackSpline->UpdateSpline();
}

// ============================================================================
// Spline Update
// ============================================================================

void ASplineGeneratingActor::UpdateSpline() const
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "UpdateSpline: TrackSpline == NULL.");
		return;
	}

	TrackSpline->UpdateSpline();
}

float ASplineGeneratingActor::GetDivisor() const
{
	return 180.f / UKismetMathLibrary::GetPI();
}

// ============================================================================
// Async Build
// ============================================================================

void ASplineGeneratingActor::RequestBuild()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "RequestBuild: TrackSpline NULL.");
		return;
	}
	if (!MainMesh)
	{
		ASYNC_LOG(Warning, "RequestBuild: MainMesh NULL -> nothing to render.");
		return;
	}

#if WITH_EDITOR
	if (!GIsEditor)
		return;
#endif

	if (bIsBuilding || bPendingRebuild)
	{
		CancelAsyncBuild();
	}

	ClearGeneratedComponents();
	CleanData();

	BuildArrayOfSplineSegments();
	CalculateSegmentsAndSetArray();

	if (SplineSegments <= 0)
	{
		ASYNC_LOG(Warning, "RequestBuild: No spline segments found.");
		return;
	}

#if WITH_EDITOR
	if (bUseAsyncBuild && GIsEditor)
	{
		bPendingRebuild = true;
		LastRebuildRequestTime = FPlatformTime::Seconds();
		SetActorTickEnabled(true);
	}
	else
#endif
	{
		AddRoadAndExtraMeshesToSpline();

		SnapToLandscape();
		UpdateSpline();
		DebugTrackSpline();

		if (bGenerateGroundWalls)
		{
			BuildGroundWalls();
		}
		else
		{
			ClearGroundWalls();
		}
	}
}

void ASplineGeneratingActor::StartBuild_Internal()
{
	if (SplineSegments <= 0 || !TrackSpline || !MainMesh)
	{
		ASYNC_LOG(Error, "StartBuild_Internal failed: invalid spline state.");
		SetActorTickEnabled(false);
		return;
	}

	TrackSpline->SetRelativeLocation(FVector(
		0.f, 0.f,
		bSnapMeshesToLandscape ? (SplineZOffset + SplineZOffsetLandscapeSnapCorrection) : SplineZOffset));

	bIsBuilding = true;
	CurrentBuildSegmentIndex = 0;

	ASYNC_LOG(Log, "Async build started (%d segments).", SplineSegments);
}

void ASplineGeneratingActor::BuildNextSegments(int32 NumSegments)
{
	int32 Processed = 0;

	while (CurrentBuildSegmentIndex < SplineSegments && Processed < NumSegments)
	{
		const int32 Index = CurrentBuildSegmentIndex;

		BuildSplineMeshComponents(Index);

		const int32 DataIndex = TrackSplineData.IsValidIndex(Index) ? Index : 0;

		if (TrackSplineData.IsValidIndex(DataIndex) &&
			TrackSplineData[DataIndex].ExtraMesh.Num() > 0)
		{
			for (int32 j = 0; j < TrackSplineData[DataIndex].ExtraMesh.Num(); j++)
			{
				BuildExtraSplineMeshComponent(Index, j);
			}
		}

		++CurrentBuildSegmentIndex;
		++Processed;
	}

	if (CurrentBuildSegmentIndex >= SplineSegments)
	{
		FinishBuild_Internal();
	}
}

void ASplineGeneratingActor::FinishBuild_Internal()
{
	bIsBuilding = false;

	SnapToLandscape();
	UpdateSpline();
	DebugTrackSpline();

	bDeformLandscape = false;

	if (bGenerateGroundWalls)
	{
		BuildGroundWalls();
	}
	else
	{
		ClearGroundWalls();
	}

	BuildDropCliffWalls();

	if (!bPendingRebuild)
	{
		SetActorTickEnabled(false);
	}

	ASYNC_LOG(Log, "Async build finished.");
}

// ============================================================================
// Cleanup
// ============================================================================

void ASplineGeneratingActor::ClearGeneratedMeshes()
{
	for (USplineMeshComponent* Comp : GeneratedSplineMeshes)
	{
		if (IsValid(Comp))
			Comp->DestroyComponent();
	}
	GeneratedSplineMeshes.Empty();

	for (UProceduralMeshComponent* Comp : GeneratedDropWalls)
	{
		if (IsValid(Comp))
			Comp->DestroyComponent();
	}
	GeneratedDropWalls.Empty();
}

void ASplineGeneratingActor::ClearDebugText()
{
	for (UTextRenderComponent* Comp : GeneratedDebugText)
	{
		if (IsValid(Comp))
			Comp->DestroyComponent();
	}
	GeneratedDebugText.Empty();
}

void ASplineGeneratingActor::ClearGroundWalls()
{
	if (IsValid(LeftGroundWall))
	{
		LeftGroundWall->DestroyComponent();
		LeftGroundWall = nullptr;
	}
	if (IsValid(RightGroundWall))
	{
		RightGroundWall->DestroyComponent();
		RightGroundWall = nullptr;
	}
}

void ASplineGeneratingActor::ClearGeneratedComponents()
{
	ClearGeneratedMeshes();
	ClearDebugText();
	ClearGroundWalls();
}

// ============================================================================
// Ground Walls (with separate trace settings + UV height scaling)
// ============================================================================

float ASplineGeneratingActor::GetHalfRoadWidthAtDistance(float DistanceAlongSpline) const
{
	if (!TrackSpline || !MainMesh)
	{
		return 0.f;
	}

	const float ScaleY = TrackSpline->GetScaleAtDistanceAlongSpline(DistanceAlongSpline).Y;
	const float MeshHalfWidth = MainMesh->GetBounds().BoxExtent.Y;
	return ScaleY * MeshHalfWidth;
}

void ASplineGeneratingActor::BuildGroundWalls()
{
	if (!TrackSpline || !MainMesh)
	{
		ASYNC_LOG(Warning, "BuildGroundWalls: missing TrackSpline or MainMesh.");
		return;
	}

	GroundWallSubdivisions = FMath::Max(4, GroundWallSubdivisions);

	BuildSingleGroundWall(-1, LeftGroundWall);
	BuildSingleGroundWall(+1, RightGroundWall);
}

void ASplineGeneratingActor::BuildSingleGroundWall(int32 SideSign, TObjectPtr<UProceduralMeshComponent>& OutComp)
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "BuildSingleGroundWall: TrackSpline == NULL");
		return;
	}

	if (!OutComp)
	{
		OutComp = CreateProcMeshComponent(SideSign < 0 ? TEXT("LeftGroundWall") : TEXT("RightGroundWall"));
		if (!OutComp)
		{
			ASYNC_LOG(Error, "BuildSingleGroundWall: could not create ProceduralMeshComponent.");
			return;
		}

		OutComp->SetCollisionEnabled(
			bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

		if (GroundWallMaterial)
		{
			OutComp->SetMaterial(0, GroundWallMaterial);
		}

		OutComp->SetCanEverAffectNavigation(false);
	}

	if (GroundWallSubdivisions < 2)
	{
		ASYNC_LOG(Warning, "GroundWallSubdivisions < 2 -> skipping wall build.");
		return;
	}
	OutComp->ClearAllMeshSections();

	TArray<FVector>          Vertices;
	TArray<int32>            Triangles;
	TArray<FVector>          Normals;
	TArray<FVector2D>        UVs;
	TArray<FLinearColor>     VertexColors;
	TArray<FProcMeshTangent> Tangents;

	const float TotalLength = TrackSpline->GetSplineLength();
	const float Step = TotalLength / (float)GroundWallSubdivisions;

	const FTransform ActorTM = GetActorTransform();
	const bool bFlipWinding = (SideSign < 0);

	// prefer new Units vars if you keep both
	const float UDenom = (GroundWallUVWorldUnitsU > 1.f) ? GroundWallUVWorldUnitsU : GroundWallUVWorldSizeU;
	const float VDenom = (GroundWallUVWorldUnitsV > 1.f) ? GroundWallUVWorldUnitsV : GroundWallUVWorldSizeV;

	for (int32 i = 0; i <= GroundWallSubdivisions; ++i)
	{
		const float Distance = i * Step;

		const int32 SegmentIndex = GetSegmentIndexFromDistance(Distance);

		float DropOffset = 0.f;
		bool  bUseConst = false;
		float ConstWorldZ = 0.f;

		const bool bHasDropInfo =
			GetDropInfoForSegmentByPoints(SegmentIndex, DropOffset, bUseConst, ConstWorldZ);

		const FVector RoadLoc = TrackSpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
		const FVector Right = TrackSpline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);

		const float HalfRoadWidth = GetHalfRoadWidthAtDistance(Distance) + GroundWallOutset;

		FVector TopPosWorld = RoadLoc + Right * (HalfRoadWidth * SideSign);

		if (bHasDropInfo)
		{
			TopPosWorld.Z = bUseConst ? ConstWorldZ : (TopPosWorld.Z + DropOffset);
		}

		FVector ImpactLoc, ImpactNormal;
		FVector BottomPosWorld = TopPosWorld;

		if (LineTraceHitGroundForWalls(TopPosWorld, ImpactLoc, ImpactNormal))
		{
			BottomPosWorld = ImpactLoc;
		}
		else
		{
			BottomPosWorld = TopPosWorld - FVector(0.f, 0.f, FMath::Max(0.f, GroundWallFallbackDepth));
		}

		const float WallHeightWorld = FMath::Max(1.f, (TopPosWorld - BottomPosWorld).Size());

		const FVector WallDirWorld = (BottomPosWorld - TopPosWorld).GetSafeNormal();
		FVector NormalWorld = FVector::CrossProduct(WallDirWorld, Right * SideSign).GetSafeNormal();
		if (SideSign < 0)
		{
			NormalWorld *= -1.f;
		}

		const FVector TangentDirWorld = Right * SideSign;

		const FVector TopPosLocal = ActorTM.InverseTransformPosition(TopPosWorld);
		const FVector BottomPosLocal = ActorTM.InverseTransformPosition(BottomPosWorld);
		const FVector NormalLocal = ActorTM.InverseTransformVectorNoScale(NormalWorld);
		const FVector TangentLocal = ActorTM.InverseTransformVectorNoScale(TangentDirWorld);

		const int32 BaseIndex = Vertices.Num();

		Vertices.Add(TopPosLocal);
		Vertices.Add(BottomPosLocal);

		Normals.Add(NormalLocal);
		Normals.Add(NormalLocal);

		const float U = (UDenom > 1.f) ? (Distance / UDenom) : (float)i;
		const float VMax = (VDenom > 1.f) ? (WallHeightWorld / VDenom) : 1.f;

		UVs.Add(FVector2D(U, 0.f));
		UVs.Add(FVector2D(U, VMax));

		VertexColors.Add(FLinearColor::White);
		VertexColors.Add(FLinearColor::White);

		FProcMeshTangent Tangent(TangentLocal, false);
		Tangents.Add(Tangent);
		Tangents.Add(Tangent);

		if (i < GroundWallSubdivisions)
		{
			if (!bFlipWinding)
			{
				Triangles.Add(BaseIndex + 0);
				Triangles.Add(BaseIndex + 1);
				Triangles.Add(BaseIndex + 2);

				Triangles.Add(BaseIndex + 2);
				Triangles.Add(BaseIndex + 1);
				Triangles.Add(BaseIndex + 3);
			}
			else
			{
				Triangles.Add(BaseIndex + 0);
				Triangles.Add(BaseIndex + 2);
				Triangles.Add(BaseIndex + 1);

				Triangles.Add(BaseIndex + 2);
				Triangles.Add(BaseIndex + 3);
				Triangles.Add(BaseIndex + 1);
			}
		}
	}

	OutComp->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		VertexColors,
		Tangents,
		bEnableCollision,
		false
	);

	if (bGroundWallsDoubleSided)
	{
		TArray<int32> BackTriangles;
		BackTriangles.Reserve(Triangles.Num());

		for (int32 t = 0; t < Triangles.Num(); t += 3)
		{
			BackTriangles.Add(Triangles[t + 0]);
			BackTriangles.Add(Triangles[t + 2]);
			BackTriangles.Add(Triangles[t + 1]);
		}

		TArray<FVector> BackNormals = Normals;
		for (FVector& N : BackNormals)
		{
			N *= -1.f;
		}

		OutComp->CreateMeshSection_LinearColor(
			1,
			Vertices,
			BackTriangles,
			BackNormals,
			UVs,
			VertexColors,
			Tangents,
			false,
			false
		);

		if (GroundWallMaterial)
		{
			OutComp->SetMaterial(1, GroundWallMaterial);
		}
	}
}

// ============================================================================
// Drop Walls
// ============================================================================

void ASplineGeneratingActor::BuildDropCliffWalls()
{
	if (!TrackSpline || SplineSegments <= 1)
	{
		return;
	}

	for (int32 i = 0; i < SplineSegments - 1; ++i)
	{
		const bool bSegAHasRoad = !IsSegmentInsideJumpGapByPoints(i);
		const bool bSegBHasRoad = !IsSegmentInsideJumpGapByPoints(i + 1);

		if (!bSegAHasRoad || !bSegBHasRoad)
		{
			continue;
		}

		const float BoundaryDistance = TrackSpline->GetDistanceAlongSplineAtSplinePoint(i + 1);
		const FVector CenterWorld = TrackSpline->GetLocationAtDistanceAlongSpline(BoundaryDistance, ESplineCoordinateSpace::World);

		float DropA = 0.f; bool bUseConstA = false; float ConstZA = 0.f;
		float DropB = 0.f; bool bUseConstB = false; float ConstZB = 0.f;

		const bool bHasDropA = GetDropInfoForSegmentByPoints(i, DropA, bUseConstA, ConstZA);
		const bool bHasDropB = GetDropInfoForSegmentByPoints(i + 1, DropB, bUseConstB, ConstZB);

		float WorldZA = CenterWorld.Z;
		float WorldZB = CenterWorld.Z;

		if (bHasDropA) WorldZA = bUseConstA ? ConstZA : (CenterWorld.Z + DropA);
		if (bHasDropB) WorldZB = bUseConstB ? ConstZB : (CenterWorld.Z + DropB);

		if (FMath::IsNearlyEqual(WorldZA, WorldZB))
		{
			continue;
		}

		BuildDropCliffWallAtDistance(BoundaryDistance, WorldZA, WorldZB);
	}
}

void ASplineGeneratingActor::BuildDropCliffWallAtDistance(float DistanceAlongSpline, float FromWorldZ, float ToWorldZ)
{
	if (!TrackSpline || FMath::IsNearlyEqual(FromWorldZ, ToWorldZ))
	{
		return;
	}

	const float TopZ = FMath::Max(FromWorldZ, ToWorldZ);
	const float BottomZ = FMath::Min(FromWorldZ, ToWorldZ);

	const FVector CenterWorld = TrackSpline->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);
	const FVector Right = TrackSpline->GetRightVectorAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

	const float HalfWidth = GetHalfRoadWidthAtDistance(DistanceAlongSpline);

	const FVector TopCenterWorld(CenterWorld.X, CenterWorld.Y, TopZ);
	const FVector BottomCenterWorld(CenterWorld.X, CenterWorld.Y, BottomZ);

	const FVector TopLeftWorld = TopCenterWorld - Right * HalfWidth;
	const FVector TopRightWorld = TopCenterWorld + Right * HalfWidth;
	const FVector BottomLeftWorld = BottomCenterWorld - Right * HalfWidth;
	const FVector BottomRightWorld = BottomCenterWorld + Right * HalfWidth;

	const float WallHeightWorld = FMath::Max(1.f, TopZ - BottomZ);
	const float WallWidthWorld = FMath::Max(1.f, (2.f * HalfWidth));

	const float UDenom = (DropWallUVWorldUnitsU > 1.f) ? DropWallUVWorldUnitsU : DropWallUVWorldSizeU;
	const float VDenom = (DropWallUVWorldUnitsV > 1.f) ? DropWallUVWorldUnitsV : DropWallUVWorldSizeV;

	const float UMax = (UDenom > 1.f) ? (WallWidthWorld / UDenom) : 1.f;
	const float VMax = (VDenom > 1.f) ? (WallHeightWorld / VDenom) : 1.f;

	const FTransform ActorTM = GetActorTransform();

	const FVector TopLeftLocal = ActorTM.InverseTransformPosition(TopLeftWorld);
	const FVector TopRightLocal = ActorTM.InverseTransformPosition(TopRightWorld);
	const FVector BottomLeftLocal = ActorTM.InverseTransformPosition(BottomLeftWorld);
	const FVector BottomRightLocal = ActorTM.InverseTransformPosition(BottomRightWorld);

	FVector NormalWorld =
		FVector::CrossProduct((BottomLeftWorld - TopLeftWorld), (TopRightWorld - TopLeftWorld)).GetSafeNormal();

	const FVector NormalLocal = ActorTM.InverseTransformVectorNoScale(NormalWorld);
	const FVector TangentLocal = ActorTM.InverseTransformVectorNoScale(Right);

	UProceduralMeshComponent* WallComp = CreateProcMeshComponent(TEXT("DropCliffWall"));
	if (!WallComp)
	{
		return;
	}

	WallComp->SetCollisionEnabled(bEnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	WallComp->SetCanEverAffectNavigation(false);

	if (DropWallMaterial)
	{
		WallComp->SetMaterial(0, DropWallMaterial);
	}

	GeneratedDropWalls.Add(WallComp);

	TArray<FVector>          Vertices;
	TArray<int32>            Triangles;
	TArray<FVector>          Normals;
	TArray<FVector2D>        UVs;
	TArray<FLinearColor>     Colors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.Add(TopLeftLocal);
	Vertices.Add(TopRightLocal);
	Vertices.Add(BottomRightLocal);
	Vertices.Add(BottomLeftLocal);

	for (int32 i = 0; i < 4; ++i)
	{
		Normals.Add(NormalLocal);
		Colors.Add(FLinearColor::White);
		Tangents.Add(FProcMeshTangent(TangentLocal, false));
	}

	UVs.Add(FVector2D(0.f, 0.f));
	UVs.Add(FVector2D(UMax, 0.f));
	UVs.Add(FVector2D(UMax, VMax));
	UVs.Add(FVector2D(0.f, VMax));

	Triangles.Add(0);
	Triangles.Add(1);
	Triangles.Add(2);

	Triangles.Add(0);
	Triangles.Add(2);
	Triangles.Add(3);

	WallComp->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		Tangents,
		bEnableCollision,
		false
	);
}

// ============================================================================
// Segment helpers / gaps / drops
// ============================================================================

int32 ASplineGeneratingActor::GetSegmentIndexFromDistance(float Distance) const
{
	if (!TrackSpline || SplineSegments <= 0)
	{
		return 0;
	}

	for (int32 Seg = 0; Seg < SplineSegments; ++Seg)
	{
		const float D0 = TrackSpline->GetDistanceAlongSplineAtSplinePoint(Seg);

		float D1 = 0.f;
		if (bClosedLoop && Seg == TrackSpline->GetNumberOfSplinePoints() - 1)
		{
			D1 = TrackSpline->GetSplineLength();
		}
		else
		{
			D1 = TrackSpline->GetDistanceAlongSplineAtSplinePoint(Seg + 1);
		}

		if (Distance >= D0 && Distance <= D1)
		{
			return Seg;
		}
	}

	return SplineSegments - 1;
}

bool ASplineGeneratingActor::IsSegmentInsideJumpGapByPoints(int32 SegmentIndex) const
{
	for (const FStuntGapByPoints& Gap : JumpGapsByPoints)
	{
		if (Gap.EndPointIndex <= Gap.StartPointIndex)
		{
			continue;
		}

		if (SegmentIndex >= Gap.StartPointIndex &&
			SegmentIndex < Gap.EndPointIndex)
		{
			return true;
		}
	}
	return false;
}

bool ASplineGeneratingActor::GetDropInfoForSegmentByPoints(
	int32 SegmentIndex,
	float& OutDropOffset,
	bool& bOutUseConstantHeight,
	float& OutConstantGapWorldZ) const
{
	bool  bHasAny = false;
	float StrongestDrop = 0.f;
	bool  bUseConst = false;
	float ConstZ = 0.f;

	for (const FStuntDropByPoints& Drop : StuntDropsByPoints)
	{
		if (Drop.EndPointIndex <= Drop.StartPointIndex)
		{
			continue;
		}

		if (SegmentIndex >= Drop.StartPointIndex &&
			SegmentIndex < Drop.EndPointIndex)
		{
			if (!bHasAny)
			{
				bHasAny = true;
				StrongestDrop = Drop.DropHeight;
				bUseConst = Drop.bUseConstantGapHeight;
				ConstZ = Drop.ConstantGapWorldZ;
			}
			else
			{
				if (Drop.DropHeight < StrongestDrop)
				{
					StrongestDrop = Drop.DropHeight;
					bUseConst = Drop.bUseConstantGapHeight;
					ConstZ = Drop.ConstantGapWorldZ;
				}
			}
		}
	}

	if (!bHasAny)
	{
		OutDropOffset = 0.f;
		bOutUseConstantHeight = false;
		OutConstantGapWorldZ = 0.f;
		return false;
	}

	OutDropOffset = StrongestDrop;
	bOutUseConstantHeight = bUseConst;
	OutConstantGapWorldZ = ConstZ;
	return true;
}

// ============================================================================
// Tangent smoothing (kept)
// ============================================================================

void ASplineGeneratingActor::SmoothSplineTangents()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "SmoothSplineTangents: TrackSpline == NULL.");
		return;
	}

	const int32 NumPoints = TrackSpline->GetNumberOfSplinePoints();
	if (NumPoints < 2)
	{
		ASYNC_LOG(Warning, "SmoothSplineTangents: not enough spline points (%d).", NumPoints);
		return;
	}

	const bool  bLoop = TrackSpline->IsClosedLoop();
	const float Tension = FMath::Clamp(TangentSmoothingTension, 0.0f, 2.0f);
	const int32 Passes = FMath::Max(1, TangentSmoothingIterations);

	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		TArray<FVector> NewTangents;
		NewTangents.SetNum(NumPoints);

		for (int32 i = 0; i < NumPoints; ++i)
		{
			const int32 PrevIndex =
				(!bLoop && i == 0) ? 0 :
				(bLoop && i == 0) ? (NumPoints - 1) : (i - 1);

			const int32 NextIndex =
				(!bLoop && i == NumPoints - 1) ? (NumPoints - 1) :
				(bLoop && i == NumPoints - 1) ? 0 : (i + 1);

			const FVector PrevPos = TrackSpline->GetLocationAtSplinePoint(PrevIndex, ESplineCoordinateSpace::Local);
			const FVector CurrPos = TrackSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);
			const FVector NextPos = TrackSpline->GetLocationAtSplinePoint(NextIndex, ESplineCoordinateSpace::Local);

			FVector TangentDir;

			if (!bLoop && (i == 0 || i == NumPoints - 1))
			{
				TangentDir = (i == 0) ? (NextPos - CurrPos) : (CurrPos - PrevPos);
			}
			else
			{
				TangentDir = (NextPos - PrevPos) * 0.5f;
			}

			if (!TangentDir.Normalize())
			{
				TangentDir = FVector::ForwardVector;
			}

			const float DistToPrev = FMath::Abs(
				TrackSpline->GetDistanceAlongSplineAtSplinePoint(i) -
				TrackSpline->GetDistanceAlongSplineAtSplinePoint(PrevIndex));

			const float DistToNext = FMath::Abs(
				TrackSpline->GetDistanceAlongSplineAtSplinePoint(NextIndex) -
				TrackSpline->GetDistanceAlongSplineAtSplinePoint(i));

			const float AvgSegLen = (DistToPrev + DistToNext) * 0.5f;
			NewTangents[i] = TangentDir * (AvgSegLen * Tension);
		}

		for (int32 i = 0; i < NumPoints; ++i)
		{
			TrackSpline->SetTangentAtSplinePoint(i, NewTangents[i], ESplineCoordinateSpace::Local, false);
			TrackSpline->SetSplinePointType(i, ESplinePointType::CurveCustomTangent, false);
		}

		TrackSpline->UpdateSpline();
	}

	ASYNC_LOG(Log, "SmoothSplineTangents: Applied %d pass(es), Tension=%.2f", Passes, Tension);
}

// ============================================================================
// Rotate spline points
// ============================================================================

void ASplineGeneratingActor::RotateSplinePointsForward()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "RotateSplinePointsForward: TrackSpline == NULL.");
		return;
	}

	const int32 Num = TrackSpline->GetNumberOfSplinePoints();
	if (Num < 2)
	{
		return;
	}

	TArray<FSplinePoint> Points;
	Points.Reserve(Num);

	for (int32 i = 0; i < Num; ++i)
	{
		Points.Add(MakeSplinePointLocal(i));
	}

	const FSplinePoint Last = Points.Last();
	Points.RemoveAt(Num - 1);
	Points.Insert(Last, 0);

	TrackSpline->ClearSplinePoints(false);
	for (const FSplinePoint& P : Points)
	{
		TrackSpline->AddPoint(P, false);
	}
	TrackSpline->UpdateSpline();

	ASYNC_LOG(Log, "RotateSplinePointsForward: Rotated %d spline points forward.", Num);

#if WITH_EDITOR
	if (bAutoRebuildOnConstruction)
	{
		RequestBuild();
	}
#endif
}

void ASplineGeneratingActor::RotateSplinePointsBackward()
{
	if (!TrackSpline)
	{
		ASYNC_LOG(Error, "RotateSplinePointsBackward: TrackSpline == NULL.");
		return;
	}

	const int32 Num = TrackSpline->GetNumberOfSplinePoints();
	if (Num < 2)
	{
		return;
	}

	TArray<FSplinePoint> Points;
	Points.Reserve(Num);

	for (int32 i = 0; i < Num; ++i)
	{
		Points.Add(MakeSplinePointLocal(i));
	}

	const FSplinePoint First = Points[0];
	Points.RemoveAt(0);
	Points.Add(First);

	TrackSpline->ClearSplinePoints(false);
	for (const FSplinePoint& P : Points)
	{
		TrackSpline->AddPoint(P, false);
	}
	TrackSpline->UpdateSpline();

	ASYNC_LOG(Log, "RotateSplinePointsBackward: Rotated %d spline points backward.", Num);

#if WITH_EDITOR
	if (bAutoRebuildOnConstruction)
	{
		RequestBuild();
	}
#endif
}

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/Selection.h"
#endif

#if WITH_EDITOR
void ASplineGeneratingActor::AddSelectedActorsToIgnoreList()
{
	if (!GEditor)
	{
		return;
	}

	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel)
	{
		return;
	}

	Modify();

	int32 Added = 0;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!IsValid(A) || A == this)
		{
			continue;
		}

		if (!ActorsToIgnoreForGenerationTraces.Contains(A))
		{
			ActorsToIgnoreForGenerationTraces.Add(A);
			++Added;
		}
	}

	if (Added > 0)
	{
		MarkPackageDirty();
		ASYNC_LOG(Log, "IgnoreList: added %d actor(s) from selection.", Added);
	}
}

void ASplineGeneratingActor::RemoveSelectedActorsFromIgnoreList()
{
	if (!GEditor)
	{
		return;
	}

	USelection* Sel = GEditor->GetSelectedActors();
	if (!Sel)
	{
		return;
	}

	Modify();

	int32 Removed = 0;
	for (FSelectionIterator It(*Sel); It; ++It)
	{
		AActor* A = Cast<AActor>(*It);
		if (!IsValid(A))
		{
			continue;
		}

		Removed += ActorsToIgnoreForGenerationTraces.Remove(A);
	}

	if (Removed > 0)
	{
		MarkPackageDirty();
		ASYNC_LOG(Log, "IgnoreList: removed %d actor(s) from selection.", Removed);
	}
}

void ASplineGeneratingActor::ClearIgnoreList()
{
	Modify();
	const int32 Prev = ActorsToIgnoreForGenerationTraces.Num();
	ActorsToIgnoreForGenerationTraces.Reset();
	MarkPackageDirty();
	ASYNC_LOG(Log, "IgnoreList: cleared (%d -> 0).", Prev);
}
#endif
