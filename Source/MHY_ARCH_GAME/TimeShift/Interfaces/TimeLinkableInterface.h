// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TimeShiftTypes.h"
#include "TimeLinkableInterface.generated.h"

/**
 *  ITimeLinkableInterface - RESERVED hook for cross-era linkage.
 *
 *  FUTURE FEATURE (not implemented yet): "moving an object in one era affects the
 *  linked object in the other era".
 *
 *  Contract: objects that represent the same thing in different eras share the same
 *  TimeLinkId. When one of them changes, it publishes a FTimeLinkState through
 *  UTimeShiftSubsystem::BroadcastLinkState, and every registered object with the
 *  matching id receives OnLinkedStateChanged.
 *
 *  Nothing in the current build calls OnLinkedStateChanged automatically; only the
 *  registration / broadcast plumbing exists so the feature can be completed later
 *  without changing the switching code.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UTimeLinkableInterface : public UInterface
{
	GENERATED_BODY()
};

class ITimeLinkableInterface
{
	GENERATED_BODY()

public:

	/** Stable id shared by the linked objects of both eras (e.g. "Bridge_01"). */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	virtual FName GetTimeLinkId() const = 0;

	/** RESERVED: called when a linked object publishes a new state. */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	virtual void OnLinkedStateChanged(const FTimeLinkState& State) {}
};
