// Copyright Epic Games, Inc. All Rights Reserved.

#include "UiCueTriggerVolume.h"

#include "Components/BoxComponent.h"

AUiCueTriggerVolume::AUiCueTriggerVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	// InitBoxExtent (not SetBoxExtent) - the Set* variant creates a body setup with an
	// empty name inside a constructor, which the engine forbids.
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->InitBoxExtent(FVector(200.0f, 200.0f, 200.0f));
	// Trigger preset: overlap only, blocks nothing - the volume must never stop the player.
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(false);
	Box->SetHiddenInGame(true);
	RootComponent = Box;
}

bool AUiCueTriggerVolume::ContainsLocation(const FVector& WorldLocation) const
{
	if (!Box)
	{
		return false;
	}

	// Geometric containment: transform the point into the box's space and compare with
	// its (scaled) extent. No overlap events, no collision response involved.
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Box->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X
		&& FMath::Abs(Local.Y) <= Extent.Y
		&& FMath::Abs(Local.Z) <= Extent.Z;
}

void AUiCueTriggerVolume::HandleTriggered()
{
	if (bTriggered)
	{
		return;
	}

	bTriggered = true;

	if (bDisableAfterTrigger)
	{
		SetActorEnableCollision(false);
		UE_LOG(LogTemp, Log, TEXT("[UiCue] 触发体积 %s（TriggerId=%s）已永久失效。"),
			*GetNameSafe(this), *TriggerId.ToString());
	}
}

void AUiCueTriggerVolume::SetVolumeExtent(FVector NewExtent)
{
	if (Box)
	{
		Box->SetBoxExtent(NewExtent);
	}
}
