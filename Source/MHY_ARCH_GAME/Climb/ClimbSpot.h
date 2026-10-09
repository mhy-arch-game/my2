// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ClimbSpot.generated.h"

class UBoxComponent;
class UAnimMontage;
class UPrimitiveComponent;

/**
 *  AClimbSpot - a designated place where a character can climb onto the platform above.
 *
 *  Placed by a designer at the foot of a climbable wall/ledge. It carries only data
 *  (where to stand, how high is climbable, which montage to use) and does not know
 *  anything about the character: when a character with a UClimbComponent enters its
 *  trigger volume, the component is asked to climb (see UClimbComponent::TryClimb).
 *
 *  The actual target - the top of the platform directly above the character, keeping
 *  the same X/Y - is resolved by the component (automatic downward probe), so the spot
 *  never needs a hand-typed height.
 */
UCLASS()
class AClimbSpot : public AActor
{
	GENERATED_BODY()

public:
	AClimbSpot();

	/** Where the character is aligned before the animation starts (relative to the spot). */
	UPROPERTY(EditAnywhere, Category="Climb")
	FVector AlignOffset = FVector(0.0f, 0.0f, 0.0f);

	/** Move the character to the align transform on climb start (keeps the animation lined up). */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bAlignCharacterOnStart = true;

	/**
	 * Use the spot's own rotation as the character's facing while climbing.
	 * Place the spot facing the wall so the animation plays in the right direction.
	 */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bAlignCharacterRotation = true;

	/** Highest step up (from the character's feet) that may be climbed. */
	UPROPERTY(EditAnywhere, Category="Climb", meta=(ClampMin="0.0"))
	float MaxClimbHeight = 300.0f;

	/** Overrides the component's default climb montage when set. */
	UPROPERTY(EditAnywhere, Category="Climb")
	TObjectPtr<UAnimMontage> ClimbMontageOverride;

	/** Start the climb automatically when a character enters the trigger volume. */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bAutoTriggerOnOverlap = true;

	/** Only usable once (becomes inert after a successful climb). */
	UPROPERTY(EditAnywhere, Category="Climb")
	bool bSingleUse = false;

	/** Prompt / debug name for the spot. */
	UPROPERTY(EditAnywhere, Category="Climb")
	FName SpotName = NAME_None;

	/** Transform the character should occupy when the climb starts. */
	UFUNCTION(BlueprintPure, Category="Climb")
	FTransform GetAlignTransform() const;

	/** True once a single-use spot has been consumed. */
	UFUNCTION(BlueprintPure, Category="Climb")
	bool IsConsumed() const { return bConsumed; }

	/** Called by UClimbComponent after a successful climb (consumes single-use spots). */
	UFUNCTION(BlueprintCallable, Category="Climb")
	void NotifyClimbed();

protected:
	virtual void BeginPlay() override;

	/** Volume that detects a candidate character. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Climb", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bConsumed = false;
};
