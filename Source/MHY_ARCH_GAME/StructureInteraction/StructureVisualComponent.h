// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StructureTypes.h"
#include "StructureVisualComponent.generated.h"

class UStructureBlockComponent;

/**
 *  UStructureVisualComponent - presentation coordinator for an interactive structure.
 *
 *  Applies the material set for the current structure state and toggles the focus
 *  outline on the structure's blocks.
 *
 *  PRESENTATION SCOPE (see Docs/StructureInteraction.md):
 *   - Implemented now : state -> material selection, custom-depth stencil toggling,
 *     and the hook points (colour/sound) other systems can drive.
 *   - Reserved / future : the post-process outline material asset, sound & particle
 *     feedback, and any per-block animated transitions. These are marked as
 *     optimisation / feature additions in the documentation.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UStructureVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStructureVisualComponent();

	/** Enable / disable the focus outline on every block of the structure. */
	UFUNCTION(BlueprintCallable, Category="Structure|Visual")
	void SetHighlighted(bool bHighlighted);

	/** Apply the material set that matches the given structure state. */
	UFUNCTION(BlueprintCallable, Category="Structure|Visual")
	void ApplyStateMaterials(EStructureState State);

	/** True while the focus outline is enabled. */
	UFUNCTION(BlueprintPure, Category="Structure|Visual")
	bool IsHighlighted() const { return bHighlighted; }

protected:
	virtual void BeginPlay() override;

	/**
	 * Master switch for the custom-depth outline.
	 * Reserved hook: the post-process material that reads the stencil is created in
	 * the editor as a future addition.
	 */
	UPROPERTY(EditAnywhere, Category="Structure|Visual")
	bool bUseCustomDepthOutline = true;

private:
	/** Blocks of the owning structure, cached on BeginPlay. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStructureBlockComponent>> CachedBlocks;

	bool bHighlighted = false;

	void ResolveBlocks();
};
