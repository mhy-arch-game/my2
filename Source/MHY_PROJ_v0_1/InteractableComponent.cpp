#include "InteractableComponent.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InteractionPrompt = NSLOCTEXT("Interaction", "DefaultInteractPrompt", "Interact");
}

bool UInteractableComponent::CanInteract(AActor*) const
{
	return bEnabled;
}

void UInteractableComponent::Interact(AActor* Interactor)
{
	if (!bEnabled)
	{
		return;
	}

	OnInteracted.Broadcast(Interactor, GetOwner());
}
