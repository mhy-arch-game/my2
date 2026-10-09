// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeShiftTravelComponent.h"

#include "TimeShiftSubsystem.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

UTimeShiftTravelComponent::UTimeShiftTravelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTimeShiftTravelComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		// Remember where we are now, so the first switch knows the "from" era.
		LastKnownEra = Subsystem->GetEra();
		Subsystem->OnEraChanged.AddDynamic(this, &UTimeShiftTravelComponent::HandleEraChanged);
	}
}

void UTimeShiftTravelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
	{
		Subsystem->OnEraChanged.RemoveDynamic(this, &UTimeShiftTravelComponent::HandleEraChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UTimeShiftTravelComponent::HandleEraChanged(ETimeEra NewEra)
{
	const ETimeEra FromEra = LastKnownEra;
	LastKnownEra = NewEra;

	TeleportToEra(FromEra, NewEra);
}

bool UTimeShiftTravelComponent::TeleportToEra(ETimeEra FromEra, ETimeEra ToEra)
{
	AActor* Owner = GetOwner();
	UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
	if (!Owner || !Subsystem || FromEra == ToEra)
	{
		return false;
	}

	FTransform Mapping;
	if (!ComputeMapping(Subsystem, FromEra, ToEra, Owner->GetActorLocation(), Mapping))
	{
		return false;
	}

	// Nothing to do when the mapping is effectively the identity.
	const bool bHasEffect =
		!Mapping.GetTranslation().IsNearlyZero() || !Mapping.GetRotation().IsIdentity();
	if (!bHasEffect)
	{
		return false;
	}

	ApplyMapping(Mapping);
	return true;
}

bool UTimeShiftTravelComponent::ComputeMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra,
	ETimeEra ToEra, const FVector& WorldLocation, FTransform& OutMapping) const
{
	if (!Subsystem)
	{
		return false;
	}

	// Preferred mode first, the other one as fallback.
	if (MappingMode == ETimeShiftMappingMode::LayoutOrigin)
	{
		return TryLayoutMapping(Subsystem, FromEra, ToEra, OutMapping)
			|| TryAnchorMapping(Subsystem, FromEra, ToEra, WorldLocation, OutMapping);
	}

	return TryAnchorMapping(Subsystem, FromEra, ToEra, WorldLocation, OutMapping)
		|| TryLayoutMapping(Subsystem, FromEra, ToEra, OutMapping);
}

bool UTimeShiftTravelComponent::TryLayoutMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra,
	ETimeEra ToEra, FTransform& OutMapping) const
{
	FTransform FromLayout;
	FTransform ToLayout;

	if (!Subsystem->GetLayoutTransform(FromEra, FromLayout) ||
		!Subsystem->GetLayoutTransform(ToEra, ToLayout))
	{
		return false;
	}

	// Rigid mapping: a world transform expressed in the old layout is re-expressed in
	// the new layout. Translation, rotation and scale of the layouts are honoured.
	OutMapping = ToLayout * FromLayout.Inverse();
	return true;
}

bool UTimeShiftTravelComponent::TryAnchorMapping(UTimeShiftSubsystem* Subsystem, ETimeEra FromEra,
	ETimeEra ToEra, const FVector& WorldLocation, FTransform& OutMapping) const
{
	AActor* SourceAnchor = Subsystem->FindNearestAnchor(FromEra, WorldLocation);
	if (!SourceAnchor)
	{
		return false;
	}

	const FName AnchorId = Subsystem->GetAnchorId(SourceAnchor);
	if (AnchorId.IsNone())
	{
		return false;
	}

	AActor* TargetAnchor = Subsystem->FindAnchor(AnchorId, ToEra);
	if (!TargetAnchor)
	{
		return false;
	}

	const FVector Delta = TargetAnchor->GetActorLocation() - SourceAnchor->GetActorLocation();
	if (Delta.IsNearlyZero())
	{
		return false;
	}

	OutMapping = FTransform(FRotator::ZeroRotator, Delta, FVector::OneVector);
	return true;
}

void UTimeShiftTravelComponent::ApplyMapping(const FTransform& Mapping)
{
	AActor* Owner = GetOwner();

	// Sample the pickup origin BEFORE moving, so the tag search uses where the
	// player was standing when the switch happened.
	const FVector Origin = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	if (bTeleportOwner && Owner)
	{
		Owner->SetActorTransform(Mapping * Owner->GetActorTransform(),
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Travellers = explicit list + tagged actors near the origin.
	TArray<AActor*> Travellers;
	for (const TObjectPtr<AActor>& Carried : CarriedActors)
	{
		if (Carried)
		{
			Travellers.AddUnique(Carried);
		}
	}

	if (bAutoCollectByTag && !CarriedActorTag.IsNone())
	{
		TArray<AActor*> Tagged;
		UGameplayStatics::GetAllActorsWithTag(this, CarriedActorTag, Tagged);

		const double RadiusSq = static_cast<double>(AutoCollectRadius) * static_cast<double>(AutoCollectRadius);
		for (AActor* Actor : Tagged)
		{
			if (!Actor || Actor == Owner)
			{
				continue;
			}

			if (AutoCollectRadius > 0.0f &&
				FVector::DistSquared(Actor->GetActorLocation(), Origin) > RadiusSq)
			{
				continue;
			}

			Travellers.AddUnique(Actor);
		}
	}

	for (AActor* Actor : Travellers)
	{
		if (!Actor)
		{
			continue;
		}

		// Actors attached to the owner already follow it; skip to avoid double mapping.
		if (Owner && Actor->GetAttachParentActor() == Owner)
		{
			continue;
		}

		Actor->SetActorTransform(Mapping * Actor->GetActorTransform(),
			/*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
}
