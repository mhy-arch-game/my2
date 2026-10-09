// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "MovementAudioTypes.generated.h"

class USoundBase;

/**
 *  Movement events that can carry audio.
 */
UENUM(BlueprintType)
enum class EMovementAudioEvent : uint8
{
	/** A footstep (timing normally comes from an AnimNotify). */
	Footstep	UMETA(DisplayName = "Footstep"),

	/** Leaving the ground. */
	Jump		UMETA(DisplayName = "Jump"),

	/** Touching the ground again. */
	Land		UMETA(DisplayName = "Land")
};

/**
 *  One audio set: the sounds used for a given ground surface.
 *
 *  Every slot is a soft reference and may stay empty - the component then simply
 *  broadcasts the event without playing anything, so concrete sound assets can be
 *  added later (or the events can be routed to Wwise / MetaSounds) without touching
 *  any code.
 */
USTRUCT(BlueprintType)
struct FMovementAudioSet
{
	GENERATED_BODY()

	/** Physical surface this set applies to (SurfaceType_Default = the fallback set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TEnumAsByte<EPhysicalSurface> Surface;

	/** Sound played for a footstep on this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Footstep;

	/** Sound played when jumping off this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Jump;

	/** Sound played when landing on this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Land;

	/** Picks the sound matching an event (null when that slot is empty). */
	TSoftObjectPtr<USoundBase> GetSoundForEvent(EMovementAudioEvent Event) const
	{
		switch (Event)
		{
		case EMovementAudioEvent::Jump:		return Jump;
		case EMovementAudioEvent::Land:		return Land;
		case EMovementAudioEvent::Footstep:
		default:							return Footstep;
		}
	}
};
