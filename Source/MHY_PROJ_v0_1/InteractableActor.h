#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "InteractableActor.generated.h"

class UBoxComponent;

/**
 * Minimal interactable actor used to verify the interaction chain.
 * It ships a box collision only, so it needs no art asset at all
 * and is hit by the visibility trace out of the box.
 */
UCLASS(Blueprintable)
class MHY_PROJ_V0_1_API AInteractableActor : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AInteractableActor();

	//~ Begin IInteractableInterface
	virtual bool CanInteract_Implementation(AActor* Interactor) override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() override;
	//~ End IInteractableInterface

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Prompt;

protected:
	/** Blueprint hook for the actual reaction. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnInteracted(AActor* Interactor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UBoxComponent> CollisionBox;
};
