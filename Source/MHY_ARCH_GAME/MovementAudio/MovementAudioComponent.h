// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MovementAudioTypes.h"
#include "MovementAudioComponent.generated.h"

class ACharacter;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** A footstep happened. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementFootstep, FName, SurfaceName, bool, bRunning);

/** A jump started. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementJump, FName, SurfaceName);

/** A landing happened (FallSpeed is the peak falling speed, for light/heavy grading). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementLand, FName, SurfaceName, float, FallSpeed);

/** Walk <-> run changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementRunStateChanged, bool, bRunning);

/**
 *  UMovementAudioComponent - the movement audio hub attached to a character.
 *
 *  It is an INTERFACE for movement sound, not a sound pack: every sound slot is an
 *  optional soft reference. With nothing assigned it still tracks the movement state
 *  and broadcasts events, so audio can be wired up later (in Blueprint, or to
 *  Wwise/MetaSounds) without changing any code.
 *
 *  What it tracks / exposes:
 *   - Walking vs running (horizontal speed against RunSpeedThreshold)
 *   - Jump (movement mode -> Falling with upward velocity)
 *   - Land  (LandedDelegate, carrying the peak fall speed)
 *   - Footsteps: timing should come from UAnimNotify_MovementAudio; a distance based
 *     fallback (bAutoFootstepByDistance) is available for testing before animations exist
 *   - Ground surface: a short downward trace resolves UPhysicalMaterial::SurfaceType,
 *     which selects the matching FMovementAudioSet (fallback = DefaultSet)
 *
 *  Playback policy: if the slot for the resolved surface has a sound, it is played at
 *  the character's location; otherwise only the event is broadcast.
 */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class UMovementAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementAudioComponent();

	// -- events (subscribe from Blueprint / audio systems) ------------------

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementFootstep OnFootstep;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementJump OnJump;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementLand OnLand;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementRunStateChanged OnRunStateChanged;

	// -- triggers (called by the AnimNotify, or any external system) ---------

	/** Play/broadcast a footstep for the surface under the character. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayFootstep();

	/** Play/broadcast the jump sound. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayJump();

	/** Play/broadcast the landing sound (uses the peak falling speed of the last air time). */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayLand();

	/** Generic entry point used by the AnimNotify. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayMovementAudioEvent(EMovementAudioEvent Event);

	// -- queries ------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="MovementAudio")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category="MovementAudio")
	bool IsInAir() const { return bInAir; }

	/** Name of the surface under the character (empty when nothing was hit). */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	FName GetCurrentSurfaceName() const;

	/** Name of the surface at an arbitrary world location. */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	FName GetSurfaceNameAtLocation(const FVector& WorldLocation) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -- sounds (leave empty until assets exist) ---------------------------
	/** Fallback set used when no surface-specific set matches. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	FMovementAudioSet DefaultSet;

	/** Per-surface sets (grass, stone, metal...). Missing surfaces fall back to DefaultSet. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TArray<FMovementAudioSet> SurfaceSets;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.0f;

	/** Random pitch range applied to each sound for variation. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.01"))
	float PitchMin = 0.95f;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.01"))
	float PitchMax = 1.05f;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TObjectPtr<USoundConcurrency> Concurrency;

	// -- detection ---------------------------------------------------------
	/** Detect jump / land from the character's movement automatically. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	bool bAutoDetectJumpAndLand = true;

	/** Horizontal speed above which the character counts as running. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="0.0"))
	float RunSpeedThreshold = 300.0f;

	/**
	 * Fallback footstep timing by travelled distance.
	 * Turn this off once the AnimNotify drives footsteps from the animation.
	 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	bool bAutoFootstepByDistance = false;

	/** Distance between automatic footsteps. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="1.0"))
	float FootstepDistance = 180.0f;

	/** How far below the character to look for the ground surface. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="0.0"))
	float SurfaceTraceDistance = 150.0f;

	/** Channel used by the surface trace (must be blocked by the ground). */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;

	bool bRunning = false;
	bool bInAir = false;

	/** Peak falling speed observed during the current air time. */
	float PeakFallSpeed = 0.0f;

	float DistanceSinceLastFootstep = 0.0f;
	FVector LastLocation = FVector::ZeroVector;

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** Refresh running / in-air state and the distance-based footstep fallback. */
	void UpdateMovementState(float DeltaTime);

	/** Find the sound set for a surface (falls back to DefaultSet). */
	const FMovementAudioSet& ResolveSet(EPhysicalSurface Surface) const;

	/** Resolve the ground surface under a location. */
	EPhysicalSurface ResolveSurfaceAtLocation(const FVector& WorldLocation) const;

	/** Play a soft sound at a location (silently does nothing when unset). */
	void PlaySoundAt(const TSoftObjectPtr<USoundBase>& Sound, const FVector& WorldLocation);
};
