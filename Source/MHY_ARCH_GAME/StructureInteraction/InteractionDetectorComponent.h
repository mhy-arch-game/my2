// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "StructureTypes.h"
#include "InteractionDetectorComponent.generated.h"

class UInputAction;

/** Broadcast whenever the focused interaction candidate changes (HUD hook). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionFocusChanged, AActor*, FocusActor, FText, Prompt);

/**
 *  UInteractionDetectorComponent - player-side interaction picking & focus.
 *
 *  Attach to the player Character/Pawn. Periodically searches for the best nearby
 *  IInteractableInterface actor using either a sphere overlap or a line trace
 *  (both implemented, selectable via PickMode), keeps track of the focused actor,
 *  fires OnFocusBegin / OnFocusEnd on it, and exposes TryInteract() to execute the
 *  interaction from an input binding.
 *
 *  Input: if InteractAction is set, the component tries to self-bind it to the
 *  owner's Enhanced Input component (retried on tick until the input component
 *  exists). Otherwise call TryInteract() from the character's own binding.
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

	/** Require the candidate to be roughly in front of the owner. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	bool bRequireFacing = true;

	/** Seconds between picking refreshes. */
	UPROPERTY(EditAnywhere, Category="Interaction", meta=(ClampMin="0.0"))
	float UpdateInterval = 0.1f;

	/** Object types considered when searching for candidates. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	TArray<TEnumAsByte<EObjectTypeQuery>> ProbeObjectTypes;

	/** Optional Enhanced Input action; when set the component self-binds it. */
	UPROPERTY(EditAnywhere, Category="Interaction")
	TObjectPtr<UInputAction> InteractAction;

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
};
