// Yves Tanas 2025 All Rights Reserved.

#include "Data/RacingCurriculumDataAsset.h"
#include "Logging/GlobalLog.h"          // deine eigenen Zeit/Klassen/Line Makros
#include "Logging/LogVerbosity.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogRacingCurriculum, Log, All);

// ------------------------------------------------------------
// Logging Makro (dein Format)
// ------------------------------------------------------------
#define CUR_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(LogRacingCurriculum, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

// ------------------------------------------------------------
// Constructor helpers
// ------------------------------------------------------------

void URacingCurriculumDataAsset::PostLoad()
{
	Super::PostLoad();

	if (bDumpOnLoad)
	{
		DumpTagStats();
		CUR_LOGFMT(Log, "Dumped curriculum tag stats on load for {Asset}", ("Asset", GetNameSafe(this)));
	}
}

#if WITH_EDITOR
void URacingCurriculumDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CUR_LOGFMT(Verbose, "Property changed in {Asset}, cache may need rebuild.", ("Asset", GetNameSafe(this)));
}
#endif

// ------------------------------------------------------------
// Core API
// ------------------------------------------------------------

bool URacingCurriculumDataAsset::FindSegmentAtDistance(float DistanceCm, FRacingCurriculumSegment& OutSegment) const
{
	if (Segments.IsEmpty() || SplineLengthCm <= 1.f)
	{
		CUR_LOGFMT(Warning, "FindSegmentAtDistance failed: empty data (Segs={N}, Len={Len})",
			("N", Segments.Num()), ("Len", SplineLengthCm));
		return false;
	}

	float S = DistanceCm;
	if (BuildSettings.bLoopedTrack)
	{
		S = FMath::Fmod(S, SplineLengthCm);
		if (S < 0.f)
			S += SplineLengthCm;
	}
	else
	{
		S = FMath::Clamp(S, 0.f, SplineLengthCm);
	}

	for (const FRacingCurriculumSegment& Seg : Segments)
	{
		if (S >= Seg.StartDistanceCm && S <= Seg.EndDistanceCm)
		{
			OutSegment = Seg;
			CUR_LOGFMT(VeryVerbose, "Segment found: {Start}-{End} (S={S})",
				("Start", Seg.StartDistanceCm), ("End", Seg.EndDistanceCm), ("S", S));
			return true;
		}
	}

	CUR_LOGFMT(VeryVerbose, "No segment found at S={S}", ("S", S));
	return false;
}

bool URacingCurriculumDataAsset::GetRandomDistanceInTag(
	int32 InMask,
	float& OutDistanceCm,
	FRandomStream& Rng,
	bool bRequireAllTags /*= false*/
) const
{
	OutDistanceCm = 0.f;

	if (InMask == 0 || Segments.IsEmpty() || SplineLengthCm <= 1.f)
	{
		CUR_LOGFMT(Warning, "GetRandomDistanceInTag failed: invalid parameters. Mask=0x{Mask} Segs={Segs} Len={Len}",
			("Mask", InMask), ("Segs", Segments.Num()), ("Len", SplineLengthCm));
		return false;
	}

	TArray<int32> CandidateIdx;
	CandidateIdx.Reserve(Segments.Num());

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const FRacingCurriculumSegment& Seg = Segments[i];
		const bool bMatch = bRequireAllTags
			? ((Seg.TagMask & InMask) == InMask)
			: ((Seg.TagMask & InMask) != 0);

		if (bMatch)
		{
			CandidateIdx.Add(i);
		}
	}

	if (CandidateIdx.IsEmpty())
	{
		CUR_LOGFMT(Warning, "No candidates found for Mask=0x{Mask} RequireAll={All}",
			("Mask", InMask), ("All", bRequireAllTags ? 1 : 0));
		return false;
	}

	const FRacingCurriculumSegment& PickSeg = Segments[CandidateIdx[Rng.RandRange(0, CandidateIdx.Num() - 1)]];
	OutDistanceCm = FMath::Clamp(Rng.FRandRange(PickSeg.StartDistanceCm, PickSeg.EndDistanceCm), 0.f, SplineLengthCm);

	CUR_LOGFMT(Verbose, "Picked random distance: Mask=0x{Mask}, Range=[{Start},{End}] Out={Out}",
		("Mask", InMask), ("Start", PickSeg.StartDistanceCm), ("End", PickSeg.EndDistanceCm), ("Out", OutDistanceCm));

	return true;
}

void URacingCurriculumDataAsset::DumpTagStats() const
{
	TMap<int32, int32> Counts;
	TMap<int32, float> TotalLength;

	for (const FRacingCurriculumSegment& Seg : Segments)
	{
		Counts.FindOrAdd(Seg.TagMask)++;
		TotalLength.FindOrAdd(Seg.TagMask) += FMath::Max(0.f, Seg.EndDistanceCm - Seg.StartDistanceCm);
	}

	CUR_LOGFMT(Log, "Curriculum Stats for {Asset}: Len={Len}cm Segs={Segs}",
		("Asset", GetNameSafe(this)), ("Len", SplineLengthCm), ("Segs", Segments.Num()));

	for (const auto& Pair : Counts)
	{
		const float Len = TotalLength.Contains(Pair.Key) ? TotalLength[Pair.Key] : 0.f;
		CUR_LOGFMT(Log, "  TagMask=0x{Mask}  Count={Count}  Length={Len}cm",
			("Mask", Pair.Key), ("Count", Pair.Value), ("Len", Len));
	}
}
