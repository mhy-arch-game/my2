// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractableEvent, AActor*, Interactor, AActor*, Interactable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractableToggled, AActor*, Interactable, bool, bIsOpen);

/**
 *  UInteractableComponent - interaction capability you can attach to ANY actor.
 *
 *  Drop this on an object to make it interactable without changing its class:
 *  the player's UInteractionDetectorComponent focuses the owner when near AND
 *  aimed at, shows InteractionPrompt, and fires OnInteractRequested when the
 *  interact key is pressed.
 *
 *  Two ways to give it behaviour:
 *    1. bind OnInteractRequested in Blueprint (full freedom), and/or
 *    2. enable the optional built-in toggle below, which drives components
 *       between a closed and an open relative transform - enough for a plain
 *       door with no Blueprint graph at all.
 *
 *  Counterpart to IInteractableInterface:
 *    - interface       -> implement on the class (C++ or Blueprint)
 *    - this component  -> attach to a specific object
 *  The detector accepts either.
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractableComponent();

	/** Whether the owner can be interacted with right now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	bool bEnabled = true;

	/** Text shown by the HUD while the owner is focused. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	FText InteractionPrompt;

	/** Fired when the interact key is pressed while the owner is focused. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnInteractRequested;

	/** Fired when the owner becomes the focused interaction candidate. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnFocusGained;

	/** Fired when the owner stops being the focused candidate. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FOnInteractableEvent OnFocusLost;

	// -- optional built-in open/close (a plain door needs no Blueprint graph) --
	/**
	 * When enabled, interacting moves ToggleComponents between
	 * ClosedRelativeTransform and OpenRelativeTransform. Leave OFF to keep all
	 * behaviour in Blueprint via OnInteractRequested.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle")
	bool bUseBuiltInToggle = false;

	/**
	 * Components driven by the toggle - normally the door mesh (a child component).
	 * NOTE: a Blueprint's Class Defaults object picker only lists ASSETS, so it is
	 * often impossible to pick a sibling component here. Use ToggleComponentNames
	 * instead when that happens.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	TArray<TObjectPtr<USceneComponent>> ToggleComponents;

	/**
	 * Same thing, addressed by COMPONENT NAME - just type the name shown in the
	 * Blueprint's Components panel (e.g. "DoorMesh"). Only used when
	 * ToggleComponents is empty, so either way works.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	TArray<FName> ToggleComponentNames;

	/** Relative transform while closed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	FTransform ClosedRelativeTransform;

	/** Relative transform while open (e.g. rotate 90 deg around Z for a door). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	FTransform OpenRelativeTransform;

	/** Seconds to travel between the two transforms. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle", ClampMin="0.0"))
	float ToggleDuration = 0.6f;

	/** Start in the open state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bStartOpen = false;

	/** Disable collision on the toggle components while open (so you can walk through). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bDisableCollisionWhenOpen = true;

	/** True while the built-in toggle is in its open state. */
	UFUNCTION(BlueprintPure, Category="Interaction|Built-in Toggle")
	bool IsOpen() const { return bIsOpen; }

	/** Drive the built-in toggle directly (NotifyInteract calls this too). */
	UFUNCTION(BlueprintCallable, Category="Interaction|Built-in Toggle")
	void SetOpen(bool bNewOpen, bool bInstant = false);

	/** Fired after the built-in toggle changed state. */
	UPROPERTY(BlueprintAssignable, Category="Interaction|Built-in Toggle")
	FOnInteractableToggled OnToggleChanged;

	// -- optional light switch (driven by the SAME interaction state) -------
	/**
	 * Also switch lights with this interaction, so one component serves a lamp
	 * as well as a door. Lights are ON while IsOpen() is true, so set
	 * bStartOpen to whether the lamp starts lit.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Light Switch")
	bool bToggleLights = false;

	/**
	 * Lights switched by the interaction. Their authored intensity is restored
	 * when switched back on.
	 * (If a Blueprint's picker refuses to list sibling components, use the names
	 * array below instead.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Light Switch", meta=(EditCondition="bToggleLights"))
	TArray<TObjectPtr<class ULightComponent>> LightComponents;

	/** Same, addressed by COMPONENT NAME - type the name shown in the Components panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Light Switch", meta=(EditCondition="bToggleLights"))
	TArray<FName> LightComponentNames;

	/** True while the lights are lit (mirrors IsOpen()). */
	UFUNCTION(BlueprintPure, Category="Interaction|Light Switch")
	bool AreLightsOn() const { return bIsOpen; }

	// -- queried by UInteractionDetectorComponent --------------------------
	virtual bool CanInteract(AActor* Interactor) const;
	virtual FText GetInteractionPrompt() const { return InteractionPrompt; }
	virtual void NotifyInteract(AActor* Interactor);
	virtual void NotifyFocusGained(AActor* Interactor);
	virtual void NotifyFocusLost(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	bool IsEnabled() const { return bEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bIsOpen = false;
	bool bTransitioning = false;

	/** Collision state captured on BeginPlay, restored when the toggle closes. */
	TArray<TEnumAsByte<ECollisionEnabled::Type>> InitialCollision;

	/** Push the transform and collision for the current state onto the components. */
	void ApplyToggleState(bool bInstant);

	/** Fill ToggleComponents: explicit references first, then names. */
	void ResolveToggleComponents();

	/** Fill LightComponents: explicit references first, then names. */
	void ResolveLightComponents();

	/** Push the on/off state onto LightComponents. */
	void ApplyLightState();

	/** Authored intensity per light, restored when switched back on. */
	TArray<float> InitialLightIntensity;
};

