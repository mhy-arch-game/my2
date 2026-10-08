#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interaction contract. Any actor that should react to the player's
 * "look at it and press interact" can implement this interface.
 *
 * Alternative: leave the actor class untouched and drop a
 * UInteractableComponent onto it instead.
 */
class MHY_PROJ_V0_1_API IInteractableInterface
{
	GENERATED_BODY()

public:
	/** Returns true if this actor can be interacted with right now. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor);
	virtual bool CanInteract_Implementation(AActor* Interactor) { return true; }

	/** Runs the actual interaction. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor) {}

	/** Prompt shown by the HUD, e.g. "Press E to open". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt();
	virtual FText GetInteractionPrompt_Implementation() { return FText::GetEmpty(); }
};
