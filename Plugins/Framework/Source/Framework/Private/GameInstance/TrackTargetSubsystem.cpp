// Fill out your copyright notice in the Description page of Project Settings.

#include "GameInstance/TrackTargetSubsystem.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TrackTargetSubsystem, Log, All);

#define TTS_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TrackTargetSubsystem, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

void UTrackTargetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TTS_LOGFMT(Display, "Initialized.");
}

void UTrackTargetSubsystem::Deinitialize()
{
	TTS_LOGFMT(Display, "Deinitialized.");

	Super::Deinitialize();
}

void UTrackTargetSubsystem::NotifyTargetTracking(AActor* TrackedActor, int32 TargetIndex)
{
	if (!TrackedActor)
	{
		TTS_LOGFMT(Warning, "NotifyTargetTracking called with null actor. TargetIndex={Index}",
			("Index", TargetIndex));
		return;
	}

	if (bLogEvents)
	{
		TTS_LOGFMT(Log, "Target tracked. Actor={Actor}, TargetIndex={Index}, Bound={Bound}",
			("Actor", GetNameSafe(TrackedActor)),
			("Index", TargetIndex),
			("Bound", OnTargetTrackedSignature.IsBound()));
	}

	// Broadcast is safe even with 0 listeners.
	OnTargetTrackedSignature.Broadcast(TrackedActor, TargetIndex);
}
