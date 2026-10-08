// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractableComponent.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UInteractableComponent::CanInteract(AActor*) const
{
	return bEnabled;
}

void UInteractableComponent::NotifyInteract(AActor* Interactor)
{
	if (!bEnabled)
	{
		return;
	}

	OnInteractRequested.Broadcast(Interactor, GetOwner());
}

void UInteractableComponent::NotifyFocusGained(AActor* Interactor)
{
	OnFocusGained.Broadcast(Interactor, GetOwner());
}

void UInteractableComponent::NotifyFocusLost(AActor* Interactor)
{
	OnFocusLost.Broadcast(Interactor, GetOwner());
}
