// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNotify_MovementAudio.h"

#include "MovementAudioComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_MovementAudio::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	// The component may live on the animated actor itself or on its owner.
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	UMovementAudioComponent* MovementAudio = Owner->FindComponentByClass<UMovementAudioComponent>();
	if (!MovementAudio)
	{
		if (AActor* AttachOwner = Owner->GetOwner())
		{
			MovementAudio = AttachOwner->FindComponentByClass<UMovementAudioComponent>();
		}
	}

	if (MovementAudio)
	{
		MovementAudio->PlayMovementAudioEvent(Event);
	}
}

FString UAnimNotify_MovementAudio::GetNotifyName_Implementation() const
{
	switch (Event)
	{
	case EMovementAudioEvent::Jump:		return TEXT("MovementAudio: Jump");
	case EMovementAudioEvent::Land:		return TEXT("MovementAudio: Land");
	case EMovementAudioEvent::Footstep:
	default:							return TEXT("MovementAudio: Footstep");
	}
}
