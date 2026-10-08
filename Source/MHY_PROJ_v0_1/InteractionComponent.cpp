#include "InteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InteractableComponent.h"
#include "InteractableInterface.h"
#include "TimerManager.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(TraceInterval, 0.02f);
		World->GetTimerManager().SetTimer(TraceTimerHandle, this, &UInteractionComponent::UpdateFocus, Interval, true);
	}
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TraceTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

bool UInteractionComponent::TraceFromViewpoint(FHitResult& OutHit) const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return false;
	}

	FVector ViewLocation = Owner->GetActorLocation();
	FRotator ViewRotation = Owner->GetActorRotation();

	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;
	FCollisionQueryParams Params(FName(TEXT("InteractionTrace")), bTraceComplex, Owner);

	return World->LineTraceSingleByChannel(OutHit, ViewLocation, TraceEnd, TraceChannel, Params);
}

void UInteractionComponent::UpdateFocus()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	FHitResult Hit;
	AActor* NewFocus = nullptr;

	if (TraceFromViewpoint(Hit))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && HitActor != Owner)
		{
			if (HitActor->Implements<UInteractableInterface>())
			{
				if (IInteractableInterface::Execute_CanInteract(HitActor, Owner))
				{
					NewFocus = HitActor;
				}
			}
			else if (UInteractableComponent* Interactable = HitActor->FindComponentByClass<UInteractableComponent>())
			{
				if (Interactable->CanInteract(Owner))
				{
					NewFocus = HitActor;
				}
			}
			else if (!bRequireInteractable)
			{
				NewFocus = HitActor;
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebug)
	{
		const FVector DebugEnd = Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
		DrawDebugLine(GetWorld(), Hit.TraceStart, DebugEnd, NewFocus ? FColor::Green : FColor::Red, false, TraceInterval + 0.02f, 0, 1.5f);
	}
#endif

	AActor* PreviousFocus = FocusedActor.Get();
	if (NewFocus != PreviousFocus)
	{
		FocusedActor = NewFocus;
		OnFocusChanged.Broadcast(NewFocus, PreviousFocus);
	}
}

AActor* UInteractionComponent::RefreshFocus()
{
	UpdateFocus();
	return FocusedActor.Get();
}

FText UInteractionComponent::GetFocusedPrompt()
{
	AActor* Target = FocusedActor.Get();
	if (!Target)
	{
		return FText::GetEmpty();
	}

	if (Target->Implements<UInteractableInterface>())
	{
		return IInteractableInterface::Execute_GetInteractionPrompt(Target);
	}

	if (const UInteractableComponent* Interactable = Target->FindComponentByClass<UInteractableComponent>())
	{
		return Interactable->GetInteractionPrompt();
	}

	return FText::GetEmpty();
}

bool UInteractionComponent::TryInteract()
{
	UpdateFocus();

	AActor* Owner = GetOwner();
	AActor* Target = FocusedActor.Get();
	if (!Owner || !Target)
	{
		return false;
	}

	if (Target->Implements<UInteractableInterface>())
	{
		if (!IInteractableInterface::Execute_CanInteract(Target, Owner))
		{
			return false;
		}

		IInteractableInterface::Execute_Interact(Target, Owner);
		return true;
	}

	if (UInteractableComponent* Interactable = Target->FindComponentByClass<UInteractableComponent>())
	{
		if (!Interactable->CanInteract(Owner))
		{
			return false;
		}

		Interactable->Interact(Owner);
		return true;
	}

	return false;
}
