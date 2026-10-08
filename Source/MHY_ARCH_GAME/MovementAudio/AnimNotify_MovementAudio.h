// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MovementAudioTypes.h"
#include "AnimNotify_MovementAudio.generated.h"

/**
 *  UAnimNotify_MovementAudio - fires a movement audio event at animation-accurate timing.
 *
 *  Drop this notify on the footstep frames of the walk/run/jump/land sequences; it finds
 *  the UMovementAudioComponent on the animated actor and forwards the event, so the sound
 *  timing follows the animation instead of the distance fallback.
 *
 *  With no sound assets assigned yet the notify is harmless: the component only
 *  broadcasts the event.
 */
UCLASS(meta=(DisplayName="Movement Audio"))
class UAnimNotify_MovementAudio : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Which movement audio event to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	EMovementAudioEvent Event = EMovementAudioEvent::Footstep;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;
};
