// Yves Tanas 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineComponent.h"
#include "Interfaces/RoadSplineInterface.h"
#include "TrackSplineData.h"

#include "SplineGeneratingActor.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAsyncSplineBuilder, Log, All);

class USceneComponent;
class USplineComponent;
class ALandscapeProxy;
class UStaticMesh;
class ULandscapeLayerInfoObject;
class USplineMeshComponent;
class USplinePointListAsset;
class UTextRenderComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UPhysicalMaterial;

// ---------------------------------------------------------
// Gaps / Drops
// ---------------------------------------------------------

USTRUCT(BlueprintType)
struct FStuntGapByPoints
{
	GENERATED_BODY()

	// Includes Start-Point, excludes End-Point:
	// Covers segments [StartPointIndex, EndPointIndex)
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Gaps")
	int32 StartPointIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Stunt Track|Gaps")
	int32 EndPointIndex = 0;
};

USTRUCT(BlueprintType)
struct FStuntDropByPoints
{
	GENERATED_BODY()

	// Applies to all segments [StartPointIndex, EndPointIndex)
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops")
	int32 StartPointIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops")
	int32 EndPointIndex = 0;

	// Negative = downwards, e.g. -400.f => 4 meters drop
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops")
	float DropHeight = -300.f;

	// Use a constant world Z for this drop region?
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops")
	bool bUseConstantGapHeight = false;

	// Absolute world Z (only if bUseConstantGapHeight)
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops", meta = (EditCondition = "bUseConstantGapHeight"))
	float ConstantGapWorldZ = 0.f;
};

UCLASS(
	hidecategories = (
		Display, Attachment, "LOD", "LOD|Advanced",
		Physics, Debug, Collision, Movement,
		Rendering, PrimitiveComponent, Object,
		Transform, Mobility, VirtualTexture,
		"Asset User Data", HLOD
		),
	showcategories = ("TrackTools", "Data")
)
class ASYNCSPLINEBUILDER_API ASplineGeneratingActor : public AActor, public IRoadSplineInterface
{
	GENERATED_BODY()

public:
	ASplineGeneratingActor();

#if WITH_EDITOR
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UFUNCTION(CallInEditor, Category = "TrackTools")
	void RebuildTrack();

	UFUNCTION(CallInEditor, Category = "TrackTools")
	void CancelAsyncBuild();

	// ---------------------------------------------------------
	// Data Asset
	// ---------------------------------------------------------
	UFUNCTION(CallInEditor, Category = "Data")
	void WriteSplineToDataAsset();

