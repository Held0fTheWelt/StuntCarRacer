#include "Components/TrackFrameProviderComponent.h"

#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#include "Interfaces/RoadSplineInterface.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TrackFrameProviderComponent, Log, All);

#define TFP_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TrackFrameProviderComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

static float WrapDistanceOnSpline(float S, float Len)
{
	if (Len <= 0.0f)
	{
		return 0.0f;
	}

	S = FMath::Fmod(S, Len);
	if (S < 0.0f)
	{
		S += Len;
	}
	return S;
}

UTrackFrameProviderComponent::UTrackFrameProviderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// We'll dynamically enable/disable tick based on debug flag.
	SetComponentTickEnabled(true);
}

void UTrackFrameProviderComponent::BeginPlay()
{
	Super::BeginPlay();

	// If debug is off, ticking is not needed.
	SetComponentTickEnabled(bDrawDebug);

	const bool bOk = ResolveRoadSpline();

	if(bDebug)
		TFP_LOGFMT(Log,
			"ResolveRoadSpline: {Result} (Provider={Provider}, Spline={Spline}, Tag={Tag})",
			("Result", bOk ? "OK" : "FAIL"),
			("Provider", GetNameSafe(RoadSplineProviderActor)),
			("Spline", GetNameSafe(CachedSpline)),
			("Tag", RequiredProviderTag.ToString())
	);
}

void UTrackFrameProviderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDrawDebug)
	{
		// In case user toggles it at runtime
		SetComponentTickEnabled(false);
		return;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Force compute (also updates internal progress tracking)
	ComputeFrameAtLocation(Owner->GetActorLocation(), Owner->GetActorForwardVector(), true);

	const TArray<float> Offsets = { 500.f, 1500.f, 3000.f };
	TArray<FTrackFrame> Lookahead;

	const bool bOk = SampleLookaheadByWorldLocation(
		Owner->GetActorLocation(),
		Owner->GetActorForwardVector(),
		Offsets,
		Lookahead,
		false // wrap
	);

	if (!bOk || Lookahead.Num() < Offsets.Num())
	{
		TFP_LOGFMT(Verbose, "Lookahead sampling failed or returned insufficient frames. Ok={Ok}, Frames={Frames}",
			("Ok", bOk),
			("Frames", Lookahead.Num()));
		return;
	}

	// Throttle debug log
	if (GetWorld())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (LastLogTimeSeconds < 0.0 || (Now - LastLogTimeSeconds) > DebugLogIntervalSeconds)
		{
			LastLogTimeSeconds = Now;

			TFP_LOGFMT(Log,
				"Lookahead: +{O0}cm -> Head={H0}rad | +{O1}cm -> Head={H1}rad | +{O2}cm -> Head={H2}rad",
				("O0", Offsets[0]), ("H0", Lookahead[0].HeadingError),
				("O1", Offsets[1]), ("H1", Lookahead[1].HeadingError),
				("O2", Offsets[2]), ("H2", Lookahead[2].HeadingError)
			);
		}
	}
}

void UTrackFrameProviderComponent::RefreshSplineLength()
{
	CachedSplineLength = (CachedSpline ? CachedSpline->GetSplineLength() : 0.0f);
}

