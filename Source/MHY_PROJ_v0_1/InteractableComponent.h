#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteracted, AActor*, Interactor, AActor*, Interactable);

/**
 * Interactable side. Drop this onto ANY existing actor to make it
 * interactable - no reparenting, no C++ required.
 * Bind OnInteracted in the blueprint to give it behaviour.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class MHY_PROJ_V0_1_API UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	/** Prompt shown by the HUD, e.g. "Press E to open". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionPrompt;

	/** When false the owner cannot be interacted with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bEnabled = true;

	/** Fired when this component is successfully interacted with. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteracted OnInteracted;

	/** Called by UInteractionComponent. */
	virtual bool CanInteract(AActor* Interactor) const;
	virtual void Interact(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractionPrompt() const { return InteractionPrompt; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }
};
