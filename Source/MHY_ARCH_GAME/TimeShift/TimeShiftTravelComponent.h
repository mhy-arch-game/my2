// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeShiftTypes.h"
#include "TimeShiftTravelComponent.generated.h"

class UTimeShiftSubsystem;

/**
 *  UTimeShiftTravelComponent - relocates the owner between era layouts on a switch.
 *
 *  Attach it to the player pawn. On an era change it builds a MAPPING TRANSFORM that
 *  converts a world transform from the old era's layout into the new era's layout,
 *  and applies it to the owner plus everything travelling with it.
 *
 *  Two mapping modes (ETimeShiftMappingMode):
 *   - LayoutOrigin (default): each era registers ONE layout reference (a
 *     ATimeShiftAnchor with bDefinesLayoutOrigin). The mapping is
 *     M = ToLayout * FromLayout.Inverse(), so positions map 1:1 between the two
 *     layouts. This is the "both layouts correspond" case and needs no per-area setup.
 *   - AnchorPair: find the anchor of the old era nearest to the owner, then jump to
 *     the anchor sharing its AnchorId in the new era. Use when layouts differ per area.
 *
 *  The other mode acts as a fallback when the preferred one has no data, so a
 *  partially authored map never teleports the player into the void.
 *
 *  Actors attached to the owner follow automatically and are therefore skipped to
 *  avoid applying the mapping twice.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UTimeShiftTravelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeShiftTravelComponent();

	/** Move the owner from one era layout to the other. Returns false when it stayed put. */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	bool TeleportToEra(ETimeEra FromEra, ETimeEra ToEra);

	/** How the position is mapped into the other era. */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	ETimeShiftMappingMode MappingMode = ETimeShiftMappingMode::LayoutOrigin;

	/** Extra actors that should travel with the owner (explicit list). */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	TArray<TObjectPtr<AActor>> CarriedActors;

	/**
	 * Also bring along nearby actors carrying CarriedActorTag.
	 * This is the practical way to author "carried items": the player is spawned at
	 * runtime, so a Blueprint default cannot reference a crate placed in the level.
	 */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bAutoCollectByTag = true;

	/** Tag that marks an actor as carried by the player. */
	UPROPERTY(EditAnywhere, Category="TimeShift", meta=(EditCondition="bAutoCollectByTag"))
	FName CarriedActorTag = TEXT("TimeTraveler");

	/** Radius around the owner within which tagged actors are picked up (0 = unlimited). */
	UPROPERTY(EditAnywhere, Category="TimeShift", meta=(EditCondition="bAutoCollectByTag", ClampMin="0.0"))
	float AutoCollectRadius = 800.0f;

	/** Whether the owner itself is relocated (disable to only move the carried actors). */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bTeleportOwner = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleEraChanged(ETimeEra NewEra);

private:
	/** Era we last observed, i.e. the era the owner is currently standing in. */
	ETimeEra LastKnownEra = ETimeEra::Ancient;

	/** Build the era-to-era mapping transform; false when neither mode has data. */
	bool ComputeMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra, ETimeEra ToEra,
		const FVector& WorldLocation, FTransform& OutMapping) const;

	bool TryLayoutMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra, ETimeEra ToEra,
		FTransform& OutMapping) const;

	bool TryAnchorMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra, ETimeEra ToEra,
		const FVector& WorldLocation, FTransform& OutMapping) const;

	/** Apply the mapping to the owner and every traveller. */
	void ApplyMapping(const FTransform& Mapping);
};
