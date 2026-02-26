// Fill out your copyright notice in the Description page of Project Settings.

#include "FlipActorComponent.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

#include "Interfaces/ResetInterface.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

DEFINE_LOG_CATEGORY_STATIC(Log_FlipActorComponent, Log, All);

#define FLIP_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_FlipActorComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

UFlipActorComponent::UFlipActorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFlipActorComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		FLIP_LOGFMT(Error, "Owner is null. Component disabled.");
		return;
	}

	// Only operate on locally controlled pawns
	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			FLIP_LOGFMT(Verbose, "Not locally controlled. Flip detection disabled.");
			return;
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FlipCheckTimer,
			this,
			&UFlipActorComponent::FlippedCheck,
			FlipCheckTime,
			true
		);
	}

	FLIP_LOGFMT(Verbose, "FlipActorComponent started (Interval={Interval}s, MinDot={MinDot})",
		("Interval", FlipCheckTime),
		("MinDot", FlipCheckMinDot));
}

void UFlipActorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlipCheckTimer);
	}

	FLIP_LOGFMT(Verbose, "FlipActorComponent ended.");

	Super::EndPlay(EndPlayReason);
}

void UFlipActorComponent::SetupControlComponent_Implementation(UInputComponent* PlayerInputComponent)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		FLIP_LOGFMT(Error, "SetupControlComponent: Owner is null.");
		return;
	}

	if (APawn* PawnOwner = Cast<APawn>(Owner))
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			FLIP_LOGFMT(Verbose, "Not locally controlled. Input binding skipped.");
			return;
		}
	}

	if (!PlayerInputComponent)
	{
		FLIP_LOGFMT(Error, "PlayerInputComponent is null.");
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		FLIP_LOGFMT(Error, "EnhancedInputComponent not found.");
		return;
	}

	if (!ResetVehicleAction)
	{
		FLIP_LOGFMT(Warning, "ResetVehicleAction is null. Manual reset disabled.");
		return;
	}

	EnhancedInput->BindAction(
		ResetVehicleAction,
		ETriggerEvent::Triggered,
		this,
		&UFlipActorComponent::ResetVehicle
	);

	if(bDebug)
		FLIP_LOGFMT(Display, "ResetVehicle input bound.");
}

void UFlipActorComponent::ResetVehicle(const FInputActionValue& /*Value*/)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		FLIP_LOGFMT(Error, "ResetVehicle called but Owner is null.");
		return;
	}

	const FVector ResetLocation =
		Owner->GetActorLocation() + FVector(0.0f, 0.0f, ResetHeightOffset);

	FRotator ResetRotation = Owner->GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	Owner->SetActorTransform(
		FTransform(ResetRotation, ResetLocation),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (Owner->GetClass()->ImplementsInterface(UResetInterface::StaticClass()))
	{
		IResetInterface::Execute_Reset(Owner);
	}

	if (bDebug)
		FLIP_LOGFMT(Log, "Vehicle reset executed.");
}

void UFlipActorComponent::FlippedCheck()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const float UpDot = FVector::DotProduct(
		FVector::UpVector,
		Owner->GetActorUpVector()
	);

	if (UpDot < FlipCheckMinDot)
	{
		if (bPreviousFlipCheck)
		{
			if (bDebug)
				FLIP_LOGFMT(Log, "Vehicle confirmed flipped (UpDot={Dot}). Resetting.",
					("Dot", UpDot));

			const FInputActionValue DummyValue;
			ResetVehicle(DummyValue);
		}

		bPreviousFlipCheck = true;
	}
	else
	{
		bPreviousFlipCheck = false;
	}
}
