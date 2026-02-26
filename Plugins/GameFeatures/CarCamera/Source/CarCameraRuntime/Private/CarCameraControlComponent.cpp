// Fill out your copyright notice in the Description page of Project Settings.

#include "CarCameraControlComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "Logging/GlobalLog.h"
#include "Logging/StructuredLog.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"

#include "TimerManager.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(Log_CarCameraControlComponent, Log, All);

// Structured logging helper to avoid repeating boilerplate everywhere.
#define CCAM_LOGFMT(Verbosity, Format, ...) \
	UE_LOGFMT(Log_CarCameraControlComponent, Verbosity, "{Time} | {Class}:{Line} | " Format, \
		("Time", GET_CURRENT_TIME), \
		("Class", GET_CLASSNAME_WITH_FUNCTION), \
		("Line", GET_LINE_NUMBER) \
		__VA_OPT__(,) __VA_ARGS__)

UCarCameraControlComponent::UCarCameraControlComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCarCameraControlComponent::BeginPlay()
{
	Super::BeginPlay();

	CCAM_LOGFMT(Verbose, "BeginPlay");

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCAM_LOGFMT(Error, "Owner is null. Aborting.");
		return;
	}

	// ---------------------------------------------------------------------
	// Ownership / local player gating
	// NOTE: Your old Instigator check can be fragile. Keeping the spirit, but safer.
	// If this component is only meant for the locally controlled car, we early out.
	// ---------------------------------------------------------------------
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (PawnOwner)
	{
		if (!PawnOwner->IsLocallyControlled())
		{
			CCAM_LOGFMT(Verbose, "Not locally controlled. Skipping camera creation.");
			return;
		}
	}
	else
	{
		// Fallback if owner is not a pawn (rare for car pawns, but still)
		AController* InstigatorController = Owner->GetInstigatorController();
		AController* FirstPC = GetWorld() ? Cast<AController>(GetWorld()->GetFirstPlayerController()) : nullptr;

		if (!InstigatorController || InstigatorController != FirstPC)
		{
			CCAM_LOGFMT(Verbose, "Not the player's owner (fallback instigator check). Skipping camera creation.");
			return;
		}
	}

	USceneComponent* Root = Owner->GetRootComponent();
	if (!Root)
	{
		CCAM_LOGFMT(Error, "Owner RootComponent is null. Cannot create camera components.");
		return;
	}

	// Create SpringArm + Camera
	if (CreateSpringArm(Root) && SpringArm)
	{
		CreateCamera(SpringArm);
	}

	CCAM_LOGFMT(Verbose, "BeginPlay finished.");
}

void UCarCameraControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Only act for locally controlled / player owner
	APawn* PawnOwner = Cast<APawn>(Owner);
	if (PawnOwner && !PawnOwner->IsLocallyControlled())
	{
		return;
	}

	if (!SpringArm)
	{
		// Avoid log spam every frame; keep VeryVerbose if you really need it.
		// CCAM_LOGFMT(VeryVerbose, "Tick: SpringArm is null.");
		return;
	}

	// Realign camera yaw to face forward (0 yaw)
	if (!SpringArmAndCameraSettings.bRealignCameraYaw)
	{
		return;
	}
	const float CurrentYaw = SpringArm->GetRelativeRotation().Yaw;
	const float NewYaw = FMath::FInterpTo(CurrentYaw, 0.0f, DeltaTime, SpringArmAndCameraSettings.DefaultYawRealignInterpSpeed);

	SpringArm->SetRelativeRotation(FRotator(0.0f, NewYaw, 0.0f));
}

bool UCarCameraControlComponent::CreateSpringArm(USceneComponent* Root)
{
	if (!Root)
	{
		CCAM_LOGFMT(Error, "Root component is null. Cannot create SpringArm.");
		return false;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		CCAM_LOGFMT(Error, "Owner/World invalid. Cannot create SpringArm.");
		return false;
	}

	if (SpringArm)
	{
		CCAM_LOGFMT(Verbose, "SpringArm already exists. Skipping creation.");
		return true;
	}

	CCAM_LOGFMT(Verbose, "Creating SpringArm...");

	// Use Owner as Outer for better lifecycle/GC behavior.
	SpringArm = NewObject<USpringArmComponent>(Owner);
	if (!SpringArm)
	{
		CCAM_LOGFMT(Error, "Failed to allocate SpringArm.");
		return false;
	}

	Owner->AddInstanceComponent(SpringArm);
	SpringArm->SetupAttachment(Root);
	SpringArm->RegisterComponent();

	// Configuration / defaults
	SpringArm->bDrawDebugLagMarkers = SpringArmAndCameraSettings.bDrawDebugLagMarkers;

	SpringArm->SetRelativeLocation(SpringArmAndCameraSettings.SpringArmRelativeLocation);
	SpringArm->TargetArmLength = SpringArmAndCameraSettings.TargetArmLength;
	SpringArm->SocketOffset = SpringArmAndCameraSettings.SocketOffset;
	SpringArm->TargetOffset = SpringArmAndCameraSettings.TargetOffset;

	SpringArm->bEnableCameraRotationLag = SpringArmAndCameraSettings.bEnableCameraRotationLag;
	SpringArm->CameraRotationLagSpeed = SpringArmAndCameraSettings.CameraRotationLagSpeed;

	SpringArm->bInheritPitch = SpringArmAndCameraSettings.bInheritPitch;
	SpringArm->bInheritYaw = SpringArmAndCameraSettings.bInheritYaw;
	SpringArm->bInheritRoll = SpringArmAndCameraSettings.bInheritRoll;

	SpringArm->bDoCollisionTest = SpringArmAndCameraSettings.bDoCollisionTestSpringArm;
	SpringArm->bUsePawnControlRotation = SpringArmAndCameraSettings.bUsePawnControlRotationSpringarm;

	CCAM_LOGFMT(Verbose, "SpringArm created and registered.");
	return true;
}

