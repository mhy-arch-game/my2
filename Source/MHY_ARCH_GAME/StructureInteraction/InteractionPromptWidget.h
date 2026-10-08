// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

/**
 *  UInteractionPromptWidget - HUD prompt for the focused interactable.
 *
 *  Presentation hook (see Docs/StructureInteraction.md): this class defines the
 *  C++ surface only. Create a Widget Blueprint from it and implement SetPrompt
 *  to lay out the text / show-hide animation.
 *
 *  Bind it by listening to UInteractionDetectorComponent::OnFocusChanged.
 */
UCLASS(abstract)
class UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Update the prompt text and visibility. */
	UFUNCTION(BlueprintImplementableEvent, Category="Interaction")
	void SetPrompt(const FText& PromptText, bool bVisible);
};
