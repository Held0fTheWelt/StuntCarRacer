#pragma once

#include "CoreMinimal.h"
#include "GameFeaturedAIController.h"
#include "FrameworkAIController.generated.h"

/**
 * AI Controller that resets training episodes when possessing a pawn
 * that has a component implementing UTrainNNInterface.
 */
UCLASS()
class FRAMEWORK_API AFrameworkAIController : public AGameFeaturedAIController
{
	GENERATED_BODY()

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	/** Finds the first component on the pawn implementing UTrainNNInterface */
	UActorComponent* FindTrainNNComponent(APawn* InPawn) const;
};
