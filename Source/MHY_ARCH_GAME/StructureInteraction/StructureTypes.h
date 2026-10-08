// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructureTypes.generated.h"

/**
 *  Logical state of an interactive structure.
 *  (Simple enum state machine, approved option.)
 */
UENUM(BlueprintType)
enum class EStructureState : uint8
{
	/** Structure is in its default, blocking configuration. */
	Closed		UMETA(DisplayName = "Closed"),

	/** Structure is in its opened / passable configuration. */
	Open		UMETA(DisplayName = "Open"),

	/** Structure cannot be interacted with until unlocked by other systems. */
	Locked		UMETA(DisplayName = "Locked"),

	/** Structure is temporarily out of service (e.g. mid-sequence). */
	Disabled	UMETA(DisplayName = "Disabled")
};

/**
 *  How the interaction detector picks its candidate.
 *  (Both modes implemented, configurable on the component.)
 */
UENUM(BlueprintType)
enum class EInteractionPickMode : uint8
{
	/** Sphere overlap around the owner, like the existing SideScrolling interaction. */
	SphereOverlap	UMETA(DisplayName = "Sphere Overlap"),

	/** Line trace from the owner's view, for precise aiming at a block. */
	LineTrace		UMETA(DisplayName = "Line Trace")
};
