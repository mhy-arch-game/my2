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

	/** Relative transform while open. Ignored when bUseAxisRotation is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && !bUseAxisRotation"))
	FTransform OpenRelativeTransform;

	// -- axis / bearing rotation -------------------------------------------
	/**
	 * Drive the open pose by rotating around an axis through RotationPivot
	 * instead of lerping Closed -> Open. This lets a single component behave as
	 * a hinge without restructuring the Blueprint hierarchy (no extra Hinge scene
	 * component needed): the pivot is expressed in this component's own space.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bUseAxisRotation = false;

	/** Axis to rotate around, in this component's local space. (0,0,1) = Z (vertical door hinge). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && bUseAxisRotation"))
	FVector RotationAxis = FVector(0.0f, 0.0f, 1.0f);

	/** Signed angle in degrees applied when opening. Use -90 to swing the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && bUseAxisRotation"))
	float OpenAngleDegrees = 90.0f;

	/**
	 * Point the axis passes through, in this component's local space. This is the
	 * hinge: e.g. with a door mesh whose origin is at its centre and width 100,
	 * set (-50, 0, 0) to hinge on the -X edge. (0,0,0) spins around the mesh origin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && bUseAxisRotation"))
	FVector RotationPivot = FVector::ZeroVector;

	/** Seconds to travel between the two transforms. 0 = instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle", ClampMin="0.0"))
	float ToggleDuration = 0.6f;

	/** Start in the open state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bStartOpen = false;

	/** Disable collision on the toggle components while open (so you can walk through). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bDisableCollisionWhenOpen = true;

	/**
	 * Extra ACTORS whose collision follows this interaction's open state without being
	 * moved - the only lever when the thing sealing the opening is not the door itself.
	 *
	 * A door FRAME is normally a separate StaticMeshActor (see the menkuang* actors in
	 * firstvision). If that frame's simple collision fills the opening, swinging the
	 * door clear makes no difference: the doorway stays impassable. Point this at the
	 * frame and its collision is switched off while the door is open and restored when
	 * it closes. Only applied together with bDisableCollisionWhenOpen.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bDisableCollisionWhenOpen"))
	TArray<TObjectPtr<AActor>> CollidersDisabledWhenOpen;

	// -- interaction proxy (stay detectable in ANY state) -------------------
	/**
	 * Keep an invisible interaction PROXY box at the object's closed pose, so it stays
	 * detectable after it has been toggled open.
	 *
	 * Without it a door is unfindable the moment it opens: the only collidable thing
	 * (the panel) has moved out of the crosshair AND bDisableCollisionWhenOpen just
	 * switched its collision off, so neither the aimed trace nor the overlap fallback
	 * can see anything. The old code only papered over that by keeping a stale focus
	 * alive, which is exactly the "must not lose focus" prerequisite we want gone.
	 *
	 * The proxy is query-only (it blocks the interaction trace and NOTHING else, so it
	 * never blocks the player), invisible, never moves, and is skipped by the focus
	 * outline because that only touches visible primitives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle"))
	bool bUseInteractionProxy = true;

	/** Explicit proxy box extent. Zero = derive it from the toggle components' closed bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && bUseInteractionProxy"))
	FVector InteractionProxyExtent = FVector::ZeroVector;

	/**
	 * Grow the proxy half-extent by this much on every axis. A small value (1-2) makes
	 * the proxy protrude past a door frame casing that sits proud of the panel, so the
	 * ray reaches the proxy before touching the frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Built-in Toggle", meta=(EditCondition="bUseBuiltInToggle && bUseInteractionProxy", ClampMin="0.0"))
	float InteractionProxyPadding = 0.0f;

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

	/**
	 * The relative transform the toggle drives toward right now: Closed when
	 * closed; when open either OpenRelativeTransform, or the closed pose rotated
	 * around RotationAxis through RotationPivot.
	 */
	FTransform GetTargetTransform() const;

	/** Fill ToggleComponents: explicit references first, then names. */
	void ResolveToggleComponents();

	/** Fill LightComponents: explicit references first, then names. */
	void ResolveLightComponents();

	/** Push the on/off state onto LightComponents. */
	void ApplyLightState();

	/** Authored intensity per light, restored when switched back on. */
	TArray<float> InitialLightIntensity;

	/** Query-only box that keeps this object detectable while it is open. */
	UPROPERTY(Transient)
	TObjectPtr<class UBoxComponent> InteractionProxy;

	/** Bounds of the toggle components at the current (closed) pose, in owner space. */
	FBox ComputeClosedPoseBounds() const;

	/** Create the interaction proxy at the closed pose (called once, on BeginPlay). */
	void CreateInteractionProxy();

	/** Primitives of CollidersDisabledWhenOpen, gathered on BeginPlay. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> ExtraColliderPrimitives;

	/** Their authored collision state, restored when the toggle closes. */
	TArray<TEnumAsByte<ECollisionEnabled::Type>> ExtraColliderInitialCollision;

	/** Gather the extra colliders and remember their authored collision. */
	void ResolveExtraColliders();
};