bool UTrackFrameProviderComponent::ResolveRoadSpline()
{
	// 1) explicit override
	if (RoadSplineProviderActor && RoadSplineProviderActor->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
	{
		USplineComponent* Spline = IRoadSplineInterface::Execute_GetRoadSpline(RoadSplineProviderActor);
		if (Spline)
		{
			CachedSpline = Spline;
			RefreshSplineLength();
			return true;
		}
	}

	// 2) owner
	if (AActor* Owner = GetOwner())
	{
		if (Owner->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
		{
			USplineComponent* Spline = IRoadSplineInterface::Execute_GetRoadSpline(Owner);
			if (Spline)
			{
				RoadSplineProviderActor = Owner;
				CachedSpline = Spline;
				RefreshSplineLength();
				return true;
			}
		}
	}

	// 3) auto-find
	if (bAutoFindRoadSplineProvider)
	{
		UWorld* World = GetWorld();
		AActor* Owner = GetOwner();
		if (!World || !Owner)
		{
			return false;
		}

		const FVector OwnerLoc = Owner->GetActorLocation();

		AActor* BestActor = nullptr;
		USplineComponent* BestSpline = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor) continue;

			if (!Actor->GetClass()->ImplementsInterface(URoadSplineInterface::StaticClass()))
			{
				continue;
			}

			if (!RequiredProviderTag.IsNone() && !Actor->ActorHasTag(RequiredProviderTag))
			{
				continue;
			}

			USplineComponent* Spline = IRoadSplineInterface::Execute_GetRoadSpline(Actor);
			if (!Spline)
			{
				continue;
			}

			const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), OwnerLoc);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestActor = Actor;
				BestSpline = Spline;
			}
		}

		if (BestSpline)
		{
			RoadSplineProviderActor = BestActor;
			CachedSpline = BestSpline;
			RefreshSplineLength();

			TFP_LOGFMT(Verbose, "Picked Provider={Provider} (dist={DistM}m)",
				("Provider", GetNameSafe(BestActor)),
				("DistM", FMath::Sqrt(BestDistSq) / 100.f));

			return true;
		}
	}

	return false;
}

void UTrackFrameProviderComponent::ResetProgressTracking(float InitialDistanceAlongSpline)
{
	LastDistanceAlongSpline = InitialDistanceAlongSpline;
	bHasLastDistance = true;
}

float UTrackFrameProviderComponent::FindClosestDistanceAlongSpline(const FVector& WorldLocation) const
{
	if (!CachedSpline)
	{
		return 0.0f;
	}

	const float Key = CachedSpline->FindInputKeyClosestToWorldLocation(WorldLocation);
	return CachedSpline->GetDistanceAlongSplineAtSplineInputKey(Key);
}

float UTrackFrameProviderComponent::WrapProgressDelta(float Delta, float SplineLen)
{
	if (SplineLen <= 0.0f) return Delta;

	const float Half = 0.5f * SplineLen;
	if (Delta < -Half) Delta += SplineLen;
	if (Delta > Half) Delta -= SplineLen;
	return Delta;
}

float UTrackFrameProviderComponent::SignedAngleRadAroundAxis(const FVector& From, const FVector& To, const FVector& Axis)
{
	const FVector F = From.GetSafeNormal();
	const FVector T = To.GetSafeNormal();
	const FVector A = Axis.GetSafeNormal();

	const float Sin = FVector::DotProduct(FVector::CrossProduct(F, T), A);
	const float Cos = FVector::DotProduct(F, T);
	return FMath::Atan2(Sin, Cos);
}

