// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "StructureTypes.h"
#include "InteractiveStructure.generated.h"

class USceneComponent;
class UStructureBlockComponent;
class UStructureVisualComponent;
class USoundBase;

/** Broadcast after the structure changed state (presentation / gameplay hook). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStructureStateChanged, EStructureState, NewState);

/**
 *  AInteractiveStructure - a controllable building structure built from blocks.
 *
 *  Approved organisation: one Actor hosting N UStructureBlockComponent children
 *  (added in the Blueprint), driven by a simple enum state machine:
 *
 *      Closed <-> Open        (interaction toggles)
 *      Locked / Disabled      (interaction refused)
 *
 *  On a state change every block interpolates from its ClosedTransform to its
 *  OpenTransform over TransitionDuration, its collision rule is applied, and the
 *  presentation component re-applies the material set.
 *
 *  It implements IInteractableInterface, so the player's UInteractionDetectorComponent
 *  can focus it, show a prompt and trigger OnInteract with the Interact input.
 */
UCLASS()
class AInteractiveStructure : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AInteractiveStructure();

	// ~begin IInteractableInterface
	virtual bool CanInteract(AActor* Interactor) const override;
	virtual void OnInteract(AActor* Interactor) override;
	virtual FText GetInteractionPrompt() const override;
	virtual void OnFocusBegin(AActor* Interactor) override;
	virtual void OnFocusEnd(AActor* Interactor) override;
	// ~end IInteractableInterface

	/** Move the structure to a specific state. */
	UFUNCTION(BlueprintCallable, Category="Structure")
	void SetState(EStructureState NewState);

	/** Toggle between Closed and Open (no-op when Locked/Disabled). */
	UFUNCTION(BlueprintCallable, Category="Structure")
	void ToggleState();

	/** Current logical state. */
	UFUNCTION(BlueprintPure, Category="Structure")
	EStructureState GetState() const { return CurrentState; }

	/** Fired after the state changed. */
	UPROPERTY(BlueprintAssignable, Category="Structure")
	FOnStructureStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// -- configuration -----------------------------------------------------
	/** State the structure starts in. */
	UPROPERTY(EditAnywhere, Category="Structure")
	EStructureState InitialState = EStructureState::Closed;

	/** Seconds the blocks take to travel between their Closed/Open transforms. */
	UPROPERTY(EditAnywhere, Category="Structure", meta=(ClampMin="0.0"))
	float TransitionDuration = 0.8f;

	/** Prompt text shown while the structure is focused. */
	UPROPERTY(EditAnywhere, Category="Structure")
	FText InteractPrompt;

	// -- presentation hooks (see Docs/StructureInteraction.md) -------------
	/**
	 * Optional sound played on a successful interaction.
	 * Baseline hook; the wider feedback set (particles, camera, animation) is
	 * documented as a reserved future addition.
	 */
	UPROPERTY(EditAnywhere, Category="Structure|Presentation")
	TObjectPtr<USoundBase> InteractSound;

	/** Root of the structure; block components attach to it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** Presentation coordinator (created automatically). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Structure", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStructureVisualComponent> VisualComponent;

private:
	/** Blocks belonging to this structure, gathered on BeginPlay. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStructureBlockComponent>> Blocks;

	EStructureState CurrentState = EStructureState::Closed;
	bool bTransitioning = false;

	void RefreshBlocks();
	void ApplyState(bool bInstant);
};
