// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RevealableInterface.h"
#include "RevealPlatform.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 *  RevealPlatform - the "invisible floor / impassable surface".
 *
 *  Implements IRevealableInterface (see RevealableInterface.h).
 *
 *  Behaviour:
 *   - Impassable state (default): physical collision DISABLED and surface hidden.
 *   - Passable state (lit):        physical collision ENABLED and surface visible,
 *     so the character can stand / walk over it.
 *
 *  Revealing is reference-counted (RevealOn / RevealOff), so a platform can be
 *  driven by several light zones at once without flickering off too early.
 *  Set bStaysRevealed to make it one-way (stays passable forever once lit).
 *
 *  Two boxes:
 *   - SolidBox    : the physical support. It is toggled on/off with the reveal,
 *                   and it BLOCKS characters (no overlap events).
 *   - DetectorBox : a generous overlap-only zone (extends above the surface) that
 *                   tracks whether a character is currently standing on / above the
 *                   platform, so RevealOff never drops a player standing on it.
 *
 *  The optional surface mesh (VisualMesh) is hidden while impassable. If its
 *  material exposes the OpacityParameterName scalar, opacity is driven by it.
 */
UCLASS()
class ARevealPlatform : public AActor, public IRevealableInterface
{
	GENERATED_BODY()

public:
	ARevealPlatform();

	// ~begin IRevealableInterface
	virtual void RevealOn() override;
	virtual void RevealOff() override;
	// ~end IRevealableInterface

	/** True while the surface is currently passable. */
	UFUNCTION(BlueprintPure, Category="LightReveal")
	bool IsRevealed() const { return bRevealed; }

protected:
	virtual void BeginPlay() override;

	// -- editor tuning -----------------------------------------------------
	/** If true, once lit the platform stays passable forever (one-way gate). */
	UPROPERTY(EditAnywhere, Category="LightReveal")
	bool bStaysRevealed = false;

	/** Scalar parameter name used on the surface material to drive opacity. */
	UPROPERTY(EditAnywhere, Category="LightReveal")
	FName OpacityParameterName = TEXT("Opacity");

	// -- components --------------------------------------------------------
	/** Physical support; its collision is toggled with the reveal state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> SolidBox;

	/** Overlap-only detector used to know when a character stands on us. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> DetectorBox;

	/** The surface mesh that fades / appears when revealed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VisualMesh;

private:
	// -- internal state ----------------------------------------------------
	bool bRevealed = false;          // currently passable
	int32 RevealRefCount = 0;        // number of active light sources
	int32 StandingPawnCount = 0;     // characters currently standing on us
	bool bUnrevealPending = false;   // wait for the floor to clear before hiding
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	void ApplyAlpha(float Alpha);
	void TryUnreveal();

	UFUNCTION()
	void OnDetectorOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnDetectorOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
