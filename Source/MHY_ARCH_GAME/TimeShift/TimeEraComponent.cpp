// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeEraComponent.h"

#include "TimeShiftSubsystem.h"
#include "GameFramework/Actor.h"

UTimeEraComponent::UTimeEraComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTimeEraComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		Subsystem->OnEraChanged.AddDynamic(this, &UTimeEraComponent::HandleEraChanged);
		ApplyEra(Subsystem->GetEra());
	}
}

void UTimeEraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		Subsystem->OnEraChanged.RemoveDynamic(this, &UTimeEraComponent::HandleEraChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UTimeEraComponent::HandleEraChanged(ETimeEra NewEra)
{
	ApplyEra(NewEra);
}

void UTimeEraComponent::ApplyEra(ETimeEra InEra)
{
	bActiveInCurrentEra = bExistsInBothEras || (Era == InEra);
	SetOwnerActive(bActiveInCurrentEra);
}

void UTimeEraComponent::SetOwnerActive(bool bActive)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bGateVisibility)
	{
		Owner->SetActorHiddenInGame(!bActive);
	}

	if (bGateCollision)
	{
		Owner->SetActorEnableCollision(bActive);
	}

	if (bGateTick)
	{
		Owner->SetActorTickEnabled(bActive);
	}
}
