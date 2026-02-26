// Fill out your copyright notice in the Description page of Project Settings.

#include "CarControlComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Interfaces/CarInterface.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_CarControlComponent, Log, All);

// Structured logging helper.
#define CCTRL_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_CarControlComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

/**
 * Safely destroys this component on the next tick using a weak pointer,
 * preventing use-after-free if the component gets destroyed earlier.
 */
static void DestroyNextTick_Safe(UCarControlComponent* Component)
{
	if (!Component)
	{
		return;
	}

	UWorld* World = Component->GetWorld();
	if (!World)
	{
		// If there's no world, we can't schedule a timer. Destroy immediately.
		Component->DestroyComponent();
		return;
	}

	TWeakObjectPtr<UCarControlComponent> WeakThis(Component);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->DestroyComponent();
			}
		}));
}

UCarControlComponent::UCarControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCarControlComponent::BeginPlay()
{
	Super::BeginPlay();

	CCTRL_LOGFMT(Verbose, "BeginPlay");

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "Owner is null. Aborting.");
		DestroyNextTick_Safe(this);
		return;
	}

	// Prefer gating by locally controlled pawn.
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			CCTRL_LOGFMT(Verbose, "Not locally controlled. Destroying component next tick.");
			DestroyNextTick_Safe(this);
			return;
		}
	}
	else
	{
		// Fallback gate if owner is not a pawn.
		UWorld* World = GetWorld();
		AController* FirstPC = World ? Cast<AController>(World->GetFirstPlayerController()) : nullptr;
		AController* InstigatorController = Owner->GetInstigatorController();

		if (!InstigatorController || InstigatorController != FirstPC)
		{
			CCTRL_LOGFMT(Verbose, "Not the player's owner (fallback instigator check). Destroying component next tick.");
			DestroyNextTick_Safe(this);
			return;
		}
	}

	CCTRL_LOGFMT(Verbose, "Owner is locally controlled. Control component active.");
}

void UCarControlComponent::SetupControlComponent_Implementation(UInputComponent* PlayerInputComponent)
{
	CCTRL_LOGFMT(Display, "SetupControlComponent_Implementation called.");

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "Owner is null. Cannot bind input. Destroying next tick.");
		DestroyNextTick_Safe(this);
		return;
	}

	// Ensure only local pawn binds input.
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			CCTRL_LOGFMT(Verbose, "Not locally controlled. Skipping input binding (component will be destroyed).");
			DestroyNextTick_Safe(this);
			return;
		}
	}

	if (!PlayerInputComponent)
	{
		CCTRL_LOGFMT(Error, "PlayerInputComponent is null. Cannot bind actions.");
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		CCTRL_LOGFMT(Error, "EnhancedInputComponent not found. This component expects Enhanced Input.");
		return;
	}

	// Bind only if actions are valid.
	if (SteeringAction)
	{
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &UCarControlComponent::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &UCarControlComponent::Steering);
	}
	else
	{
		CCTRL_LOGFMT(Warning, "SteeringAction is null. Steering binding skipped.");
	}

	if (ThrottleAction)
	{
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &UCarControlComponent::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &UCarControlComponent::Throttle);
	}
	else
	{
		CCTRL_LOGFMT(Warning, "ThrottleAction is null. Throttle binding skipped.");
	}

	if (BrakeAction)
	{
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &UCarControlComponent::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &UCarControlComponent::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &UCarControlComponent::StopBrake);
	}
	else
	{
		CCTRL_LOGFMT(Warning, "BrakeAction is null. Brake binding skipped.");
	}

	if (HandbrakeAction)
	{
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &UCarControlComponent::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &UCarControlComponent::StopHandbrake);
	}
	else
	{
		CCTRL_LOGFMT(Warning, "HandbrakeAction is null. Handbrake binding skipped.");
	}

	CCTRL_LOGFMT(Display, "Input bindings completed for '{Name}'.", ("Name", GetNameSafe(this)));
}

void UCarControlComponent::Steering(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "Steering: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "Steering: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_Steering(Owner, Value.Get<float>());
}

void UCarControlComponent::Throttle(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "Throttle: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "Throttle: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_Throttle(Owner, Value.Get<float>());
}

void UCarControlComponent::Brake(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "Brake: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "Brake: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_Brake(Owner, Value.Get<float>());
}

void UCarControlComponent::StartBrake(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "StartBrake: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "StartBrake: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_StartBrake(Owner);
}

void UCarControlComponent::StopBrake(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "StopBrake: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "StopBrake: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_StopBrake(Owner);
}

void UCarControlComponent::StartHandbrake(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "StartHandbrake: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "StartHandbrake: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_StartHandbrake(Owner);
}

void UCarControlComponent::StopHandbrake(const FInputActionValue& Value)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCTRL_LOGFMT(Error, "StopHandbrake: Owner is null.");
		return;
	}

	if (!Owner->GetClass()->ImplementsInterface(UCarInterface::StaticClass()))
	{
		CCTRL_LOGFMT(Warning, "StopHandbrake: Owner does not implement CarInterface. Call ignored.");
		return;
	}

	ICarInterface::Execute_StopHandbrake(Owner);
}