FTrackFrame UTrackFrameProviderComponent::ComputeFrameAtLocation(
	const FVector& WorldLocation,
	const FVector& ForwardVector,
	bool bUpdateProgressTracking
)
{
	FTrackFrame Out;

	if (!CachedSpline && !ResolveRoadSpline())
	{
		return Out;
	}

	if (CachedSplineLength <= 0.0f)
	{
		RefreshSplineLength();
	}

	const float S = FindClosestDistanceAlongSpline(WorldLocation);
	Out.DistanceAlongSpline = S;

	Out.ClosestPoint = CachedSpline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);

	const FVector Tangent = CachedSpline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal();

	// Use spline rotation for banking/up
	const FRotator Rot = CachedSpline->GetRotationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
	const FVector Up = Rot.RotateVector(FVector::UpVector).GetSafeNormal();

	const FVector Right = FVector::CrossProduct(Up, Tangent).GetSafeNormal();
	const FVector Normal = FVector::CrossProduct(Tangent, Right).GetSafeNormal();

	Out.Tangent = Tangent;
	Out.Right = Right;
	Out.Normal = Normal;

	const FVector Delta = WorldLocation - Out.ClosestPoint;
	Out.LateralError = FVector::DotProduct(Delta, Right);
	Out.HeadingError = SignedAngleRadAroundAxis(Tangent, ForwardVector.GetSafeNormal(), Normal);

	Out.DistanceToClosestPoint = FVector::Dist(WorldLocation, Out.ClosestPoint);

	// Progress tracking (optional)
	if (!bUpdateProgressTracking)
	{
		Out.ProgressDelta = 0.0f;
	}
	else
	{
		if (!bHasLastDistance)
		{
			LastDistanceAlongSpline = S;
			bHasLastDistance = true;
			Out.ProgressDelta = 0.0f;
		}
		else
		{
			float DeltaS = S - LastDistanceAlongSpline;
			DeltaS = WrapProgressDelta(DeltaS, CachedSplineLength);
			Out.ProgressDelta = DeltaS;
			LastDistanceAlongSpline = S;
		}
	}

	// Re-resolve if far away
	if (ReResolveIfDistanceAboveCm > 0.0f && Out.DistanceToClosestPoint > ReResolveIfDistanceAboveCm)
	{
		CachedSpline = nullptr;
		CachedSplineLength = 0.0f;
		ResolveRoadSpline();
	}

	// Debug draw
	if (bDrawDebug && GetWorld())
	{
		const float LifeTime = DebugLifeTime;
		const float AxLen = DebugAxisLength;
		const FVector Offset(0.f, 0.f, 25.f);

		DrawDebugLine(GetWorld(), Out.ClosestPoint + Offset, Out.ClosestPoint + Out.Tangent * AxLen + Offset, FColor::Green, false, LifeTime, 0, 2.0f);
		DrawDebugLine(GetWorld(), Out.ClosestPoint + Offset, Out.ClosestPoint + Out.Right * AxLen + Offset, FColor::Red, false, LifeTime, 0, 2.0f);
		DrawDebugLine(GetWorld(), Out.ClosestPoint + Offset, Out.ClosestPoint + Out.Normal * AxLen + Offset, FColor::Blue, false, LifeTime, 0, 2.0f);
	}

	return Out;
}

FTrackFrame UTrackFrameProviderComponent::ComputeFrameAtDistance(float DistanceAlongSpline, const FVector& ForwardVector)
{
	FTrackFrame Out;

	if (!CachedSpline && !ResolveRoadSpline())
	{
		return Out;
	}
	if (CachedSplineLength <= 0.0f)
	{
		RefreshSplineLength();
	}
	if (!CachedSpline || CachedSplineLength <= 0.0f)
	{
		return Out;
	}

	const float S = WrapDistanceOnSpline(DistanceAlongSpline, CachedSplineLength);
	Out.DistanceAlongSpline = S;

	Out.ClosestPoint = CachedSpline->GetLocationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);

	const FVector Tangent = CachedSpline->GetDirectionAtDistanceAlongSpline(S, ESplineCoordinateSpace::World).GetSafeNormal();
	const FRotator Rot = CachedSpline->GetRotationAtDistanceAlongSpline(S, ESplineCoordinateSpace::World);
	const FVector Up = Rot.RotateVector(FVector::UpVector).GetSafeNormal();

	const FVector Right = FVector::CrossProduct(Up, Tangent).GetSafeNormal();
	const FVector Normal = FVector::CrossProduct(Tangent, Right).GetSafeNormal();

	Out.Tangent = Tangent;
	Out.Right = Right;
	Out.Normal = Normal;

	Out.LateralError = 0.0f; // by distance only
	Out.HeadingError = SignedAngleRadAroundAxis(Tangent, ForwardVector.GetSafeNormal(), Normal);
	Out.ProgressDelta = 0.0f;

	return Out;
}

bool UTrackFrameProviderComponent::SampleLookaheadByDistance(
	float BaseDistanceAlongSpline,
	const TArray<float>& OffsetsCm,
	TArray<FTrackFrame>& OutFrames,
	bool bClampToSplineLength
)
{
	OutFrames.Reset();

	if (!CachedSpline && !ResolveRoadSpline())
	{
		return false;
	}
	if (CachedSplineLength <= 0.0f)
	{
		RefreshSplineLength();
	}
	if (!CachedSpline || CachedSplineLength <= 0.0f)
	{
		return false;
	}

	OutFrames.Reserve(OffsetsCm.Num());

	const FVector Fwd = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;

	for (const float Offset : OffsetsCm)
	{
		float TargetS = BaseDistanceAlongSpline + Offset;

		if (bClampToSplineLength)
		{
			TargetS = FMath::Clamp(TargetS, 0.0f, CachedSplineLength);
		}
		else
		{
			TargetS = WrapDistanceOnSpline(TargetS, CachedSplineLength);
		}

		FTrackFrame Frame = ComputeFrameAtDistance(TargetS, Fwd);
		OutFrames.Add(Frame);

		if (bDrawDebug && GetWorld())
		{
			DrawDebugSphere(GetWorld(), Frame.ClosestPoint, 45.f, 10, FColor::White, false, 0.05f, 0, 1.5f);
		}
	}

	return true;
}

