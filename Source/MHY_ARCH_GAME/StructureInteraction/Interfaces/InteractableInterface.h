// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

/**
 *  Generic interaction contract for controllable / interactive actors.
 *
 *  The detector (UInteractionDetectorComponent) only ever talks to this interface,
 *  so it never knows concrete types. Every object supplies its OWN logic:
 *  implement this interface on the class (C++ or Blueprint), or - if you do not
 *  want to touch the class - drop a UInteractableComponent on the actor instead
 *  (the detector accepts either).
 *
 *  Declared Blueprintable so designers can implement it per-object in Blueprint
 *  without writing C++; each method is a BlueprintNativeEvent, so C++ classes
 *  override the _Implementation form.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class IInteractableInterface
{
	GENERATED_BODY()

public:

	/** Whether the actor can currently be interacted with by Interactor. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interactable")
	bool CanInteract(AActor* Interactor);
	virtual bool CanInteract_Implementation(AActor* Interactor) { return true; }

	/** Perform the interaction. Per-object logic goes here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interactable")
	void OnInteract(AActor* Interactor);
	virtual void OnInteract_Implementation(AActor* Interactor) {}

	/** Prompt shown to the player while this actor is focused. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interactable")
	FText GetInteractionPrompt();
	virtual FText GetInteractionPrompt_Implementation() { return FText::GetEmpty(); }

	/** Called when this actor becomes the focused interaction candidate. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interactable")
	void OnFocusBegin(AActor* Interactor);
	virtual void OnFocusBegin_Implementation(AActor* Interactor) {}

	/** Called when this actor stops being the focused interaction candidate. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interactable")
	void OnFocusEnd(AActor* Interactor);
	virtual void OnFocusEnd_Implementation(AActor* Interactor) {}
};
