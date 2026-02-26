// Yves Tanas 2025 All Rights Reserved.

/**
 * This file is part of the "AsyncSplineBuilder" plugin.
 *
 * Use of this software is governed by the Fab Standard End User License Agreement
 * (EULA) applicable to this product, available at:
 * https://www.fab.com/eula
 *
 * Except as expressly permitted by the Fab Standard EULA, any reproduction,
 * distribution, modification, or use of this software, in whole or in part,
 * is strictly prohibited.
 *
 * This software is provided on an "AS IS" basis, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied, including but not
 * limited to warranties of merchantability, fitness for a particular purpose,
 * and non-infringement.
 *
 * -------------------------------------------------------------------------------
 * File:        [TrackSplineData.h]
 * Description: Defines data structures for configuring track spline mesh segments
 *              and associated extra static mesh instances.
 * -------------------------------------------------------------------------------
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "TrackSplineData.generated.h"

class USplineMeshComponent;
class UStaticMesh;

/**
 * FGroundWallTraceSettings
 *
 * Separate trace settings specifically for "GroundWalls".
 * This allows SnapMeshes / Landscape snapping to stay "Landscape-only",
 * while walls can hit WorldStatic (or any other object types).
 */
USTRUCT(BlueprintType)
struct FGroundWallTraceSettings
{
	GENERATED_BODY()

public:
	/** Object types that the wall trace is allowed to hit (e.g. WorldStatic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectsToHit;

	/**
	 * Trace length (in cm / Unreal Units) from the top edge downwards.
	 * Larger values allow very deep cliffs.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "0.0"))
	float TraceLength = 250000.f;

	/** Whether the trace should be complex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bTraceComplex = false;

	FGroundWallTraceSettings()
	{
		// Sensible default: WorldStatic.
		// Users can still override this in the Details panel.
		ObjectsToHit.Add(EObjectTypeQuery::ObjectTypeQuery1);
	}
};

/**
 * FGroundWallUVSettings
 *
 * UV scaling in world units.
 * The key point: V tiling is based on the REAL wall height,
 * so materials do not stretch for big drops/cliffs.
 */
USTRUCT(BlueprintType)
struct FGroundWallUVSettings
{
	GENERATED_BODY()

public:
	/**
	 * UV tiling world size for U (along track direction), in cm.
	 * Example: 200 means the texture repeats every 2 meters.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "1.0"))
	float UVWorldSizeU = 200.f;

	/**
	 * UV tiling world size for V (wall height), in cm.
	 * Example: 200 means the texture repeats every 2 meters vertically.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "1.0"))
	float UVWorldSizeV = 200.f;

	/** Optional flips for quick alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bFlipU = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bFlipV = false;
};

/**
 * FGroundWallSettings
 *
 * High-level wall generation configuration.
 */
USTRUCT(BlueprintType)
struct FGroundWallSettings
{
	GENERATED_BODY()

public:
	/** Enable/disable ground walls generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bGenerateGroundWalls = true;

	/** If true, generate walls on both sides of the track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bGenerateBothSides = true;

	/**
	 * Half width of the track used to compute the left/right edges for walls.
	 * (Fallback if you don't derive width from the mesh bounds)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "0.0"))
	float TrackHalfWidth = 500.f;

	/** Minimum wall height required to generate geometry (skip tiny height deltas). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "0.0"))
	float MinWallHeight = 25.f;

	/** Optional max wall height clamp (safety). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default, meta = (ClampMin = "0.0"))
	float MaxWallHeight = 250000.f;

	/** Whether walls should create collision (ProceduralMesh collision). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	bool bCreateCollision = true;

	/** Trace settings specifically for ground walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	FGroundWallTraceSettings TraceSettings;

	/** UV settings for walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	FGroundWallUVSettings UVSettings;
};

/**
 * FSplineMeshComponentData
 *
 * Stores all geometric information needed to reconstruct or update a spline
 * mesh component and any additional spline mesh components that belong to the
 * same logical segment.
 *
 * The arrays (LocationStarts, LocationEnds, TangentStarts, TangentEnds) are
 * expected to be aligned by index:
 * - Index i in each array describes one spline segment of SplineMeshComponent,
 *   or (depending on usage) one variation of the same base component.
 *
 * This struct is primarily intended as a runtime/async data container for
 * the AsyncSplineBuilder systems.
 */
USTRUCT(BlueprintType)
struct FSplineMeshComponentData
{
	GENERATED_BODY()

public:
	/** Start locations for each spline segment controlled by this data set. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FVector> LocationStarts;

	/** End locations for each spline segment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FVector> LocationEnds;

	/** Start tangents for each spline segment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FVector> TangentStarts;

	/** End tangents for each spline segment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TArray<FVector> TangentEnds;

	/**
	 * Primary spline mesh component.
	 * Transient: runtime-only pointer.
	 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TObjectPtr<USplineMeshComponent> SplineMeshComponent = nullptr;

	/**
	 * Extra spline mesh components (guard rails, etc.).
	 * Transient: runtime-only pointers.
	 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<USplineMeshComponent>> ExtraMeshComponents;

	FSplineMeshComponentData() = default;
};

/**
 * FTrackSplineData
 *
 * High-level configuration for how a spline-based racetrack (or road) segment
 * should be populated with meshes.
 */
USTRUCT(BlueprintType)
struct FTrackSplineData
{
	GENERATED_BODY()

public:
	/** Number of road mesh instances to place along the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	int32 MeshInstances = 0;

	/** Logical length of a segment along the spline, in Unreal units (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	float SegmentLength = 1.f;

	/** Physical length of the base road static mesh in Unreal units (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	float RoadMeshLength = 1400.f;

	/** Optional static meshes placed at the start of the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<UStaticMesh>> ExtraMeshStart;

	/** Static meshes placed repeatedly along the spline between start/end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<UStaticMesh>> ExtraMesh;

	/** Optional static meshes placed at the end of the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<TObjectPtr<UStaticMesh>> ExtraMeshEnd;

	/**
	 * Offsets used when placing extra meshes relative to the spline.
	 * Default contains a single entry 0.0 to avoid empty-array pitfalls.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TArray<float> ExtraMeshOffset;

	/** Ground wall settings can be configured per track config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	FGroundWallSettings GroundWallSettings;

	FTrackSplineData()
	{
		ExtraMeshOffset.Add(0.f);
	}
};
