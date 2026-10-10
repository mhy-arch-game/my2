// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbSpot.h"

#include "ClimbComponent.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

AClimbSpot::AClimbSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	// InitBoxExtent: constructor-safe (SetBoxExtent would NewObject a BodySetup here).
	TriggerBox->InitBoxExtent(FVector(70.0f, 70.0f, 100.0f));
	TriggerBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AClimbSpot::OnTriggerBeginOverlap);
}

void AClimbSpot::BeginPlay()
{
	Super::BeginPlay();
}

FTransform AClimbSpot::GetAlignTransform() const
{
	// Location + facing of the spot. The component decides whether to apply the
	// rotation (bAlignCharacterRotation) and keeps the character's own yaw otherwise.
	const FVector Location = GetActorLocation() + AlignOffset;
	return FTransform(GetActorRotation(), Location, FVector::OneVector);
}

void AClimbSpot::NotifyClimbed()
{
	if (bSingleUse)
	{
		bConsumed = true;
	}
}

void AClimbSpot::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bAutoTriggerOnOverlap || bConsumed || !OtherActor)
	{
		return;
	}

	// Ask the character's climb component; the spot stays agnostic of the character type.
	if (UClimbComponent* ClimbComponent = OtherActor->FindComponentByClass<UClimbComponent>())
	{
		ClimbComponent->TryClimb(this);
	}
}
