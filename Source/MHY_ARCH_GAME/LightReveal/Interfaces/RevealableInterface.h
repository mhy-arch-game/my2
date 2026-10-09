// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RevealableInterface.generated.h"

/**
 *  LightReveal Interface (generic, gameplay-agnostic)
 *
 *  Decoupling contract between a "light / reveal trigger" and anything that can
 *  transition from impassable -> passable when it gets lit:
 *
 *      ARevealLightVolume (light zone)  --reveal-->  IRevealableInterface  --state-->  ARevealPlatform (surface)
 *
 *  Any actor implementing this interface can be "lit" (RevealOn) or go back to
 *  being hidden (RevealOff). Implementations are free to expose extra blueprint
 *  hooks; the interface only defines the low-level state toggles.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class URevealableInterface : public UInterface
{
	GENERATED_BODY()
};

class IRevealableInterface
{
	GENERATED_BODY()

public:

	/** The area/object becomes revealed -> passable. Called when light hits it. */
	UFUNCTION(BlueprintCallable, Category="LightReveal")
	virtual void RevealOn() = 0;

	/** The area/object goes back to hidden -> impassable. Called when light leaves it. */
	UFUNCTION(BlueprintCallable, Category="LightReveal")
	virtual void RevealOff() = 0;
};