bool UCarCameraControlComponent::CreateCamera(USceneComponent* InSpringArm)
{
	if (!InSpringArm)
	{
		CCAM_LOGFMT(Error, "SpringArm is null. Cannot create Camera.");
		return false;
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		CCAM_LOGFMT(Error, "Owner/World invalid. Cannot create Camera.");
		return false;
	}

	if (Camera)
	{
		CCAM_LOGFMT(Verbose, "Camera already exists. Skipping creation.");
		return true;
	}

	CCAM_LOGFMT(Verbose, "Creating Camera...");

	Camera = NewObject<UCameraComponent>(Owner);
	if (!Camera)
	{
		CCAM_LOGFMT(Error, "Failed to allocate Camera.");
		return false;
	}

	Owner->AddInstanceComponent(Camera);
	Camera->SetupAttachment(InSpringArm);
	Camera->RegisterComponent();

	//Camera->SetAutoActivate(true);
	Camera->bUsePawnControlRotation = SpringArmAndCameraSettings.bUsePawnControlRotationCamera;
	Camera->FieldOfView = SpringArmAndCameraSettings.FieldOfView;
	Camera->SetVisibility(true);
	Camera->SetHiddenInGame(false);

	CCAM_LOGFMT(Verbose, "Camera created and registered.");
	return true;
}

void UCarCameraControlComponent::SetupControlComponent_Implementation(UInputComponent* PlayerInputComponent)
{
	CCAM_LOGFMT(Verbose, "SetupControlComponent_Implementation called.");

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		CCAM_LOGFMT(Error, "Owner is null. Cannot bind input.");
		return;
	}

	APawn* PawnOwner = Cast<APawn>(Owner);
	if (PawnOwner && !PawnOwner->IsLocallyControlled())
	{
		CCAM_LOGFMT(Verbose, "Not locally controlled. Skipping input binding.");
		return;
	}

	if (!PlayerInputComponent)
	{
		CCAM_LOGFMT(Error, "PlayerInputComponent is null. Cannot bind actions.");
		return;
	}

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInputComponent)
	{
		CCAM_LOGFMT(Error, "EnhancedInputComponent not found. This component expects Enhanced Input.");
		return;
	}

	// Bind Actions (assuming these are valid UInputAction* members)
	if (LookAroundAction)
	{
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &UCarCameraControlComponent::LookAround);
	}
	else
	{
		CCAM_LOGFMT(Warning, "LookAroundAction is null. Look binding skipped.");
	}

	if (ToggleCameraAction)
	{
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &UCarCameraControlComponent::ToggleCamera);
	}
	else
	{
		CCAM_LOGFMT(Warning, "ToggleCameraAction is null. Toggle binding skipped.");
	}

	CCAM_LOGFMT(Verbose, "Enhanced Input actions bound for component '{Name}'.", ("Name", GetNameSafe(this)));
}

void UCarCameraControlComponent::LookAround(const FInputActionValue& InputValue)
{
	if (!SpringArm)
	{
		CCAM_LOGFMT(Verbose, "LookAround called but SpringArm is null. Ignoring input.");
		return;
	}

	// For a "Look" axis you often get Vector2D (mouse/gamepad), but your original uses float.
	// Keeping float as you had it.
	const float YawDelta = InputValue.Get<float>();
	SpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void UCarCameraControlComponent::ToggleCamera(const FInputActionValue& Value)
{
	// TODO: Implement camera toggling (front/back), or different camera modes.
	// Keep logs very low to avoid spam on button mash.
	CCAM_LOGFMT(Verbose, "ToggleCamera triggered.");
}
