// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProximityBarrierComponent.h"

#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UProximityBarrierComponent::UProximityBarrierComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void UProximityBarrierComponent::BeginPlay()
{
	Super::BeginPlay();

	// The authored state IS the "materialised" state, so collect it first and only
	// then hide the group. That lets the level be authored as the finished article.
	CollectTargets();
	ApplyHiddenState();

	bSealed = false;
	TimeSinceCheck = 0.0f;
}

void UProximityBarrierComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSealed)
	{
		return;
	}

	TimeSinceCheck += DeltaTime;
	if (UpdateInterval > 0.0f && TimeSinceCheck < UpdateInterval)
	{
		return;
	}
	TimeSinceCheck = 0.0f;

	const float Signed = GetSignedDistance();

	if (bDrawOnScreenDebug && GEngine)
	{
		// The component itself is the key, so each barrier keeps its own line and the
		// message is simply replaced instead of piling up.
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.0f, FColor::Cyan,
			GetBarrierDebugString());
	}

	// Remember having been inside the threshold band at least once; that is what
	// bRequireInsideFirst means by "先靠近，再远离".
	if (FMath::Abs(Signed) < TriggerDistance)
	{
		bWasInside = true;
		return;
	}

	if (bRequireInsideFirst && !bWasInside)
	{
		return;
	}

	if (EvaluateTrigger(Signed))
	{
		SealNow();
	}
}

// ---------------------------------------------------------------------------
// distance
// ---------------------------------------------------------------------------

FVector UProximityBarrierComponent::GetWorldAxis() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}

	const FVector Axis = LocalAxis.GetSafeNormal();
	return Axis.IsNearlyZero() ? Owner->GetActorForwardVector() : Owner->GetActorRotation().RotateVector(Axis);
}

float UProximityBarrierComponent::GetSignedDistance() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0.0f;
	}

	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, PlayerIndex);
	if (!Pawn)
	{
		return 0.0f;
	}

	// Signed projection onto the axis: positive on the +axis side, negative behind.
	return FVector::DotProduct(Pawn->GetActorLocation() - Owner->GetActorLocation(), GetWorldAxis());
}

bool UProximityBarrierComponent::EvaluateTrigger(float SignedDistance) const
{
	if (bDrawDebug)
	{
		if (const UWorld* World = GetWorld())
		{
			const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
			const FVector Axis = GetWorldAxis();
			const float Sign = bTriggerOnNegativeSide ? -1.0f : 1.0f;
			const FVector Plane = Origin + Axis * (Sign * TriggerDistance);

			DrawDebugLine(World, Origin, Plane, FColor::Cyan, false, UpdateInterval > 0.0f ? UpdateInterval : 0.0f, 0, 2.0f);
			DrawDebugPoint(World, Plane, 16.0f, FColor::Cyan, false,
				UpdateInterval > 0.0f ? UpdateInterval : 0.0f);
			DrawDebugPoint(World, Origin, 10.0f, FColor::Yellow, false,
				UpdateInterval > 0.0f ? UpdateInterval : 0.0f);
		}
	}

	// Only the requested side counts: going far away on the wrong side never triggers,
	// which is the whole point of a SIGNED distance.
	return bTriggerOnNegativeSide
		? (SignedDistance <= -TriggerDistance)
		: (SignedDistance >= TriggerDistance);
}

// ---------------------------------------------------------------------------
// group
// ---------------------------------------------------------------------------

void UProximityBarrierComponent::CollectTargets()
{
	TargetActors.Reset();
	Primitives.Reset();
	AuthoredCollision.Reset();
	Lights.Reset();
	AuthoredLightVisible.Reset();
	AuthoredLightIntensity.Reset();

	TArray<AActor*> Candidates;
	if (AActor* Owner = GetOwner())
	{
		Candidates.Add(Owner);
	}

	for (const TObjectPtr<AActor>& Extra : ExtraActors)
	{
		if (Extra)
		{
			Candidates.AddUnique(Extra);
		}
	}

	if (bIncludeAttachedActors)
	{
		// Grows while iterating on purpose: children of children are included too.
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			TArray<AActor*> Attached;
			Candidates[Index]->GetAttachedActors(Attached, /*bResetArray=*/true,
				/*bRecursivelyIncludeAttachedActors=*/true);
			for (AActor* AttachedActor : Attached)
			{
				if (AttachedActor)
				{
					Candidates.AddUnique(AttachedActor);
				}
			}
		}
	}

	for (AActor* Actor : Candidates)
	{
		if (!Actor)
		{
			continue;
		}

		TargetActors.Add(Actor);

		TArray<UPrimitiveComponent*> ActorPrimitives;
		Actor->GetComponents<UPrimitiveComponent>(ActorPrimitives);
		for (UPrimitiveComponent* Primitive : ActorPrimitives)
		{
			if (!Primitive)
			{
				continue;
			}
			Primitives.Add(Primitive);
			AuthoredCollision.Add(Primitive->GetCollisionEnabled());
		}

		TArray<ULightComponent*> ActorLights;
		Actor->GetComponents<ULightComponent>(ActorLights);
		for (ULightComponent* Light : ActorLights)
		{
			if (!Light)
			{
				continue;
			}
			Lights.Add(Light);
			AuthoredLightVisible.Add(Light->IsVisible() ? 1 : 0);
			AuthoredLightIntensity.Add(Light->Intensity);
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Barrier] %s: group = %d actor(s), %d primitive(s), %d light(s)."),
		*GetNameSafe(GetOwner()), TargetActors.Num(), Primitives.Num(), Lights.Num());
}

