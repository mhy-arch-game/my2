// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractiveStructure.h"

#include "StructureBlockComponent.h"
#include "StructureVisualComponent.h"

#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AInteractiveStructure::AInteractiveStructure()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Presentation is always available; blocks are authored in the Blueprint.
	VisualComponent = CreateDefaultSubobject<UStructureVisualComponent>(TEXT("StructureVisual"));

	InteractPrompt = FText::FromString(TEXT("Interact"));
}

void AInteractiveStructure::BeginPlay()
{
	Super::BeginPlay();

	RefreshBlocks();

	CurrentState = InitialState;
	ApplyState(/*bInstant=*/true);
}

void AInteractiveStructure::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bTransitioning)
	{
		return;
	}

	const float Alpha = FMath::Clamp(
		DeltaSeconds / FMath::Max(TransitionDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);

	bool bAllSettled = true;

	for (TObjectPtr<UStructureBlockComponent>& Block : Blocks)
	{
		if (!Block)
		{
			continue;
		}

		const FTransform Target = Block->GetTargetTransform(CurrentState);
		FTransform Next = Block->GetRelativeTransform();
		Next.BlendWith(Target, Alpha);
		Block->SetRelativeTransform(Next);

		if (!Next.Equals(Target, 0.01f))
		{
			bAllSettled = false;
		}
	}

	if (bAllSettled)
	{
		// Snap exactly and stop ticking until the next transition.
		for (TObjectPtr<UStructureBlockComponent>& Block : Blocks)
		{
			if (Block)
			{
				Block->SetRelativeTransform(Block->GetTargetTransform(CurrentState));
			}
		}
		bTransitioning = false;
	}
}

void AInteractiveStructure::RefreshBlocks()
{
	Blocks.Reset();

	// GetComponents() wants a raw-pointer array; copy into the tracked array.
	TArray<UStructureBlockComponent*> Found;
	GetComponents<UStructureBlockComponent>(Found);
	for (UStructureBlockComponent* Block : Found)
	{
		if (Block)
		{
			Blocks.Add(Block);
		}
	}
}

void AInteractiveStructure::SetState(EStructureState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	// Locked / Disabled are terminal for interaction but can be set by other systems.
	CurrentState = NewState;
	ApplyState(/*bInstant=*/false);

	OnStateChanged.Broadcast(CurrentState);
}

void AInteractiveStructure::ToggleState()
{
	if (CurrentState == EStructureState::Locked || CurrentState == EStructureState::Disabled)
	{
		return;
	}

	SetState(CurrentState == EStructureState::Closed ? EStructureState::Open : EStructureState::Closed);
}

void AInteractiveStructure::ApplyState(bool bInstant)
{
	for (TObjectPtr<UStructureBlockComponent>& Block : Blocks)
	{
		if (Block)
		{
			Block->ApplyCollisionForState(CurrentState);
		}
	}

	if (VisualComponent)
	{
		VisualComponent->ApplyStateMaterials(CurrentState);
	}

	if (bInstant)
	{
		for (TObjectPtr<UStructureBlockComponent>& Block : Blocks)
		{
			if (Block)
			{
				Block->SetRelativeTransform(Block->GetTargetTransform(CurrentState));
			}
		}
		bTransitioning = false;
	}
	else
	{
		bTransitioning = true;
	}
}

bool AInteractiveStructure::CanInteract(AActor* Interactor) const
{
	return CurrentState != EStructureState::Locked && CurrentState != EStructureState::Disabled;
}

void AInteractiveStructure::OnInteract(AActor* Interactor)
{
	if (!CanInteract(Interactor))
	{
		return;
	}

	ToggleState();

	// Baseline presentation hook (reserved for richer feedback later).
	if (InteractSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, InteractSound, GetActorLocation());
	}
}

FText AInteractiveStructure::GetInteractionPrompt() const
{
	return InteractPrompt;
}

void AInteractiveStructure::OnFocusBegin(AActor* Interactor)
{
	if (VisualComponent)
	{
		VisualComponent->SetHighlighted(true);
	}
}

void AInteractiveStructure::OnFocusEnd(AActor* Interactor)
{
	if (VisualComponent)
	{
		VisualComponent->SetHighlighted(false);
	}
}
