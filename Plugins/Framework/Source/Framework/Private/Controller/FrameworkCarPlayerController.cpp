// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller/FrameworkCarPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Pawn.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AFrameworkCarPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ensure we're attached to the vehicle pawn so that World Partition streaming works correctly
	bAttachToPawn = true;

	// only spawn UI on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				// add the controls to the player screen
				MobileControlsWidget->AddToPlayerScreen(0);

			}
			else {

				UE_LOG(LogTemp, Error, TEXT("Could not spawn mobile controls widget."));

			}
		}
	}
}

void AFrameworkCarPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void AFrameworkCarPlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	//if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	//{
	//	VehicleUI->UpdateSpeed(VehiclePawn->GetChaosVehicleMovement()->GetForwardSpeed());
	//	VehicleUI->UpdateGear(VehiclePawn->GetChaosVehicleMovement()->GetCurrentGear());
	//}
}

void AFrameworkCarPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	//// get a pointer to the controlled pawn
	//VehiclePawn = CastChecked<AStuntCarRacerPawn>(InPawn);

	//// subscribe to the pawn's OnDestroyed delegate
	//VehiclePawn->OnDestroyed.AddDynamic(this, &AStuntCarRacerPlayerController::OnPawnDestroyed);
}

void AFrameworkCarPlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	//// find the player start
	//TArray<AActor*> ActorList;
	//UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	//if (ActorList.Num() > 0)
	//{
	//	// spawn a vehicle at the player start
	//	const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

	//	if (AStuntCarRacerPawn* RespawnedVehicle = GetWorld()->SpawnActor<AStuntCarRacerPawn>(VehiclePawnClass, SpawnTransform))
	//	{
	//		// possess the vehicle
	//		Possess(RespawnedVehicle);
	//	}
	//}
}

bool AFrameworkCarPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
