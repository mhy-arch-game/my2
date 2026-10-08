// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RevealLightVolume.generated.h"

class UBoxComponent;
class AActor;

/**
 *  RevealLightVolume - a generic "light zone" trigger volume.
 *
 *  Represents the light already present in a level. Any actor that can carry
 *  that light (default: a Pawn) entering this volume "lights" the zone, which
 *  reveals every bound surface (anything implementing IRevealableInterface,
 *  e.g. ARevealPlatform) from impassable -> passable.
 *
 *  - While at least one qualifying actor is inside, bound platforms stay lit.
 *  - When the last qualifying actor leaves, platforms go back to impassable
 *    (unless a platform has bStaysRevealed / is still held by another zone).
 *
 *  It is fully decoupled: it holds generic actor references and only talks to
 *  them through IRevealableInterface, never through concrete platform types.
 */
UCLASS()
class ARevealLightVolume : public AActor
{
	GENERATED_BODY()

public:
	ARevealLightVolume();

protected:
	// -- editor tuning -----------------------------------------------------
	/** Actors that will be revealed while this zone is lit (should implement IRevealableInterface). */
	UPROPERTY(EditInstanceOnly, Category="LightReveal", meta=(DisplayName="Revealed Actors"))
	TArray<TObjectPtr<AActor>> RevealedActors;

	/** If true, any Pawn can carry the light; if false only actors whose owner is tagged 'LightCarrier' do. */
	UPROPERTY(EditAnywhere, Category="LightReveal")
	bool bAnyPawnCarriesLight = true;

	// -- components --------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> LightZone;

private:
	/** Number of qualifying actors currently inside the zone. */
	int32 ActiveCarrierCount = 0;

	bool Qualifies(AActor* Actor) const;

	UFUNCTION()
	void OnZoneOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnZoneOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void RevealAll();
	void UnrevealAll();
};