	UFUNCTION(CallInEditor, Category = "Data")
	void ReadSplineFromDataAsset();

protected:
	// ---------------------------------------------------------
	// Components
	// ---------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> TrackSpline = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	TObjectPtr<USplinePointListAsset> SplinePointList = nullptr;

protected:
	// ---------------------------------------------------------
	// TrackTools
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bEditSpline = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bShowSegmentNumbers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bShowPointNumbers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bShowSplineVisualizationWidth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools", meta = (ClampMin = "0.0"))
	float SplineVisualizationWidth = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bSnapPointsToLandscape = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bDeformLandscape = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bTangentPointsUpdate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	bool bMirrorExtraMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	TEnumAsByte<ESplinePointType::Type> SplinePointType = ESplinePointType::CurveCustomTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	float SplineZOffset = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "TrackTools")
	int32 SplineSegments = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	TArray<FTrackSplineData> TrackSplineData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	TObjectPtr<ALandscapeProxy> Landscape = nullptr;

	// ---------------------------------------------------------
	// Mesh Setup
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh Setup")
	TObjectPtr<UStaticMesh> StartMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh Setup")
	TObjectPtr<UStaticMesh> MainMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh Setup")
	TObjectPtr<UStaticMesh> EndMesh = nullptr;

	// ---------------------------------------------------------
	// Options
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bClosedLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bEnableCollision = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bCastShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bCastContactShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bMirrorMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Options")
	bool bSnapMeshesToLandscape = false;

	// ---------------------------------------------------------
	// Landscape / Trace (for snapping road meshes / points)
	// ---------------------------------------------------------


	/**
	 * Separate object types for GroundWalls projection.
	 * Example: SnapMeshes = Landscape-only, GroundWalls = WorldStatic.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectsToHitForGroundWallsLineTrace;

	/**
	 * Actors to ignore for ALL generator traces (snap + walls).
	 * Put your helper actors here (ramps, blockers, debug meshes, etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	TArray<TObjectPtr<AActor>> ActorsToIgnoreForGenerationTraces;

	/**
	 * If true, "snap" traces will only accept Landscape hits.
	 * (Helps when helper actors are WorldStatic but you still want Landscape snapping.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	bool bSnapTraceLandscapeOnly = false;

	/** Object types to consider for line traces when snapping / ground projection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectsToHitForLandscapeLineTrace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape", meta = (ClampMin = "0.0"))
	float FallOff = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape", meta = (ClampMin = "1"))
	int32 NumberOfSubdivisionsForDeform = 500;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	bool bRaiseHeights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	bool bLowerHeights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	TObjectPtr<ULandscapeLayerInfoObject> PaintLayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	FName LandscapeEditLayerName = FName("Layer");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	float SplineZOffsetLandscapeSnapCorrection = 100.f;

	/**
	 * End Z for trace (world Z).
	 * Example: -100000.f means trace down to Z=-100000.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape")
	float LineTraceLength = -100000.f;

	// ---------------------------------------------------------
	// Debug
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0.0"))
	float DebugTextWorldSize = 500.f;

	// ---------------------------------------------------------
	// Data
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	FName SplineListName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	bool bWriteToDataAsset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	bool bReadFromDataAsset = false;

	// ---------------------------------------------------------
	// Async Build
	// ---------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools|Async")
	bool bUseAsyncBuild = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools|Async", meta = (EditCondition = "bUseAsyncBuild", ClampMin = "1", UIMin = "1"))
	int32 SegmentsPerTick = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools|Async")
	bool bAutoRebuildOnConstruction = true;

	UPROPERTY(Transient)
	bool bIsBuilding = false;

	UPROPERTY(Transient)
	bool bPendingRebuild = false;

	UPROPERTY(Transient)
	int32 CurrentBuildSegmentIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USplineMeshComponent>> GeneratedSplineMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> GeneratedDebugText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> GeneratedDropWalls;

private:
	double LastRebuildRequestTime = 0.0;
	double RebuildDelaySeconds = 0.1;

	// ---------------------------------------------------------
	// Ground Walls
	// ---------------------------------------------------------
protected:
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls")
	bool bGenerateGroundWalls = true;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls", meta = (ClampMin = "4", ClampMax = "4096"))
	int32 GroundWallSubdivisions = 64;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls")
	float GroundWallOutset = 0.f;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls", meta = (ClampMin = "0.0"))
	float GroundWallFallbackDepth = 20000.f;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls")
	bool bGroundWallsDoubleSided = true;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls")
	TObjectPtr<UMaterialInterface> GroundWallMaterial = nullptr;

	/** Separate trace settings for GroundWalls (can hit WorldStatic etc.) */
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|Trace")
	TArray<TEnumAsByte<EObjectTypeQuery>> GroundWallObjectsToHitForLineTrace;

	/** End Z for GroundWall trace (world Z). */
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|Trace")
	float GroundWallLineTraceEndWorldZ = -100000.f;

	/** UV tiling in world units (U along track length). */
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|UV", meta = (ClampMin = "1.0"))
	float GroundWallUVWorldSizeU = 1000.f;

	/** UV tiling in world units (V along wall height). */
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|UV", meta = (ClampMin = "1.0"))
	float GroundWallUVWorldSizeV = 500.f;

	// ---------------------------------------------------------
	// Ground Walls - UV
	// ---------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|UV", meta = (ClampMin = "1.0"))
	float GroundWallUVWorldUnitsU = 1000.f;   // tiling along spline length

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|GroundWalls|UV", meta = (ClampMin = "1.0"))
	float GroundWallUVWorldUnitsV = 300.f;    // tiling by real wall height

	// ---------------------------------------------------------
	// Drop Walls - UV
	// ---------------------------------------------------------
	UPROPERTY(EditAnywhere, Category = "AsyncSpline|Drop Walls|UV", meta = (ClampMin = "1.0"))
	float DropWallUVWorldUnitsU = 1000.f;

	UPROPERTY(EditAnywhere, Category = "AsyncSpline|Drop Walls|UV", meta = (ClampMin = "1.0"))
	float DropWallUVWorldUnitsV = 300.f;


	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> LeftGroundWall = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> RightGroundWall = nullptr;

	void BuildGroundWalls();
	void BuildSingleGroundWall(int32 SideSign, TObjectPtr<UProceduralMeshComponent>& OutComp);
	void ClearGroundWalls();

	float GetHalfRoadWidthAtDistance(float DistanceAlongSpline) const;

	// ---------------------------------------------------------
	// Stunt Gaps / Drops
	// ---------------------------------------------------------
