// Copyright Epic Games, Inc. All Rights Reserved.

#include "StructureBlockComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

UStructureBlockComponent::UStructureBlockComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Sensible defaults so a freshly added block has distinct Closed/Open poses
	// (designers override these in the Blueprint).
	ClosedTransform = FTransform::Identity;
	OpenTransform = FTransform::Identity;
}

void UStructureBlockComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveMesh();
}

void UStructureBlockComponent::ResolveMesh()
{
	if (Mesh)
	{
		return;
	}

	// The block body is the first StaticMeshComponent attached beneath this component.
	TArray<USceneComponent*> Children;
	GetChildrenComponents(/*bIncludeAllDescendants=*/true, Children);

	for (USceneComponent* Child : Children)
	{
		if (UStaticMeshComponent* MeshChild = Cast<UStaticMeshComponent>(Child))
		{
			Mesh = MeshChild;
			return;
		}
	}
}

UStaticMeshComponent* UStructureBlockComponent::GetBlockMesh()
{
	ResolveMesh();
	return Mesh;
}

FTransform UStructureBlockComponent::GetTargetTransform(EStructureState State) const
{
	return (State == EStructureState::Open) ? OpenTransform : ClosedTransform;
}

void UStructureBlockComponent::ApplyCollisionForState(EStructureState State)
{
	UStaticMeshComponent* BlockMesh = GetBlockMesh();
	if (!BlockMesh)
	{
		return;
	}

	const bool bSolid = !(State == EStructureState::Open && bDisableCollisionWhenOpen);
	BlockMesh->SetCollisionEnabled(bSolid
		? ECollisionEnabled::QueryAndPhysics
		: ECollisionEnabled::NoCollision);
}

void UStructureBlockComponent::ApplyMaterial(UMaterialInterface* Material)
{
	UStaticMeshComponent* BlockMesh = GetBlockMesh();
	if (BlockMesh && Material)
	{
		BlockMesh->SetMaterial(0, Material);
	}
}

void UStructureBlockComponent::SetOutlineEnabled(bool bEnabled)
{
	UStaticMeshComponent* BlockMesh = GetBlockMesh();
	if (!BlockMesh)
	{
		return;
	}

	const bool bUseOutline = bEnabled && HighlightStencilValue > 0;
	BlockMesh->SetRenderCustomDepth(bUseOutline);
	if (bUseOutline)
	{
		BlockMesh->SetCustomDepthStencilValue(HighlightStencilValue);
	}
}
