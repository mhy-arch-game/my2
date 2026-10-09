// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeShiftInputComponent.h"

#include "TimeShiftSubsystem.h"

#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"

UTimeShiftInputComponent::UTimeShiftInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UTimeShiftInputComponent::BeginPlay()
{
	Super::BeginPlay();

	TryBindInput();
}

void UTimeShiftInputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The pawn's input component may not exist on the first frame; keep retrying.
	if (!bInputBound)
	{
		TryBindInput();
	}
}

void UTimeShiftInputComponent::RequestSwitch()
{
	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		Subsystem->SwitchEra();
	}
}

bool UTimeShiftInputComponent::CanRequestSwitch() const
{
	if (const UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		return Subsystem->CanSwitchEra();
	}
	return false;
}

void UTimeShiftInputComponent::TryBindInput()
{
	if (bInputBound || !SwitchAction)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Pawn->InputComponent))
	{
		EnhancedInput->BindAction(SwitchAction, ETriggerEvent::Started, this,
			&UTimeShiftInputComponent::HandleSwitchInput);
		bInputBound = true;
	}
}

void UTimeShiftInputComponent::HandleSwitchInput()
{
	RequestSwitch();
}
