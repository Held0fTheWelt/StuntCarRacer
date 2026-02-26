#include "Actors/TrackGateActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"

#include "Interfaces/GameActorInterface.h"
#include "GameInstance/TrackTargetSubsystem.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_TrackGateActor, Log, All);

#define TG_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_TrackGateActor, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

ATrackGateActor::ATrackGateActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);

	CollisionBox->SetBoxExtent(FVector(1000.0f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionBox->SetGenerateOverlapEvents(true);

#if WITH_EDITOR
	CollisionBox->SetHiddenInGame(false);
	CollisionBox->bVisualizeComponent = true;
#endif

	Tags.Add(FName("TrackGate"));
}

void ATrackGateActor::BeginPlay()
{
	Super::BeginPlay();

	// Recover, falls BP/Serialization den inherited Component-Pointer genuked hat
	if (!CollisionBox)
	{
		CollisionBox = FindComponentByClass<UBoxComponent>();
	}

	if (!CollisionBox)
	{
		TG_LOGFMT(Error, "CollisionBox is null (likely BP removed/overrode inherited component). Actor={Actor}",
			("Actor", GetNameSafe(this)));
		return;
	}

	// Bind overlap (zuverlässiger als NotifyActorBeginOverlap, und du nutzt ja explizit die Box)
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &ATrackGateActor::OnGateBeginOverlap);

	if(bDebug)
	{
		TG_LOGFMT(Display, "TrackGateActor started. GateIndex={Index} Box={Box}",
			("Index", GateIndex),
			("Box", GetNameSafe(CollisionBox)));
	}
}

void ATrackGateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TG_LOGFMT(Verbose, "TrackGateActor ended. GateIndex={Index}", ("Index", GateIndex));
	Super::EndPlay(EndPlayReason);
}

void ATrackGateActor::OnGateBeginOverlap(
	UPrimitiveComponent* /*OverlappedComp*/,
	AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/,
	int32 /*OtherBodyIndex*/,
	bool /*bFromSweep*/,
	const FHitResult& /*SweepResult*/
)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	if (!OtherActor->GetClass()->ImplementsInterface(UGameActorInterface::StaticClass()))
	{
		TG_LOGFMT(Verbose, "Overlap ignored. Actor={Actor} does not implement GameActorInterface.",
			("Actor", GetNameSafe(OtherActor)));
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		TG_LOGFMT(Warning, "GameInstance is null. Cannot notify TrackTargetSubsystem.");
		return;
	}

	UTrackTargetSubsystem* TrackTargetSubsystem = GameInstance->GetSubsystem<UTrackTargetSubsystem>();
	if (!TrackTargetSubsystem)
	{
		TG_LOGFMT(Warning, "TrackTargetSubsystem is null.");
		return;
	}

	TrackTargetSubsystem->NotifyTargetTracking(OtherActor, GateIndex);
	
	if (bDebug)
		TG_LOGFMT(Log, "Gate triggered. Actor={Actor}, GateIndex={Index}",
			("Actor", GetNameSafe(OtherActor)),
			("Index", GateIndex));
}
