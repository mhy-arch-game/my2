// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimeShiftTypes.h"
#include "TimeShiftAnchor.generated.h"

class USceneComponent;

/**
 *  ATimeShiftAnchor - a named reference point inside one era's layout.
 *
 *  Both the Ancient and the Modern layouts live in the same map at different world
 *  positions. Place one anchor per era at the spots that represent "the same place",
 *  give the pair the SAME AnchorId, and switching eras will move the player between
 *  them (see UTimeShiftTravelComponent).
 *
 *  Anchors register themselves with UTimeShiftSubsystem on BeginPlay and unregister
 *  on EndPlay, so no manual wiring is required.
 */
UCLASS()
class ATimeShiftAnchor : public AActor
{
	GENERATED_BODY()

public:
	ATimeShiftAnchor();

	/** Id shared by the paired anchors of both eras (e.g. "Gate_01"). */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	FName AnchorId = NAME_None;

	/** Which era layout this anchor belongs to. */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	ETimeEra Era = ETimeEra::Ancient;

	/**
	 * Mark this anchor as the LAYOUT REFERENCE of its era.
	 *
	 * When both era layouts are identical up to a rigid transform (the "layouts
	 * correspond" case), place ONE layout-origin anchor per era instead of pairing an
	 * anchor for every area; positions then map 1:1 between the two eras.
	 * Such an anchor may leave AnchorId empty - it only serves as the layout origin.
	 */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bDefinesLayoutOrigin = false;

	UFUNCTION(BlueprintPure, Category="TimeShift")
	FName GetAnchorId() const { return AnchorId; }

	UFUNCTION(BlueprintPure, Category="TimeShift")
	ETimeEra GetEra() const { return Era; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Root so the anchor is easy to place and see in the editor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="TimeShift", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot;
};
