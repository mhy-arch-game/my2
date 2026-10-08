// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionDetectorComponent.h"

#include "InteractableInterface.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"

UInteractionDetectorComponent::UInteractionDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Typical scene object types an interactable may use.
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery1); // WorldStatic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery2); // WorldDynamic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery3); // Pawn
}

void UInteractionDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to bind straight away; if the pawn's input component is not ready yet,
	// TickComponent will retry until it is.
	TryBindInput();
	RefreshFocus();
}

void UInteractionDetectorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInputBound)
	{
		TryBindInput();
	}

	TimeSinceRefresh += DeltaTime;
	if (UpdateInterval <= 0.0f || TimeSinceRefresh >= UpdateInterval)
	{
		TimeSinceRefresh = 0.0f;
		RefreshFocus();
	}
}

void UInteractionDetectorComponent::TryInteract()
{
	AActor* Target = FocusedActor;
	if (!Target)
	{
		return;
	}

	if (IInteractableInterface* Interactable = Cast<IInteractableInterface>(Target))
	{
		if (Interactable->CanInteract(GetOwner()))
		{
			Interactable->OnInteract(GetOwner());
			// Focus may need refreshing after the interaction changed the state.
			RefreshFocus();
		}
	}
}

FText UInteractionDetectorComponent::GetCurrentPrompt() const
{
	if (FocusedActor)
	{
		if (const IInteractableInterface* Interactable = Cast<IInteractableInterface>(FocusedActor))
		{
			return Interactable->GetInteractionPrompt();
		}
	}
	return FText::GetEmpty();
}

void UInteractionDetectorComponent::RefreshFocus()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	SetFocusedActor(PickBestCandidate(Candidates));
}

void UInteractionDetectorComponent::GatherCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(GetOwner());

	if (PickMode == EInteractionPickMode::SphereOverlap)
	{
		UKismetSystemLibrary::SphereOverlapActors(
			this,
			Owner->GetActorLocation(),
			InteractionRadius,
			ProbeObjectTypes,
			nullptr,
			IgnoredActors,
			OutCandidates);
	}
	else
	{
		const FVector Start = Owner->GetActorLocation();
		const FVector End = Start + Owner->GetActorForwardVector() * TraceDistance;

		FHitResult Hit;
		const bool bHit = UKismetSystemLibrary::LineTraceSingle(
			this,
			Start,
			End,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			IgnoredActors,
			EDrawDebugTrace::None,
			Hit,
			true);

		if (bHit && Hit.GetActor())
		{
			OutCandidates.Add(Hit.GetActor());
		}
	}
}

AActor* UInteractionDetectorComponent::PickBestCandidate(const TArray<AActor*>& Candidates) const
{
	AActor* Best = nullptr;
	float BestScore = -FLT_MAX;

	for (AActor* Candidate : Candidates)
	{
		const float Score = ScoreCandidate(Candidate);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

float UInteractionDetectorComponent::ScoreCandidate(AActor* Candidate) const
{
	if (!Candidate || Candidate == GetOwner())
	{
		return -FLT_MAX;
	}

	IInteractableInterface* Interactable = Cast<IInteractableInterface>(Candidate);
	if (!Interactable || !Interactable->CanInteract(GetOwner()))
	{
		return -FLT_MAX;
	}

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return -FLT_MAX;
	}

	const FVector ToCandidate = Candidate->GetActorLocation() - Owner->GetActorLocation();
	const float Distance = ToCandidate.Size();
	const float Dot = FVector::DotProduct(Owner->GetActorForwardVector(), ToCandidate.GetSafeNormal());

	// Reject candidates behind the player when facing is required.
	if (bRequireFacing && Dot <= 0.0f)
	{
		return -FLT_MAX;
	}

	// Facing dominates, distance breaks ties.
	return Dot * 1000.0f - Distance;
}

void UInteractionDetectorComponent::SetFocusedActor(AActor* NewFocus)
{
	if (NewFocus == FocusedActor)
	{
		return;
	}

	if (FocusedActor)
	{
		if (IInteractableInterface* OldInteractable = Cast<IInteractableInterface>(FocusedActor))
		{
			OldInteractable->OnFocusEnd(GetOwner());
		}
	}

	FocusedActor = NewFocus;

	if (FocusedActor)
	{
		if (IInteractableInterface* NewInteractable = Cast<IInteractableInterface>(FocusedActor))
		{
			NewInteractable->OnFocusBegin(GetOwner());
		}
	}

	OnFocusChanged.Broadcast(FocusedActor, GetCurrentPrompt());
}

void UInteractionDetectorComponent::TryBindInput()
{
	if (bInputBound || !InteractAction)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Pawn->InputComponent))
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this,
			&UInteractionDetectorComponent::HandleInteractInput);
		bInputBound = true;
	}
}

void UInteractionDetectorComponent::HandleInteractInput()
{
	TryInteract();
}
