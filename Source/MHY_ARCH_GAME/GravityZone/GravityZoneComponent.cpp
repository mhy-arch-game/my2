// Copyright Epic Games, Inc. All Rights Reserved.

#include "GravityZoneComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

UGravityZoneComponent::UGravityZoneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// The zone is a pure trigger: never block movement.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetHiddenInGame(true);

	// InitBoxExtent (NOT SetBoxExtent): SetBoxExtent calls UpdateBodySetup ->
	// NewObject with an empty name, which is fatal inside a constructor.
	InitBoxExtent(FVector(400.0f, 400.0f, 200.0f));
}

void UGravityZoneComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddDynamic(this, &UGravityZoneComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UGravityZoneComponent::HandleEndOverlap);

	// Catch anything already inside when the zone starts (e.g. it spawns around
	// the player, or a level starts with the player standing in it).
	TArray<AActor*> Overlapping;
	GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		ApplyTo(Actor);
	}
}

void UGravityZoneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnComponentBeginOverlap.RemoveDynamic(this, &UGravityZoneComponent::HandleBeginOverlap);
	OnComponentEndOverlap.RemoveDynamic(this, &UGravityZoneComponent::HandleEndOverlap);

	// Never leave a character floating: restore everyone still tracked.
	TMap<TWeakObjectPtr<AActor>, FGravityZoneEntry> Pending = MoveTemp(Tracked);
	Tracked.Empty();

	for (const TPair<TWeakObjectPtr<AActor>, FGravityZoneEntry>& Pair : Pending)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			RestoreFrom(Actor, Pair.Value);
		}
	}

	Super::EndPlay(EndPlayReason);
}

UCharacterMovementComponent* UGravityZoneComponent::GetMovement(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UCharacterMovementComponent>() : nullptr;
}

void UGravityZoneComponent::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ApplyTo(OtherActor);
}

void UGravityZoneComponent::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	const FGravityZoneEntry* Found = Tracked.Find(OtherActor);
	if (!Found)
	{
		return;
	}

	const FGravityZoneEntry Saved = *Found;
	Tracked.Remove(OtherActor);

	RestoreFrom(OtherActor, Saved);

	if (UCharacterMovementComponent* Movement = GetMovement(OtherActor))
	{
		OnActorExited.Broadcast(OtherActor, Movement->GravityScale);
	}
}

void UGravityZoneComponent::ApplyTo(AActor* Actor)
{
	if (!Actor || Tracked.Contains(Actor))
	{
		return;
	}

	if (bAffectPawnsOnly && !Actor->IsA<APawn>())
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetMovement(Actor);
	if (!Movement)
	{
		return;
	}

	FGravityZoneEntry Entry;
	Entry.GravityScale = Movement->GravityScale;
	Entry.JumpZVelocity = Movement->JumpZVelocity;
	Tracked.Add(Actor, Entry);

	Movement->GravityScale = Entry.GravityScale * GravityScaleInside;

	if (TargetJumpHeight > 0.0f)
	{
		// Absolute height request: solve h = v^2 / (2g) for v using the gravity this
		// character actually feels inside the zone (volume gravity * applied scale).
		// NOTE: UCharacterMovementComponent::GetGravityZ() already multiplies by
		// GravityScale in UE5 (return Super::GetGravityZ() * GravityScale), so this is
		// the final gravity the character feels - do NOT scale it again here.
		const float AppliedGravityZ = FMath::Abs(Movement->GetGravityZ());
		if (AppliedGravityZ > 1e-3f)
		{
			Movement->JumpZVelocity = FMath::Sqrt(2.0f * AppliedGravityZ * TargetJumpHeight);
		}
	}
	else if (bScaleJumpVelocity)
	{
		// h = v^2 / (2g): scaling v by sqrt(k) keeps the height while g scales by k.
		Movement->JumpZVelocity = Entry.JumpZVelocity * FMath::Sqrt(FMath::Max(GravityScaleInside, 0.0f));
	}

	OnActorEntered.Broadcast(Actor, Movement->GravityScale);
}

void UGravityZoneComponent::RestoreFrom(AActor* Actor, const FGravityZoneEntry& Entry)
{
	UCharacterMovementComponent* Movement = GetMovement(Actor);
	if (!Movement)
	{
		return;
	}

	Movement->GravityScale = Entry.GravityScale;

	if (bScaleJumpVelocity || TargetJumpHeight > 0.0f)
	{
		Movement->JumpZVelocity = Entry.JumpZVelocity;
	}
}

bool UGravityZoneComponent::IsActorInside(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FGravityZoneEntry>& Pair : Tracked)
	{
		if (Pair.Key.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Preserving the jump height while changing how fast the arc is
// ---------------------------------------------------------------------------

void UGravityZoneComponent::ConfigurePreservingJumpHeight(float GravityMultiplier, bool bAlsoScaleJumpVelocity)
{
	GravityScaleInside = FMath::Max(GravityMultiplier, 0.0f);
	bScaleJumpVelocity = bAlsoScaleJumpVelocity;
}

float UGravityZoneComponent::GetJumpHeightScale() const
{
	// h = v^2 / (2g). With v * sqrt(k) and g * k the height is unchanged; scaling
	// only g divides the height by k.
	if (bScaleJumpVelocity || TargetJumpHeight > 0.0f)
	{
		// TargetJumpHeight pins the height by construction, so the ratio is 1 as well.
		return 1.0f;
	}
	return GravityScaleInside > 1e-4f ? 1.0f / GravityScaleInside : 1.0f;
}

float UGravityZoneComponent::GetAirTimeScale() const
{
	// Rise time t = v / g (fall time behaves the same way).
	const float k = GravityScaleInside;
	if (k <= 1e-4f)
	{
		return 1.0f;
	}
	const float VelocityScale = bScaleJumpVelocity ? FMath::Sqrt(k) : 1.0f;
	return VelocityScale / k;
}
