// Fill out your copyright notice in the Description page of Project Settings.

#include "Actors/OutOfBoundsActor.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#include "GameInstance/RespawnGameInstanceSubsystem.h"
#include "Interfaces/GameActorInterface.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_OutOfBoundsActor, Log, All);

#define OOB_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_OutOfBoundsActor, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

AOutOfBoundsActor::AOutOfBoundsActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	// Sensible defaults for an "out of bounds" volume
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldStatic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Overlap);

	// Optional: if you have a project collision profile for volumes
	// CollisionComponent->SetCollisionProfileName(TEXT("Trigger"));
}

void AOutOfBoundsActor::BeginPlay()
{
	Super::BeginPlay();

	if (!CollisionComponent)
	{
		OOB_LOGFMT(Error, "CollisionComponent is null.");
		return;
	}

	// Defensive: avoid double binding (PIE / reinstancing)
	CollisionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &AOutOfBoundsActor::OnOverlapBegin);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AOutOfBoundsActor::OnOverlapBegin);

	OOB_LOGFMT(Verbose, "OutOfBoundsActor started. Collision={Collision}",
		("Collision", GetNameSafe(CollisionComponent)));
}

void AOutOfBoundsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.RemoveDynamic(this, &AOutOfBoundsActor::OnOverlapBegin);
	}

	OOB_LOGFMT(Verbose, "OutOfBoundsActor ended.");

	Super::EndPlay(EndPlayReason);
}

void AOutOfBoundsActor::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	(void)OverlappedComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	// Fast guards (no log spam)
	if (!OtherActor || OtherActor == this || !OtherComp)
	{
		return;
	}

	// Optional: only react to pawns / vehicles
	// if (!OtherActor->IsA<APawn>()) return;

	if (!OtherActor->GetClass()->ImplementsInterface(UGameActorInterface::StaticClass()))
	{
		OOB_LOGFMT(Verbose, "Overlap ignored: Actor={Actor} does not implement GameActorInterface.",
			("Actor", GetNameSafe(OtherActor)));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		OOB_LOGFMT(Warning, "GameInstance is null. Cannot respawn Actor={Actor}.",
			("Actor", GetNameSafe(OtherActor)));
		return;
	}

	URespawnGameInstanceSubsystem* RespawnSubsystem = GameInstance->GetSubsystem<URespawnGameInstanceSubsystem>();
	if (!RespawnSubsystem)
	{
		OOB_LOGFMT(Warning, "RespawnGameInstanceSubsystem is null. Cannot respawn Actor={Actor}.",
			("Actor", GetNameSafe(OtherActor)));
		return;
	}

	RespawnSubsystem->NotifyRespawn(OtherActor);

	if(bDebug)
	{
		OOB_LOGFMT(Log, "Respawn notified: Actor={Actor}", ("Actor", GetNameSafe(OtherActor)));
	}
}
