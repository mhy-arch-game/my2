// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeShiftInputComponent.generated.h"

class UInputAction;

/**
 *  UTimeShiftInputComponent - binds a configurable key/action to the era switch.
 *
 *  Attach it to the player pawn (or character). Assign SwitchAction to an existing
 *  Enhanced Input action (reuse is fine - the project already ships several IA_*
 *  assets) and pressing it calls UTimeShiftSubsystem::SwitchEra().
 *
 *  Binding is attempted on BeginPlay and retried every tick until the pawn's
 *  Enhanced Input component exists, so it works regardless of setup order.
 *  RequestSwitch() is exposed for characters that prefer to bind manually.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UTimeShiftInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeShiftInputComponent();

	/** Trigger an era switch (same as pressing the configured key). */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	void RequestSwitch();

	/**
	 * Whether a switch is currently allowed (not travelling, cooldown finished).
	 * Use it to gray out a HUD prompt while the timed lock is active.
	 */
	UFUNCTION(BlueprintPure, Category="TimeShift")
	bool CanRequestSwitch() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Input action that performs the era switch.
	 * Reuse any existing action, or create a dedicated IA_TimeShift and point here.
	 */
	UPROPERTY(EditAnywhere, Category="TimeShift")
	TObjectPtr<UInputAction> SwitchAction;

private:
	bool bInputBound = false;

	void TryBindInput();

	void HandleSwitchInput();
};
