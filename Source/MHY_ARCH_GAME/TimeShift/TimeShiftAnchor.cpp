// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeShiftAnchor.h"

#include "TimeShiftSubsystem.h"
#include "Components/SceneComponent.h"

ATimeShiftAnchor::ATimeShiftAnchor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ATimeShiftAnchor::BeginPlay()
{
	Super::BeginPlay();

	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		Subsystem->RegisterAnchor(this, AnchorId, Era);

		if (bDefinesLayoutOrigin)
		{
			Subsystem->RegisterLayoutOrigin(this, Era);
		}
	}
}

void ATimeShiftAnchor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		if (bDefinesLayoutOrigin)
		{
			Subsystem->UnregisterLayoutOrigin(this);
		}

		Subsystem->UnregisterAnchor(this);
	}

	Super::EndPlay(EndPlayReason);
}
