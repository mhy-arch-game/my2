// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractableEvent, AActor*, Interactor, AActor*, Interactable);

/**
 *  UInteractableComponent - interaction capability you can attach to ANY actor.
 *
 *  Drop this on an object to make it interactable without changing its class and
 *  without writing C++: the player's UInteractionDetectorComponent focuses the
 *  owner when near AND aimed at, shows InteractionPrompt, and fires
 *  OnInteractRequested when the interact key is pressed. Put the object-specific
 *  logic in that event - different objects, different logic.
 *
 *  Counterpart to IInteractableInterface:
 *    - interface       -> implement on the class (C++ or Blueprint)
 *    - this component  -> attach to a specific object (logic in Blueprint)
 *  The detector accepts either.
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	/** Whether the owner can be interacted with right now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bEnabled = true;

	/** Text shown by the HUD while the owner is focused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText InteractionPrompt;

	/** Fired when the interact key is pressed while the owner is focused. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnInteractRequested;

	/** Fired when the owner becomes the focused interaction candidate. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnFocusGained;

	/** Fired when the owner stops being the focused candidate. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnFocusLost;

	// -- queried by UInteractionDetectorComponent --------------------------
	virtual bool CanInteract(AActor* Interactor) const;
	virtual FText GetInteractionPrompt() const { return InteractionPrompt; }
	virtual void NotifyInteract(AActor* Interactor);
	virtual void NotifyFocusGained(AActor* Interactor);
	virtual void NotifyFocusLost(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsEnabled() const { return bEnabled; }
};
