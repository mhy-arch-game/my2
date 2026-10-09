// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "ClimbTypes.h"
#include "ClimbComponent.generated.h"

class AClimbSpot;
class ACharacter;
class UAnimMontage;
class UCurveFloat;
class UMotionWarpingComponent;

/** Broadcast when a climb starts / finishes on this component. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnClimbEvent);

/**
 *  UClimbComponent - plays a climb animation and delivers the character to the top of
 *  the platform directly above, keeping the SAME X/Y as the start position.
 *
 *  Attach to any ACharacter. Normally triggered by an AClimbSpot the character walks
 *  into, but TryClimb() can also be called from code/Blueprint.
 *
 *  Flow:
 *   1. validate (not already climbing, character present, montage available)
 *   2. optional alignment to the spot's align transform
 *   3. resolve the target: downward probe from (X, Y, Start.Z + TraceUpHeight) gives the
 *      platform top; the target keeps X/Y and sits one capsule half-height above it
 *      -> no platform found means the climb is refused
 *   4. lock locomotion, play the montage
 *   5a. MotionWarping mode: push the target into the MotionWarping component so the root
 *       motion lands on it
 *   5b. CurveDriven mode (automatic fallback when no MotionWarping component exists):
 *       interpolate the character vertically over the montage length
 *   6. on montage end: snap exactly onto the target (X/Y identical to the start),
 *      restore locomotion and broadcast OnClimbFinished
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UClimbComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UClimbComponent();

	/** Start a climb at the given spot. Returns false when the climb is not possible. */
	UFUNCTION(BlueprintCallable, Category="Climb")
	bool TryClimb(AClimbSpot* Spot);

	/** Abort an in-progress climb, restoring locomotion (no snap). */
	UFUNCTION(BlueprintCallable, Category="Climb")
	void CancelClimb();

	UFUNCTION(BlueprintPure, Category="Climb")
	bool IsClimbing() const { return bClimbing; }

	/** The target the character is heading to (valid while climbing). */
	UFUNCTION(BlueprintPure, Category="Climb")
	FVector GetClimbTargetLocation() const { return TargetLocation; }

	UPROPERTY(BlueprintAssignable, Category="Climb")
	FOnClimbEvent OnClimbStarted;

	UPROPERTY(BlueprintAssignable, Category="Climb")
	FOnClimbEvent OnClimbFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -- configuration -----------------------------------------------------
	/** Default climb montage (a spot may override it). */
	UPROPERTY(EditAnywhere, Category="Climb")
	TObjectPtr<UAnimMontage> ClimbMontage;

	/** How the displacement is produced. */
	UPROPERTY(EditAnywhere, Category="Climb")
	EClimbMoveMode MoveMode = EClimbMoveMode::MotionWarping;

	/** Warp target name; must match the MotionWarping notify state in the montage. */
	UPROPERTY(EditAnywhere, Category="Climb")
	FName WarpTargetName = TEXT("ClimbTarget");

	/** How far above the character the downward probe starts. */
	UPROPERTY(EditAnywhere, Category="Climb", meta=(ClampMin="0.0"))
	float TraceUpHeight = 400.0f;

	/** Trace channel used to find the platform top. */
	UPROPERTY(EditAnywhere, Category="Climb")
	TEnumAsByte<ECollisionChannel> LedgeTraceChannel = ECC_Visibility;

	/** Montage play rate. */
	UPROPERTY(EditAnywhere, Category="Climb", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.0f;

	/** Lock the character's rotation to the spot facing while climbing. */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bLockRotationDuringClimb = true;

	/** Snap exactly onto the resolved target when the montage ends (guarantees X/Y). */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bSnapToTargetOnFinish = true;

	/** Vertical profile used by CurveDriven mode (0..1 over the montage). Smoothstep if unset. */
	UPROPERTY(EditAnywhere, Category="Climb")
	TObjectPtr<UCurveFloat> ClimbHeightCurve;

private:
	bool bClimbing = false;
	bool bCurveDriven = false;

	/** Where the character was when the climb started. */
	FVector StartLocation = FVector::ZeroVector;

	/** Resolved destination: same X/Y as StartLocation, Z on top of the platform. */
	FVector TargetLocation = FVector::ZeroVector;

	float ElapsedTime = 0.0f;
	float ClimbDuration = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<AClimbSpot> ActiveSpot;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UMotionWarpingComponent> CachedWarping;

	/** Character yaw setting saved on lock, restored on unlock. */
	bool bSavedUseControllerRotationYaw = true;

	void ResolveCharacter();
	bool ComputeTargetLocation(const AClimbSpot* Spot, float& OutStepHeight);
	void ApplyAlignment(const AClimbSpot* Spot);
	void LockLocomotion();
	void RestoreLocomotion();
	void FinishClimb(bool bSnapToTarget);

	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