protected:
	UPROPERTY(EditAnywhere, Category = "Stunt Track|Gaps")
	TArray<FStuntGapByPoints> JumpGapsByPoints;

	UPROPERTY(EditAnywhere, Category = "Stunt Track|Drops")
	TArray<FStuntDropByPoints> StuntDropsByPoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AsyncSpline|Drop Walls")
	TObjectPtr<UMaterialInterface> DropWallMaterial = nullptr;

	/** Optional UV tiling for drop walls as well (prevents stretching). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AsyncSpline|Drop Walls|UV", meta = (ClampMin = "1.0"))
	float DropWallUVWorldSizeU = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AsyncSpline|Drop Walls|UV", meta = (ClampMin = "1.0"))
	float DropWallUVWorldSizeV = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TrackTools")
	TObjectPtr<UPhysicalMaterial> RoadPhysicalMaterial = nullptr;

	// ---------------------------------------------------------
	// Tangent smoothing
	// ---------------------------------------------------------
protected:
	UPROPERTY(EditAnywhere, Category = "TrackTools|Tangents", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TangentSmoothingTension = 1.0f;

	UPROPERTY(EditAnywhere, Category = "TrackTools|Tangents", meta = (ClampMin = "1", ClampMax = "10"))
	int32 TangentSmoothingIterations = 1;

	UFUNCTION(CallInEditor, Category = "TrackTools|Tangents")
	void SmoothSplineTangents();

	UFUNCTION(CallInEditor, Category = "StuntCarRacer|Spline")
	void RotateSplinePointsForward();

	UFUNCTION(CallInEditor, Category = "StuntCarRacer|Spline")
	void RotateSplinePointsBackward();

private:
	// ---------------------------------------------------------
	// Core build helpers (keep structure)
	// ---------------------------------------------------------
	void CleanData();
	void BuildArrayOfSplineSegments();
	void CalculateSegmentsAndSetArray();

	float CalculateSplineSegmentLength(const int32 SegmentIndex) const;

	void AddRoadAndExtraMeshesToSpline();
	void BuildSplineMeshComponents(const int32 SegmentIndex);
	void BuildExtraSplineMeshComponent(const int32 SegmentIndex, const int32 MeshIndex);

	void DebugTrackSpline();
	void ShowSegmentNumbers();

	void SnapToLandscape();

	bool LineTraceHitLandscape(const FVector& StartPoint, FVector& ImpactPoint, FVector& ImpactNormal) const;
	bool LineTraceHitGroundForWalls(const FVector& StartPoint, FVector& ImpactPoint, FVector& ImpactNormal) const;

	void UpdateSpline() const;
	float GetDivisor() const;

	// Async control
	void RequestBuild();
	void StartBuild_Internal();
	void BuildNextSegments(int32 NumSegments);
	void FinishBuild_Internal();

	// Cleanup
	void ClearGeneratedMeshes();
	void ClearDebugText();
	void ClearGeneratedComponents();

	// Drop walls
	void BuildDropCliffWalls();
	void BuildDropCliffWallAtDistance(float DistanceAlongSpline, float FromWorldZ, float ToWorldZ);

	// Segment/drops helpers
	int32 GetSegmentIndexFromDistance(float Distance) const;
	bool IsSegmentInsideJumpGapByPoints(int32 SegmentIndex) const;

	bool GetDropInfoForSegmentByPoints(
		int32 SegmentIndex,
		float& OutDropOffset,
		bool& bOutUseConstantHeight,
		float& OutConstantGapWorldZ
	) const;

	// ---------------------------------------------------------
	// Spline Point Helpers (restore original intent)
	// ---------------------------------------------------------
	FSplinePoint MakeSplinePointLocal(int32 PointIndex) const;

	// ---------------------------------------------------------
	// Component creation helpers (prevents silent render failures)
	// ---------------------------------------------------------
	USplineMeshComponent* CreateSplineMeshComponent();
	UTextRenderComponent* CreateTextComponent(const FTransform& WorldTransform);
	UProceduralMeshComponent* CreateProcMeshComponent(FName DebugName);

	// Local conversion helpers for spline mesh (expects LOCAL)
	FVector WorldToActorLocalPos(const FVector& WorldPos) const;
	FVector WorldToActorLocalDir(const FVector& WorldDir) const;

private:
	void BuildTraceIgnoreActors(TArray<AActor*>& OutIgnore) const;

	bool LineTraceSingleForObjectsEx(
		const FVector& StartWorld,
		FVector& OutImpactPoint,
		FVector& OutImpactNormal,
		const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
		bool bLandscapeOnly
	) const;

	static bool IsLandscapeHit(const FHitResult& Hit);

	virtual class USplineComponent* GetRoadSpline_Implementation() const override;
protected:
#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Landscape|Ignore")
	void AddSelectedActorsToIgnoreList();

	UFUNCTION(CallInEditor, Category = "Landscape|Ignore")
	void RemoveSelectedActorsFromIgnoreList();

	UFUNCTION(CallInEditor, Category = "Landscape|Ignore")
	void ClearIgnoreList();
#endif

};

