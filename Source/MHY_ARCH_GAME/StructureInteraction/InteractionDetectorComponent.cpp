// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionDetectorComponent.h"

#include "InteractableComponent.h"
#include "InteractableInterface.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/Controller.h"
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

void UInteractionDetectorComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	OutLocation = Owner->GetActorLocation();
	OutRotation = Owner->GetActorRotation();

	// Prefer the camera: "aimed at" must mean "under the crosshair". The capsule
	// forward vector never pitches, so it cannot express looking up or down.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		}
	}
}

UInteractableComponent* UInteractionDetectorComponent::FindInteractableComponent(AActor* Target) const
{
	return Target ? Target->FindComponentByClass<UInteractableComponent>() : nullptr;
}

bool UInteractionDetectorComponent::IsInteractableTarget(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return true;
	}

	return FindInteractableComponent(Target) != nullptr;
}

bool UInteractionDetectorComponent::QueryCanInteract(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return IInteractableInterface::Execute_CanInteract(Target, GetOwner());
	}

	if (const UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		return Component->CanInteract(GetOwner());
	}

	return false;
}

void UInteractionDetectorComponent::QueryOnInteract(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnInteract(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyInteract(GetOwner());
	}
}

FText UInteractionDetectorComponent::QueryPrompt(AActor* Target) const
{
	if (!Target)
	{
		return FText::GetEmpty();
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return IInteractableInterface::Execute_GetInteractionPrompt(Target);
	}

	if (const UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		return Component->GetInteractionPrompt();
	}

	return FText::GetEmpty();
}

void UInteractionDetectorComponent::QueryFocusBegin(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnFocusBegin(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyFocusGained(GetOwner());
	}
}

void UInteractionDetectorComponent::QueryFocusEnd(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnFocusEnd(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyFocusLost(GetOwner());
	}
}

void UInteractionDetectorComponent::TryInteract()
{
	AActor* Target = FocusedActor;
	if (!Target)
	{
		return;
	}

	if (!QueryCanInteract(Target))
	{
		return;
	}

	QueryOnInteract(Target);

	// The interaction may have changed the state (e.g. Locked), so re-evaluate
	// the focus immediately instead of waiting for the next poll.
	RefreshFocus();
}

FText UInteractionDetectorComponent::GetCurrentPrompt() const
{
	return QueryPrompt(FocusedActor);
}

void UInteractionDetectorComponent::RefreshFocus()
{
	if (!GetOwner())
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
		// Nearby: centred on the pawn, because "close enough" is about the body.
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
		// Aimed: traced from the view point so it follows the crosshair.
		FVector ViewLocation;
		FRotator ViewRotation;
		GetViewPoint(ViewLocation, ViewRotation);

		const FVector End = ViewLocation + ViewRotation.Vector() * TraceDistance;

		FHitResult Hit;
		const bool bHit = UKismetSystemLibrary::LineTraceSingle(
			this,
			ViewLocation,
			End,
			UEngineTypes::ConvertToTraceType(ECC_Visibility),
			false,
			IgnoredActors,
			bDrawDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
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

	// Must actually be interactable right now.
	if (!IsInteractableTarget(Candidate) || !QueryCanInteract(Candidate))
	{
		return -FLT_MAX;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
	const float Distance = ToCandidate.Size();
	const float Dot = FVector::DotProduct(ViewRotation.Vector(), ToCandidate.GetSafeNormal());

	// Reject anything the player is not aiming at closely enough.
	if (bRequireFacing && Dot < MinFacingCosine)
	{
		return -FLT_MAX;
	}

	// Aiming dominates, distance breaks ties.
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
		QueryFocusEnd(FocusedActor);
	}

	FocusedActor = NewFocus;

	if (FocusedActor)
	{
		QueryFocusBegin(FocusedActor);
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
