#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FlipActorComponent.generated.h"

class UInputAction;

/**
 * Component that detects flipped vehicles and allows manual or automatic reset.
 *
 * - Periodically checks orientation (UpVector vs WorldUp)
 * - Automatically resets vehicle if flipped for multiple checks
 * - Allows manual reset via Enhanced Input
 * - Only active for locally controlled actors
 */
UCLASS(ClassGroup = (Car), meta = (BlueprintSpawnableComponent))
class FLIPSYSTEMRUNTIME_API UFlipActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFlipActorComponent();

protected:
	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//~ Input setup
	UFUNCTION(BlueprintNativeEvent)
	void SetupControlComponent(UInputComponent* PlayerInputComponent);

	/* ------------------------------------------------------------------ */
	/* Flip / Reset Logic                                                  */
	/* ------------------------------------------------------------------ */

	/** Manual reset (input-triggered) */
	void ResetVehicle(const struct FInputActionValue& Value);

	/** Periodic flipped-state check */
	void FlippedCheck();

protected:
	/* ------------------------------------------------------------------ */
	/* Configuration                                                       */
	/* ------------------------------------------------------------------ */

	/** Input action used to reset the vehicle */
	UPROPERTY(EditDefaultsOnly, Category = "Flip|Input")
	TObjectPtr<UInputAction> ResetVehicleAction;

	/** Interval (seconds) between flipped checks */
	UPROPERTY(EditAnywhere, Category = "Flip|Detection", meta = (ClampMin = "0.05"))
	float FlipCheckTime = 0.5f;

	/**
	 * Dot(WorldUp, ActorUp) threshold.
	 *  1.0 = perfectly upright
	 *  0.0 = sideways
	 * < 0.0 = upside-down
	 */
	UPROPERTY(EditAnywhere, Category = "Flip|Detection", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float FlipCheckMinDot = 0.2f;

	/** Vertical offset applied when resetting (cm) */
	UPROPERTY(EditAnywhere, Category = "Flip|Reset", meta = (ClampMin = "0.0"))
	float ResetHeightOffset = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flip|Debug")
	bool bDebug = false;
protected:
	/* ------------------------------------------------------------------ */
	/* Runtime State                                                       */
	/* ------------------------------------------------------------------ */

	/** Timer handle for flipped checks */
	FTimerHandle FlipCheckTimer;

	/** True if the previous check already detected a flip */
	bool bPreviousFlipCheck = false;
};