void UProximityBarrierComponent::ApplyHiddenState()
{
	// Start exactly as specified: hidden and with no collision at all. The actor-level
	// flag is used so every component follows without losing its own settings.
	for (AActor* Actor : TargetActors)
	{
		if (!Actor)
		{
			continue;
		}
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
	}

	// Hiding an actor does not reliably switch a light off, so do it explicitly.
	for (ULightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->SetVisibility(false);
		}
	}
}

void UProximityBarrierComponent::ApplyShownState()
{
	for (AActor* Actor : TargetActors)
	{
		if (!Actor)
		{
			continue;
		}
		Actor->SetActorHiddenInGame(false);
		Actor->SetActorEnableCollision(true);
	}

	// Restore the authored collision, forcing a blocking state when the authored one
	// was NoCollision - the whole point is that the barrier cannot be crossed.
	for (int32 Index = 0; Index < Primitives.Num(); ++Index)
	{
		UPrimitiveComponent* Primitive = Primitives[Index];
		if (!Primitive)
		{
			continue;
		}

		ECollisionEnabled::Type Authored = AuthoredCollision.IsValidIndex(Index)
			? AuthoredCollision[Index].GetValue()
			: ECollisionEnabled::QueryAndPhysics;

		if (bForceCollisionWhenShown && Authored == ECollisionEnabled::NoCollision)
		{
			Authored = ECollisionEnabled::QueryAndPhysics;
		}

		Primitive->SetCollisionEnabled(Authored);
	}

	for (int32 Index = 0; Index < Lights.Num(); ++Index)
	{
		ULightComponent* Light = Lights[Index];
		if (!Light)
		{
			continue;
		}

		const bool bWasVisible = AuthoredLightVisible.IsValidIndex(Index)
			? AuthoredLightVisible[Index] != 0
			: true;

		Light->SetVisibility(bWasVisible);

		if (AuthoredLightIntensity.IsValidIndex(Index))
		{
			Light->SetIntensity(AuthoredLightIntensity[Index]);
		}
	}
}

// ---------------------------------------------------------------------------
// sealing
// ---------------------------------------------------------------------------

FString UProximityBarrierComponent::GetBarrierDebugString() const
{
	const float Signed = GetSignedDistance();
	const float AxisSide = bTriggerOnNegativeSide ? -1.0f : 1.0f;

	return FString::Printf(
		TEXT("Barrier %s | sealed=%s | signed=%.1f (along %s) | need %s%.1f | inside=%s | group %d actor / %d prim / %d light"),
		*GetNameSafe(GetOwner()),
		bSealed ? TEXT("YES") : TEXT("no"),
		Signed,
		*GetWorldAxis().ToCompactString(),
		bTriggerOnNegativeSide ? TEXT("<=") : TEXT(">="),
		AxisSide * TriggerDistance,
		bWasInside ? TEXT("yes") : TEXT("no"),
		TargetActors.Num(), Primitives.Num(), Lights.Num());
}

void UProximityBarrierComponent::SealNow()
{
	if (bSealed)
	{
		return;
	}

	bSealed = true;

	ApplyShownState();

	if (SealSound)
	{
		const FVector Location = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		UGameplayStatics::PlaySoundAtLocation(this, SealSound, Location);
	}

	UE_LOG(LogTemp, Log, TEXT("[Barrier] %s: sealed at signed distance %.1f - the group stays materialised."),
		*GetNameSafe(GetOwner()), GetSignedDistance());

	if (bDrawOnScreenDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this) + 1, 5.0f, FColor::Green,
			GetBarrierDebugString());
	}

	OnSealed.Broadcast(GetOwner(), UGameplayStatics::GetPlayerPawn(this, PlayerIndex));

	if (bStopPollingAfterTrigger)
	{
		SetComponentTickEnabled(false);
	}
}
