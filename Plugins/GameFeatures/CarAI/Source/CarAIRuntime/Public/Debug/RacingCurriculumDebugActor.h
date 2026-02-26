#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/RacingCurriculumTypes.h"
#include "RacingCurriculumDebugActor.generated.h"

class USplineComponent;

UENUM(BlueprintType)
enum class ERacingCurriculumVizMode : uint8
{
	Tags        UMETA(DisplayName = "Tags"),
	Curvature   UMETA(DisplayName = "Curvature"),
	Slope       UMETA(DisplayName = "Slope"),
	Difficulty  UMETA(DisplayName = "Difficulty"),
	SpawnScore  UMETA(DisplayName = "SpawnScore"),
	SpawnHotspots UMETA(DisplayName = "SpawnHotspots"),
};

USTRUCT()
struct FRC_DebugSample
{
	GENERATED_BODY()

	UPROPERTY() float  S = 0.f;
	UPROPERTY() FVector P = FVector::ZeroVector;
	UPROPERTY() FVector Forward = FVector::ForwardVector;
	UPROPERTY() FVector SurfaceNormal = FVector::UpVector;

	UPROPERTY() int32 TagMask = 0;
	UPROPERTY() float CurvNormAbs = 0.f;
	UPROPERTY() float SlopeZ = 0.f;
	UPROPERTY() float SpeedNorm = 0.f;
	UPROPERTY() float MaxSteer = 0.f;

	UPROPERTY() bool  bHasSurface = false;
	UPROPERTY() float SpawnScore01 = 0.f;
};

USTRUCT()
struct FRC_SpawnEvent
{
	GENERATED_BODY()

	UPROPERTY() float TimeSec = 0.f;
	UPROPERTY() TWeakObjectPtr<AActor> Agent;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float S = 0.f;
	UPROPERTY() int32 TagMask = 0;
	UPROPERTY() float Score = 0.f;
	UPROPERTY() FName Reason = NAME_None;
};

USTRUCT()
struct FRC_SpawnHotspot
{
	GENERATED_BODY()

	UPROPERTY() int32 Count = 0;
	UPROPERTY() FVector AvgLocation = FVector::ZeroVector;
	UPROPERTY() float SCenter = 0.f;
	UPROPERTY() FName LastReason = NAME_None;
	UPROPERTY() TWeakObjectPtr<AActor> LastAgent;
};

UCLASS()
class CARAIRUNTIME_API ARacingCurriculumDebugActor : public AActor
{
	GENERATED_BODY()

public:
	ARacingCurriculumDebugActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION(CallInEditor, Category = "Curriculum")
	void RebuildNow();

	UFUNCTION(CallInEditor, Category = "Curriculum")
	void ClearRecordedSpawns();

	/** Call this from your agent system whenever an agent spawns (optional but recommended for hotspot debug). */
	UFUNCTION(BlueprintNativeEvent, Category = "Curriculum|Spawns")
	void ReportAgentSpawn(AActor* Agent, const FTransform& SpawnWorldTransform, FName Reason, float Score);
	virtual void ReportAgentSpawn_Implementation(AActor* Agent, const FTransform& SpawnWorldTransform, FName Reason, float Score);

public:
	// -----------------------------
	// Source
	// -----------------------------
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Curriculum|Source")
	TObjectPtr<AActor> TrackActor = nullptr;

