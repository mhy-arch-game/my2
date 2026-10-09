// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "StructureTypes.h"
#include "StructureBlockComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

/**
 *  UStructureBlockComponent - one block of an interactive structure.
 *
 *  Approved organisation: a single structure Actor hosts N of these components.
 *  The block is a transform/state anchor; attach a StaticMeshComponent as a CHILD
 *  of it in the Blueprint (that child is the visible, collidable block).
 *
 *  The block carries:
 *   - the relative transform for each state (Closed / Open),
 *   - the material set used by the presentation layer,
 *   - the collision rule when opened.
 *
 *  It performs no interaction logic itself; AInteractiveStructure drives the state
 *  and UStructureVisualComponent applies the materials.
 *
 *  NOTE: the mesh is resolved from child components (first UStaticMeshComponent found),
 *  so no shared default subobject is created per block.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UStructureBlockComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UStructureBlockComponent();

	/** Relative transform used when the structure is Closed. */
	UPROPERTY(EditAnywhere, Category="Structure|Block")
	FTransform ClosedTransform;

	/** Relative transform used when the structure is Open. */
	UPROPERTY(EditAnywhere, Category="Structure|Block")
	FTransform OpenTransform;

	/** Disable this block's collision while the structure is Open. */
	UPROPERTY(EditAnywhere, Category="Structure|Block")
	bool bDisableCollisionWhenOpen = true;

	// -- material set (applied by the presentation layer) -------------------
	UPROPERTY(EditAnywhere, Category="Structure|Materials")
	TObjectPtr<UMaterialInterface> NormalMaterial;

	UPROPERTY(EditAnywhere, Category="Structure|Materials")
	TObjectPtr<UMaterialInterface> HighlightMaterial;

	UPROPERTY(EditAnywhere, Category="Structure|Materials")
	TObjectPtr<UMaterialInterface> ActiveMaterial;

	UPROPERTY(EditAnywhere, Category="Structure|Materials")
	TObjectPtr<UMaterialInterface> DisabledMaterial;

	/** Custom depth stencil value used for the focus outline (0 disables). */
	UPROPERTY(EditAnywhere, Category="Structure|Materials", meta=(ClampMin="0", ClampMax="255"))
	int32 HighlightStencilValue = 1;

	/** The mesh that represents this block (resolved from child components). */
	UFUNCTION(BlueprintPure, Category="Structure|Block")
	UStaticMeshComponent* GetBlockMesh();

	/** Target relative transform for the given structure state. */
	FTransform GetTargetTransform(EStructureState State) const;

	/** Apply the collision rule for the given structure state. */
	void ApplyCollisionForState(EStructureState State);

	/** Apply one of the configured materials to the block mesh. */
	void ApplyMaterial(UMaterialInterface* Material);

	/** Toggle the focus outline (custom depth stencil) on the block mesh. */
	void SetOutlineEnabled(bool bEnabled);

protected:
	virtual void BeginPlay() override;

private:
	/** Child mesh used as the block body; resolved lazily from child components. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Find (and cache) the first child StaticMeshComponent. */
	void ResolveMesh();
};
