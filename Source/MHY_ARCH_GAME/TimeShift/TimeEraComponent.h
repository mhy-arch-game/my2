// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeShiftTypes.h"
#include "TimeEraComponent.generated.h"

/**
 *  UTimeEraComponent - binds an actor to one era.
 *
 *  Attach to any actor that should only exist / be active in one of the two eras
 *  (useful for objects that live in a shared level, for both-era props, and as a
 *  safety net if the two eras are ever built inside a single level).
 *
 *  On each era change the owner is switched on or off:
 *   - Visibility (SetActorHiddenInGame)
 *   - Collision  (SetActorEnableCollision)
 *   - Tick       (SetActorTickEnabled, optional)
 *
 *  The component subscribes to UTimeShiftSubsystem::OnEraChanged and unsubscribes on
 *  EndPlay, so level travel does not leave dangling bindings behind.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UTimeEraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeEraComponent();

	/** The era this actor belongs to. */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	ETimeEra Era = ETimeEra::Ancient;

	/** If true the actor stays active in both eras (era-agnostic prop). */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bExistsInBothEras = false;

	/** Toggle the owner's visibility with the era. */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bGateVisibility = true;

	/** Toggle the owner's collision with the era. */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bGateCollision = true;

	/** Toggle the owner's tick with the era (off by default; enable if the actor simulates). */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	bool bGateTick = false;

	/** Apply an era immediately (also called automatically on era changes). */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	void ApplyEra(ETimeEra InEra);

	/** Whether the owner is currently active in the active era. */
	UFUNCTION(BlueprintPure, Category="TimeShift")
	bool IsActiveInCurrentEra() const { return bActiveInCurrentEra; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleEraChanged(ETimeEra NewEra);

private:
	bool bActiveInCurrentEra = true;

	void SetOwnerActive(bool bActive);
};
