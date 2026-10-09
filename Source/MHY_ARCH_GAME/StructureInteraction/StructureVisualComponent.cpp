// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructureVisualComponent.h"

#include "StructureBlockComponent.h"
#include "GameFramework/Actor.h"

UStructureVisualComponent::UStructureVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStructureVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveBlocks();
}

void UStructureVisualComponent::ResolveBlocks()
{
	CachedBlocks.Reset();

	TArray<UStructureBlockComponent*> Found;
	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents<UStructureBlockComponent>(Found);
	}

	for (UStructureBlockComponent* Block : Found)
	{
		if (Block)
		{
			CachedBlocks.Add(Block);
		}
	}
}

void UStructureVisualComponent::SetHighlighted(bool bInHighlighted)
{
	this->bHighlighted = bInHighlighted;

	if (!bUseCustomDepthOutline)
	{
		return;
	}

	// Blocks may be resolved after this component's BeginPlay; resolve lazily.
	if (CachedBlocks.Num() == 0)
	{
		ResolveBlocks();
	}

	for (TObjectPtr<UStructureBlockComponent>& Block : CachedBlocks)
	{
		if (Block)
		{
			Block->SetOutlineEnabled(bInHighlighted);
		}
	}
}

void UStructureVisualComponent::ApplyStateMaterials(EStructureState State)
{
	if (CachedBlocks.Num() == 0)
	{
		ResolveBlocks();
	}

	for (TObjectPtr<UStructureBlockComponent>& Block : CachedBlocks)
	{
		if (!Block)
		{
			continue;
		}

		switch (State)
		{
		case EStructureState::Open:
			Block->ApplyMaterial(Block->ActiveMaterial ? Block->ActiveMaterial : Block->NormalMaterial);
			break;

		case EStructureState::Locked:
		case EStructureState::Disabled:
			Block->ApplyMaterial(Block->DisabledMaterial ? Block->DisabledMaterial : Block->NormalMaterial);
			break;

		case EStructureState::Closed:
		default:
			Block->ApplyMaterial(Block->NormalMaterial);
			break;
		}
	}
}
