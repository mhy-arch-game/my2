#include "InteractableActor.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(50.f, 50.f, 50.f));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBox->SetGenerateOverlapEvents(false);
	RootComponent = CollisionBox;

	Prompt = NSLOCTEXT("Interaction", "DefaultInteractActorPrompt", "Interact");
}

bool AInteractableActor::CanInteract_Implementation(AActor*)
{
	return true;
}

void AInteractableActor::Interact_Implementation(AActor* Interactor)
{
	UE_LOG(LogTemp, Log, TEXT("[Interaction] %s interacted with %s"), *GetNameSafe(Interactor), *GetName());

#if !(UE_BUILD_SHIPPING)
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, FString::Printf(TEXT("Interacted: %s"), *GetName()));
	}
#endif

	OnInteracted(Interactor);
}

FText AInteractableActor::GetInteractionPrompt_Implementation()
{
	return Prompt;
}
