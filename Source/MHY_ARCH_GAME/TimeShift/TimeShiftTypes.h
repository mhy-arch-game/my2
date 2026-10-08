// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeShiftTypes.generated.h"

class AActor;

/**
 *  The two eras the game switches between ("古今").
 */
UENUM(BlueprintType)
enum class ETimeEra : uint8
{
	/** 古 - the past era. */
	Ancient	UMETA(DisplayName = "Ancient (古)"),

	/** 今 - the present era. */
	Modern	UMETA(DisplayName = "Modern (今)")
};

/**
 *  How the traveller maps the player's position into the other era.
 */
UENUM(BlueprintType)
enum class ETimeShiftMappingMode : uint8
{
	/**
	 * Layout mapping: each era registers ONE layout reference (a layout-origin anchor)
	 * and positions are mapped through those two transforms. Best when both era layouts
	 * are identical up to a rigid transform - the "layouts correspond" case.
	 */
	LayoutOrigin	UMETA(DisplayName = "Layout Origin"),

	/**
	 * Anchor mapping: find the nearest anchor of the old era, then jump to the anchor
	 * sharing the same AnchorId in the new era. Use when the layouts are NOT uniform.
	 */
	AnchorPair		UMETA(DisplayName = "Anchor Pair")
};

/**
 *  The pair of anchor actors that represent the SAME place in both eras.
 *
 *  Both era layouts live in the same map at different world positions, so switching
 *  eras teleports the player (and whatever travels with them) by the offset between
 *  the two anchors of the matching AnchorId.
 */
USTRUCT()
struct FTimeShiftAnchorPair
{
	GENERATED_BODY()

	/** Anchor placed in the Ancient layout. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Ancient;

	/** Anchor placed in the Modern layout. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Modern;
};

/**
 *  Generic state payload published between linked objects of different eras.
 *
 *  RESERVED (future feature): moving an object in one era affecting the other.
 *  The payload is intentionally generic and extensible; the current switching
 *  implementation only defines and carries it, it does not yet act on it.
 */
USTRUCT(BlueprintType)
struct FTimeLinkState
{
	GENERATED_BODY()

	/** World transform of the linked object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift")
	FTransform Transform = FTransform::Identity;

	/** Whether the linked object is currently active/enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift")
	bool bEnabled = true;

	/** Free-form tag for future puzzle semantics. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift")
	FName Tag = NAME_None;
};