	/** If true, auto-find TrackActor by tag "Track" if TrackActor is null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Source")
	bool bAutoFindTrackActor = true;

	/** Prefer getting spline via RoadSplineInterface (recommended if your track actor implements it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Source")
	bool bPreferRoadSplineInterface = true;

	/** Fallback: pick the spline component by tag (if multiple splines exist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Source")
	FName TrackSplineComponentTag = FName("RoadSpline");

	/** Fallback: pick the spline component by exact name (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Source")
	FName TrackSplineComponentName = NAME_None;

	// -----------------------------
	// Draw
	// -----------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	bool bDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	ERacingCurriculumVizMode VizMode = ERacingCurriculumVizMode::SpawnScore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float DrawIntervalSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float DrawSampleStepCm = 150.f;

	/** Offset from surface along surface normal (or spline up if no surface) to avoid z-fighting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float DrawSurfaceOffsetCm = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float LineThickness = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	bool bDrawForwardArrows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float ArrowEveryCm = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float ArrowSize = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	bool bDrawLabels = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	int32 LabelEveryNSamples = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	float LabelZOffsetCm = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Draw")
	bool bDrawLegend = true;

	// -----------------------------
	// Surface Trace (crucial!)
	// -----------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	bool bUseSurfaceTrace = true;

	/** If true, trace along spline-up. If false, trace along world-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	bool bTraceAlongSplineUp = true;

	/** Extra offset applied BEFORE SurfaceTraceUpCm (helps if spline is inside mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	float PreTraceUpOffsetCm = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	float SurfaceTraceUpCm = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	float SurfaceTraceDownCm = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	bool bSurfaceTraceComplex = true;

	/** Prefer hits on TrackActor; if none found, optionally fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	bool bPreferTrackActorHits = true;

	/** Strongly recommend FALSE while debugging (prevents Landscape being treated as track). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	bool bFallbackToAnyHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	TArray<TObjectPtr<AActor>> TraceIgnoreActors;

	/** Draw trace debug every N samples (0 disables). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Surface Trace")
	int32 DebugDrawTraceEveryNSamples = 0;

	// -----------------------------
	// Spawn score heuristics
	// -----------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float SurfaceNormalUpMin = 0.75f; // reject wall-like surfaces

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float PitchBadDeg = 8.f;          // starts penalizing

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float PitchHardFailDeg = 14.f;    // ramps/jumps are forbidden

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float PitchExponent = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float CurvatureWindowCm = 300.f;

	/** Curvature considered "bad" (1/cm). Example: radius 12m => 1/1200cm^-1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float CurvatureBadInvCm = (1.f / 1200.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float CurvatureExponent = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float ScoreGreenMin = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float ScoreYellowMin = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|SpawnScore")
	float ScoreOrangeMin = 0.25f;

	// -----------------------------
	// Spawn candidates / hotspots
	// -----------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	bool bDrawSpawnCandidates = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float SpawnPointStepCm = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float SpawnPointRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float SpawnPointZOffsetCm = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	bool bDrawSpawnHotspots = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float HotspotBinSizeCm = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	int32 MaxRecordedSpawnEvents = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float HotspotBaseRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Curriculum|Spawns")
	float HotspotRadiusPerCount = 18.f;

protected:
	void RebuildInternal(bool bFromConstruction);
	void EnsureTrackSpline();
	void BuildSamples();
	void DrawInternal();

	// Draw helpers
	void DrawSpawnCandidates();
	void DrawSpawnHotspots();
	void RebuildHotspots();

	// Trace helpers
	float WrapDistance(float S, float LengthCm) const;
	bool TraceSurfaceAtDistance(float S, FVector& OutSurfacePos, FVector& OutSurfaceNormal) const;
	bool ChooseBestSurfaceHit(const TArray<FHitResult>& Hits, FHitResult& OutBest) const;

	// Analysis helpers
	float ComputeCurvatureInvCm(float S, float WindowCm, float LengthCm, FVector& InOutForward) const;
	int32 BuildTagMask(float S, float LengthCm, FVector& InOutForward, float& OutCurvNormAbs, float& OutSlopeZ) const;
	void ComputeSpeedAndSteerHints(int32 TagMask, float CurvNormAbs, float SlopeZ, float& OutSpeedNorm, float& OutMaxSteer) const;

	// Spawn scoring / colors
	float ComputeSpawnScore01(bool bHasHit, const FVector& SurfaceNormal, float CurvatureInvCm, float PitchDeg) const;
	FColor ColorForScore01(float Score01, bool bValidHit) const;
	
	// NoSpawnZone checking
	bool IsInAnyNoSpawnZone(const FVector& WorldPoint) const;

	// Visual palette
	FLinearColor HeatColor01(float T) const;
	FLinearColor ColorForTagMask(int32 TagMask) const;
	FString TagMaskToString(int32 TagMask) const;

private:
	UPROPERTY(Transient) TObjectPtr<USplineComponent> TrackSpline = nullptr;
	float CachedSplineLengthCm = 0.f;

	UPROPERTY(Transient) TArray<FRC_DebugSample> CachedSamples;
	UPROPERTY(Transient) TArray<FRacingCurriculumSegment> CachedSegments;

	// Recorded spawns
	UPROPERTY(Transient) TArray<FRC_SpawnEvent> SpawnEvents;
	UPROPERTY(Transient) TMap<int32, FRC_SpawnHotspot> HotspotsByBin;

	float DrawAccum = 0.f;
	bool bBuiltOnce = false;
};
