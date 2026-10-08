// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

/**
 *  Generic interaction contract for controllable / interactive actors.
 *
 *  Generalises the existing variant-local ISideScrollingInteractable by adding
 *  focus and prompt support, so a detector can highlight the focused structure
 *  and a HUD can show a prompt, without knowing the concrete class.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class IInteractableInterface
{
	GENERATED_BODY()

public:

	/** Whether the actor can currently be interacted with by Instigator. */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual bool CanInteract(AActor* Interactor) const = 0;

	/** Perform the interaction. */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual void OnInteract(AActor* Interactor) = 0;

	/** Prompt shown to the player while this actor is focused. */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual FText GetInteractionPrompt() const = 0;

	/** Called when this actor becomes the focused interaction candidate. */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual void OnFocusBegin(AActor* Interactor) {}

	/** Called when this actor stops being the focused interaction candidate. */
	UFUNCTION(BlueprintCallable, Category="Interactable")
	virtual void OnFocusEnd(AActor* Interactor) {}
};
