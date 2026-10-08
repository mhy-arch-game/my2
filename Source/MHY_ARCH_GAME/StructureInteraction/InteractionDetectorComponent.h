// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "StructureTypes.h"
#include "InteractionDetectorComponent.generated.h"

class UInputAction;
class UInteractableComponent;

/** Broadcast whenever the focused interaction candidate changes (HUD hook). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionFocusChanged, AActor*, FocusActor, FText, Prompt);

/**
 *  UInteractionDetectorComponent - player-side interaction picking & focus.
 *
 *  Attach to the player Character/Pawn. Periodically searches for the best
 *  nearby AND aimed-at target using either a sphere overlap or a line trace
 *  (PickMode), tracks the focused actor, fires OnFocusBegin / OnFocusEnd on it,
 *  and exposes TryInteract() to run the interaction from an input binding.
 *
 *  A target is any actor that either
 *    - implements IInteractableInterface (C++ or Blueprint), or
 *    - carries a UInteractableComponent,
 *  so each object brings its own logic and the detector stays type-agnostic.
 *
 *  Aiming uses the owner's VIEW point (camera) when the owner is controlled,
 *  so "aimed at" means "under the crosshair" rather than "in front of the
 *  capsule" (the capsule forward vector never pitches).
 *
 *  Input: if InteractAction is set, the component self-binds it to the owner's
 *  Enhanced Input component (retried on tick until it exists). Otherwise set
 *  InteractAction to None and call TryInteract() from the character's binding.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UInteractionDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionDetectorComponent();

	/** Execute the interaction on the currently focused actor (if allowed). */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void TryInteract();

	/** The actor currently focused for interaction (may be null). */
	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetFocusedActor() const { return FocusedActor; }

	/** Prompt text of the focused actor, or empty text when nothing is focused. */
	UFUNCTION(BlueprintPure, Category="Interaction")
	FText GetCurrentPrompt() const;

	/** Whether Target is an interaction target (interface or component) right now. */
	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsInteractableTarget(AActor* Target) const;

	/** Fired when the focused candidate changes (bind a HUD widget here). */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractionFocusChanged OnFocusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -- configuration -----------------------------------------------------
	/** Which picking strategy to use. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	EInteractionPickMode PickMode = EInteractionPickMode::SphereOverlap;

	/** Radius used by the sphere-overlap picking mode. */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="0.0"))
	float InteractionRadius = 250.0f;

	/** Distance used by the line-trace picking mode. */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="0.0"))
	float TraceDistance = 400.0f;

	/** Require the candidate to be aimed at, not just nearby. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	bool bRequireFacing = true;

	/**
	 * How precisely the candidate must be aimed at, as the minimum dot product
	 * between the view direction and the direction to the candidate:
	 *   0.0 = anything in the forward hemisphere
	 *   0.5 = within about 60 degrees
	 *   0.9 = within about 25 degrees (tight aiming)
	 * Only applied while bRequireFacing is true.
	 */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="-1.0", ClampMax="1.0"))
	float MinFacingCosine = 0.0f;

	/** Seconds between picking refreshes. */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="0.0"))
	float UpdateInterval = 0.1f;

	/** Object types considered when searching for candidates. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	TArray<TEnumAsByte<EObjectTypeQuery>> ProbeObjectTypes;

	/** Optional Enhanced Input action; when set the component self-binds it. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	TObjectPtr<UInputAction> InteractAction;

	/** Draw the picking trace for debugging. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	bool bDrawDebug = false;

private:
	/** Currently focused interaction candidate. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedActor;

	float TimeSinceRefresh = 0.0f;
	bool bInputBound = false;

	void RefreshFocus();
	void GatherCandidates(TArray<AActor*>& OutCandidates) const;
	AActor* PickBestCandidate(const TArray<AActor*>& Candidates) const;
	float ScoreCandidate(AActor* Candidate) const;
	void SetFocusedActor(AActor* NewFocus);
	void TryBindInput();
	void HandleInteractInput();

	/** View location/rotation (camera when controlled), else the owner's transform. */
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** The UInteractableComponent on Target, if any. */
	UInteractableComponent* FindInteractableComponent(AActor* Target) const;

	// -- unified queries: interface OR component ---------------------------
	bool QueryCanInteract(AActor* Target) const;
	void QueryOnInteract(AActor* Target);
	FText QueryPrompt(AActor* Target) const;
	void QueryFocusBegin(AActor* Target);
	void QueryFocusEnd(AActor* Target);
};