bool UTrackFrameProviderComponent::SampleLookaheadByWorldLocation(
	const FVector& WorldLocation,
	const FVector& ForwardVector,
	const TArray<float>& OffsetsCm,
	TArray<FTrackFrame>& OutFrames,
	bool bClampToSplineLength
)
{
	// Guard against stale ptrs
	if (!IsValid(CachedSpline))
	{
		CachedSpline = nullptr;
		CachedSplineLength = 0.0f;
	}

	if (!CachedSpline && !ResolveRoadSpline())
	{
		TFP_LOGFMT(Warning, "SampleLookaheadByWorldLocation: no valid spline (Owner={Owner})",
			("Owner", GetNameSafe(GetOwner())));
		return false;
	}

	OutFrames.Reset();

	if (CachedSplineLength <= 0.0f)
	{
		RefreshSplineLength();
	}
	if (!CachedSpline || CachedSplineLength <= 0.0f)
	{
		TFP_LOGFMT(Warning, "SampleLookaheadByWorldLocation: invalid spline length");
		return false;
	}

	const float BaseS = FindClosestDistanceAlongSpline(WorldLocation);

	OutFrames.Reserve(OffsetsCm.Num());
	for (const float Offset : OffsetsCm)
	{
		float TargetS = BaseS + Offset;

		if (bClampToSplineLength)
		{
			TargetS = FMath::Clamp(TargetS, 0.0f, CachedSplineLength);
		}
		else
		{
			TargetS = WrapDistanceOnSpline(TargetS, CachedSplineLength);
		}

		FTrackFrame Frame = ComputeFrameAtDistance(TargetS, ForwardVector);
		OutFrames.Add(Frame);

		if (bDrawDebug && GetWorld())
		{
			DrawDebugSphere(GetWorld(), Frame.ClosestPoint, 45.f, 10, FColor::White, false, 0.05f, 0, 1.5f);
			DrawDebugLine(GetWorld(), Frame.ClosestPoint, Frame.ClosestPoint + Frame.Tangent * 120.f, FColor::Green, false, 0.05f, 0, 2.0f);
		}
	}

	return true;
}
// ===============================
// Progress API (RL-safe)
// ===============================

void UTrackFrameProviderComponent::UpdateProgressAtLocation(const FVector& WorldLocation)
{
	//UE_LOG(LogTemp, Warning,
	//	TEXT("TrackProvider UpdateProgress CALLED at %s"),
	//	*WorldLocation.ToString()
	//);
	if (!CachedSpline && !ResolveRoadSpline())
	{
		return;
	}

	if (CachedSplineLength <= 0.f)
	{
		RefreshSplineLength();
	}

	const float S = FindClosestDistanceAlongSpline(WorldLocation);

	if (!bHasLastDistance)
	{
		LastDistanceAlongSpline = S;
		bHasLastDistance = true;
		PendingProgressDelta = 0.f;
		return;
	}

	float DeltaS = S - LastDistanceAlongSpline;
	DeltaS = WrapProgressDelta(DeltaS, CachedSplineLength);

	PendingProgressDelta = DeltaS;
	LastDistanceAlongSpline = S;
}

float UTrackFrameProviderComponent::ConsumeProgressDeltaCm()
{
	//UE_LOG(LogTemp, Warning,
	//	TEXT("ConsumeProgressDeltaCm = %.3f"), PendingProgressDelta
	//);
	const float Out = PendingProgressDelta;
	PendingProgressDelta = 0.f;
	return Out;
}
