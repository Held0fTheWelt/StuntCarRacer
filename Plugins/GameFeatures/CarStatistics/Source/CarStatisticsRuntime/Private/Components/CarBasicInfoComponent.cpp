// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/CarBasicInfoComponent.h"

#include "Widgets/SpeedAndGearWidget.h"
#include "Interfaces/CarInterface.h"

#include "ChaosWheeledVehicleMovementComponent.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_CarBasicInfoComponent, Log, All);

// -----------------------------------------------------------------------------
// Structured logging helper
// -----------------------------------------------------------------------------
#define CBASIC_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_CarBasicInfoComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

// -----------------------------------------------------------------------------
// Safe destroy helper
// -----------------------------------------------------------------------------
static void DestroyNextTick_Safe(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}

	UWorld* World = Component->GetWorld();
	if (!World)
	{
		Component->DestroyComponent();
		return;
	}

	TWeakObjectPtr<UActorComponent> WeakComp(Component);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakComp]()
		{
			if (WeakComp.IsValid())
			{
				WeakComp->DestroyComponent();
			}
		}));
}

// -----------------------------------------------------------------------------
// Component
// -----------------------------------------------------------------------------
UCarBasicInfoComponent::UCarBasicInfoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCarBasicInfoComponent::BeginPlay()
{
	Super::BeginPlay();

	CBASIC_LOGFMT(Verbose, "BeginPlay");

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CBASIC_LOGFMT(Error, "Owner is null. Destroying component.");
		DestroyNextTick_Safe(this);
		return;
	}

	if (GetWorld()->GetFirstPlayerController() != Owner->GetInstigatorController())
	{
		CBASIC_LOGFMT(Verbose, "Not Player Controller.");
		DestroyNextTick_Safe(this);
		return;
	}


	// Prefer local pawn check
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			CBASIC_LOGFMT(Verbose, "Not locally controlled. Skipping widget creation.");
			DestroyNextTick_Safe(this);
			return;
		}
	}
	else
	{
		// Fallback
		AController* InstigatorController = Owner->GetInstigatorController();
		AController* FirstPC = GetWorld() ? Cast<AController>(GetWorld()->GetFirstPlayerController()) : nullptr;

		if (!InstigatorController || InstigatorController != FirstPC)
		{
			CBASIC_LOGFMT(Verbose, "Not the local player's actor. Skipping widget creation.");
			DestroyNextTick_Safe(this);
			return;
		}
	}

	// Widget class validation
	if (!SpeedAndGearWidgetClass)
	{
		CBASIC_LOGFMT(Warning, "SpeedAndGearWidgetClass is not set. Component will stay inactive.");
		return;
	}

	// Create widget
	SpeedAndGearWidgetInstance = CreateWidget<USpeedAndGearWidget>(
		GetWorld(),
		SpeedAndGearWidgetClass
	);

	if (!SpeedAndGearWidgetInstance)
	{
		CBASIC_LOGFMT(Error, "Failed to create SpeedAndGearWidgetInstance.");
		return;
	}

	SpeedAndGearWidgetInstance->AddToViewport();

	// Initialize UI state
	SpeedAndGearWidgetInstance->OnSpeedUpdated.Broadcast(0.0f);
	SpeedAndGearWidgetInstance->OnGearUpdated.Broadcast(0);

	CBASIC_LOGFMT(Verbose, "SpeedAndGearWidget created and added to viewport.");
}

void UCarBasicInfoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpeedAndGearWidgetInstance)
	{
		CBASIC_LOGFMT(Verbose, "Removing SpeedAndGearWidget from viewport.");
		SpeedAndGearWidgetInstance->RemoveFromParent();
		SpeedAndGearWidgetInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UCarBasicInfoComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SpeedAndGearWidgetInstance)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		// No spam — once per frame wäre zu laut
		return;
	}

	const UChaosWheeledVehicleMovementComponent* VehicleMovement =
		ICarInterface::Execute_GetCarChaosVehicleMovement(Owner);

	if (!VehicleMovement)
	{
		CBASIC_LOGFMT(Verbose, "VehicleMovement is null. Skipping UI update.");
		return;
	}

	const float CurrentSpeed = VehicleMovement->GetForwardSpeed();
	const int32 CurrentGear = VehicleMovement->GetCurrentGear();

	SpeedAndGearWidgetInstance->OnSpeedUpdated.Broadcast(CurrentSpeed);
	SpeedAndGearWidgetInstance->OnGearUpdated.Broadcast(CurrentGear);
}
