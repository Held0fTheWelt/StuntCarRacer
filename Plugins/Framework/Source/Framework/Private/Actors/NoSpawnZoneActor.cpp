#include "Actors/NoSpawnZoneActor.h"
#include "GameInstance/RespawnGameInstanceSubsystem.h"
#include "Components/BoxComponent.h"

ANoSpawnZoneActor::ANoSpawnZoneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	SetRootComponent(Box);

	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetHiddenInGame(false);
#if WITH_EDITOR
	Box->bVisualizeComponent = true;
#endif
}

void ANoSpawnZoneActor::BeginPlay()
{
	Super::BeginPlay();

	UGameInstance* GI = GetGameInstance();

	if (GI)
	{
		URespawnGameInstanceSubsystem* RespawnGISubsystem = GI->GetSubsystem<URespawnGameInstanceSubsystem>();
		if (RespawnGISubsystem)
		{
			RespawnGISubsystem->RegisterNoSpawnZone_Implementation(this);
		}
	}
}

void ANoSpawnZoneActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		URespawnGameInstanceSubsystem* RespawnGISubsystem = GI->GetSubsystem<URespawnGameInstanceSubsystem>();
		if (RespawnGISubsystem)
		{
			RespawnGISubsystem->UnRegisterNoSpawnZone_Implementation(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool ANoSpawnZoneActor::ContainsPoint(const FVector& WorldPoint) const
{
	return ContainsPointWithMargin(WorldPoint, SafetyExtraCm);
}

bool ANoSpawnZoneActor::ContainsPointWithMargin(const FVector& WorldPoint, float ExtraCm) const
{
	if (!Box)
	{
		return false;
	}

	const FTransform BoxTM = Box->GetComponentTransform();
	const FVector Local = BoxTM.InverseTransformPosition(WorldPoint);

	const FVector Ext = Box->GetScaledBoxExtent() + FVector(ExtraCm);

	return (FMath::Abs(Local.X) <= Ext.X) &&
		(FMath::Abs(Local.Y) <= Ext.Y) &&
		(FMath::Abs(Local.Z) <= Ext.Z);
}
