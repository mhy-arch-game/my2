// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GravityZoneComponent.generated.h"

class UCharacterMovementComponent;

/** Broadcast when an actor enters or leaves the zone (AppliedGravityScale is its new value). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGravityZoneActorChanged, AActor*, Actor, float, AppliedGravityScale);

/** Authored movement values captured the moment an actor enters the zone. */
struct FGravityZoneEntry
{
	float GravityScale = 1.0f;
	float JumpZVelocity = 0.0f;
};

/**
 *  UGravityZoneComponent - a box-shaped region with reduced gravity.
 *
 *  The component IS the zone: place it, size the box, and every character whose
 *  capsule overlaps it has UCharacterMovementComponent::GravityScale multiplied
 *  by GravityScaleInside. On exit the character's own authored gravity scale
 *  (and jump velocity, when scaled) is restored exactly - the component never
 *  assumes the default value of 1.
 *
 *  It is query-only and hidden in game, so it never blocks movement.
 *
 *  NOTE on overlapping zones: each zone caches and restores its own values, so
 *  two zones overlapping the same character are NOT additive - the last one to
 *  apply wins, and leaving restores the authored value. Keep zones disjoint.
 */
UCLASS(ClassGroup=(Gravity), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UGravityZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UGravityZoneComponent();

	/** Multiplier applied to GravityScale while inside. 1 = unchanged, 0.3 = floaty, 0 = weightless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone", meta=(ClampMin="0.0", ClampMax="10.0"))
	float GravityScaleInside = 0.3f;

	/** Only affect Pawns (typical). Turn off to also affect anything with a CharacterMovementComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone")
	bool bAffectPawnsOnly = true;

	/**
	 * Also scale JumpZVelocity by sqrt(GravityScaleInside), so the jump keeps
	 * roughly the same height but hangs in the air longer - the classic
	 * low-gravity feel. Off by default (jumps then simply go much higher).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone")
	bool bScaleJumpVelocity = false;

	/** Whether Actor is currently inside the zone and being affected by it. */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	bool IsActorInside(const AActor* Actor) const;

	/** How many actors are currently tracked as inside the zone. */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	int32 GetTrackedActorCount() const { return Tracked.Num(); }

	UPROPERTY(BlueprintAssignable, Category="Gravity Zone")
	FOnGravityZoneActorChanged OnActorEntered;

	UPROPERTY(BlueprintAssignable, Category="Gravity Zone")
	FOnGravityZoneActorChanged OnActorExited;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/** Authored values per tracked actor, captured on entry. */
	TMap<TWeakObjectPtr<AActor>, FGravityZoneEntry> Tracked;

	static UCharacterMovementComponent* GetMovement(const AActor* Actor);

	/** Capture + apply reduced gravity (no-op if already tracked or not affected). */
	void ApplyTo(AActor* Actor);

	/** Write the captured authored values back. */
	void RestoreFrom(AActor* Actor, const FGravityZoneEntry& Entry);
};
