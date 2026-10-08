// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevealLightVolume.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/RevealableInterface.h"

ARevealLightVolume::ARevealLightVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	LightZone = CreateDefaultSubobject<UBoxComponent>(TEXT("LightZone"));
	RootComponent = LightZone;
	LightZone->SetBoxExtent(FVector(300.0f, 300.0f, 200.0f));
	LightZone->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	LightZone->SetGenerateOverlapEvents(true);
	LightZone->OnComponentBeginOverlap.AddDynamic(this, &ARevealLightVolume::OnZoneOverlapBegin);
	LightZone->OnComponentEndOverlap.AddDynamic(this, &ARevealLightVolume::OnZoneOverlapEnd);
}

bool ARevealLightVolume::Qualifies(AActor* Actor) const
{
	if (!Actor || Actor == this)
	{
		return false;
	}

	if (bAnyPawnCarriesLight)
	{
		return Cast<APawn>(Actor) != nullptr;
	}

	// Otherwise require the actor (or its owning actor) to carry the 'LightCarrier' tag.
	AActor* TestActor = Actor;
	while (TestActor)
	{
		if (TestActor->ActorHasTag(TEXT("LightCarrier")))
		{
			return true;
		}
		TestActor = TestActor->GetOwner();
	}
	return false;
}

void ARevealLightVolume::OnZoneOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Qualifies(OtherActor))
	{
		if (ActiveCarrierCount == 0)
		{
			RevealAll();
		}
		++ActiveCarrierCount;
	}
}

void ARevealLightVolume::OnZoneOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Qualifies(OtherActor))
	{
		if (ActiveCarrierCount > 0)
		{
			--ActiveCarrierCount;
		}
		if (ActiveCarrierCount == 0)
		{
			UnrevealAll();
		}
	}
}

void ARevealLightVolume::RevealAll()
{
	for (TObjectPtr<AActor> RevealedActor : RevealedActors)
	{
		if (RevealedActor)
		{
			if (IRevealableInterface* Revealable = Cast<IRevealableInterface>(RevealedActor))
			{
				Revealable->RevealOn();
			}
		}
	}
}

void ARevealLightVolume::UnrevealAll()
{
	for (TObjectPtr<AActor> RevealedActor : RevealedActors)
	{
		if (RevealedActor)
		{
			if (IRevealableInterface* Revealable = Cast<IRevealableInterface>(RevealedActor))
			{
				Revealable->RevealOff();
			}
		}
	}
}
