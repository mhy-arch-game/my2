// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovementAudioComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

namespace
{
	/** Turn a physical surface value into a readable name (honours renamed surfaces). */
	FName SurfaceToName(EPhysicalSurface Surface)
	{
		if (const UEnum* Enum = StaticEnum<EPhysicalSurface>())
		{
			return FName(*Enum->GetDisplayNameTextByValue(static_cast<int64>(Surface)).ToString());
		}
		return NAME_None;
	}
}

UMovementAudioComponent::UMovementAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UMovementAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
	if (!CachedCharacter)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MovementAudio] %s is not an ACharacter; movement audio disabled."),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	if (bAutoDetectJumpAndLand)
	{
		CachedCharacter->LandedDelegate.AddDynamic(this, &UMovementAudioComponent::HandleLanded);
		CachedCharacter->MovementModeChangedDelegate.AddDynamic(
			this, &UMovementAudioComponent::HandleMovementModeChanged);
	}

	LastLocation = CachedCharacter->GetActorLocation();
}

void UMovementAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedCharacter && bAutoDetectJumpAndLand)
	{
		CachedCharacter->LandedDelegate.RemoveDynamic(this, &UMovementAudioComponent::HandleLanded);
		CachedCharacter->MovementModeChangedDelegate.RemoveDynamic(
			this, &UMovementAudioComponent::HandleMovementModeChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void UMovementAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMovementState(DeltaTime);
}

void UMovementAudioComponent::UpdateMovementState(float DeltaTime)
{
	if (!CachedCharacter)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = CachedCharacter->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const FVector CurrentLocation = CachedCharacter->GetActorLocation();
	const float Speed2D = Movement->Velocity.Size2D();

	// In air tracking + peak fall speed (used to grade the landing sound).
	bInAir = Movement->IsFalling();
	if (bInAir)
	{
		PeakFallSpeed = FMath::Max(PeakFallSpeed, FMath::Abs(Movement->Velocity.Z));
	}

	// Walk <-> run.
	const bool bNewRunning = !bInAir && Speed2D > RunSpeedThreshold;
	if (bNewRunning != bRunning)
	{
		bRunning = bNewRunning;
		OnRunStateChanged.Broadcast(bRunning);
	}

	// Distance based footstep fallback (for testing before animations exist).
	if (bAutoFootstepByDistance && !bInAir && Movement->IsMovingOnGround())
	{
		DistanceSinceLastFootstep += FVector::Dist2D(CurrentLocation, LastLocation);
		if (DistanceSinceLastFootstep >= FootstepDistance)
		{
			DistanceSinceLastFootstep = 0.0f;
			PlayFootstep();
		}
	}
	else
	{
		DistanceSinceLastFootstep = 0.0f;
	}

	LastLocation = CurrentLocation;
}

// ---------------------------------------------------------------------------
// Triggers
// ---------------------------------------------------------------------------

void UMovementAudioComponent::PlayFootstep()
{
	PlayMovementAudioEvent(EMovementAudioEvent::Footstep);
}

void UMovementAudioComponent::PlayJump()
{
	PlayMovementAudioEvent(EMovementAudioEvent::Jump);
}

void UMovementAudioComponent::PlayLand()
{
	PlayMovementAudioEvent(EMovementAudioEvent::Land);
}

void UMovementAudioComponent::PlayMovementAudioEvent(EMovementAudioEvent Event)
{
	if (!CachedCharacter)
	{
		return;
	}

	const FVector Location = CachedCharacter->GetActorLocation();
	const EPhysicalSurface Surface = ResolveSurfaceAtLocation(Location);
	const FName SurfaceName = SurfaceToName(Surface);
	const FMovementAudioSet& Set = ResolveSet(Surface);

	// 1) Broadcast first: audio middleware hooks get the event even with no assets set.
	switch (Event)
	{
	case EMovementAudioEvent::Jump:
		OnJump.Broadcast(SurfaceName);
		break;

	case EMovementAudioEvent::Land:
		OnLand.Broadcast(SurfaceName, PeakFallSpeed);
		PeakFallSpeed = 0.0f;
		break;

	case EMovementAudioEvent::Footstep:
	default:
		OnFootstep.Broadcast(SurfaceName, bRunning);
		break;
	}

	// 2) Then play the optional asset, if one is assigned for this surface.
	PlaySoundAt(Set.GetSoundForEvent(Event), Location);
}

void UMovementAudioComponent::PlaySoundAt(const TSoftObjectPtr<USoundBase>& Sound, const FVector& WorldLocation)
{
	// No asset assigned yet -> nothing to play (the event was already broadcast).
	if (Sound.IsNull())
	{
		return;
	}

	USoundBase* LoadedSound = Sound.LoadSynchronous();
	if (!LoadedSound)
	{
		return;
	}

	// UE 5.7: the overload that takes an OwningActor also requires an explicit
	// rotation argument, so pass ZeroRotator to reach that overload.
	UGameplayStatics::PlaySoundAtLocation(
		this,
		LoadedSound,
		WorldLocation,
		FRotator::ZeroRotator,
		VolumeMultiplier,
		FMath::FRandRange(PitchMin, FMath::Max(PitchMin, PitchMax)),
		/*StartTime=*/0.0f,
		Attenuation,
		Concurrency,
		CachedCharacter);
}

// ---------------------------------------------------------------------------
// Surface resolution
// ---------------------------------------------------------------------------

const FMovementAudioSet& UMovementAudioComponent::ResolveSet(EPhysicalSurface Surface) const
{
	for (const FMovementAudioSet& Candidate : SurfaceSets)
	{
		if (Candidate.Surface == Surface)
		{
			return Candidate;
		}
	}
	return DefaultSet;
}

EPhysicalSurface UMovementAudioComponent::ResolveSurfaceAtLocation(const FVector& WorldLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return SurfaceType_Default;
	}

	const FVector Start = WorldLocation;
	const FVector End = WorldLocation - FVector(0.0f, 0.0f, SurfaceTraceDistance);

	FCollisionQueryParams Params(FName(TEXT("MovementAudioSurface")), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(CachedCharacter);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, SurfaceTraceChannel, Params))
	{
		return SurfaceType_Default;
	}

	// Physical material of the hit body (stable across engine versions; FHitResult has
	// no per-triangle PhysMaterial member on all versions).
	UPhysicalMaterial* PhysMat = nullptr;
	if (UPrimitiveComponent* HitComponent = Hit.GetComponent())
	{
		if (FBodyInstance* Body = HitComponent->GetBodyInstance())
		{
			PhysMat = Body->GetSimplePhysicalMaterial();
		}
	}

	return PhysMat ? PhysMat->SurfaceType.GetValue() : SurfaceType_Default;
}

FName UMovementAudioComponent::GetCurrentSurfaceName() const
{
	if (!CachedCharacter)
	{
		return NAME_None;
	}
	return GetSurfaceNameAtLocation(CachedCharacter->GetActorLocation());
}

FName UMovementAudioComponent::GetSurfaceNameAtLocation(const FVector& WorldLocation) const
{
	return SurfaceToName(ResolveSurfaceAtLocation(WorldLocation));
}

// ---------------------------------------------------------------------------
// Auto detection
// ---------------------------------------------------------------------------

void UMovementAudioComponent::HandleLanded(const FHitResult& Hit)
{
	if (!bAutoDetectJumpAndLand)
	{
		return;
	}

	// Land sound carries the peak fall speed so audio can grade light vs heavy.
	PlayMovementAudioEvent(EMovementAudioEvent::Land);
}

void UMovementAudioComponent::HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode,
	uint8 PreviousCustomMode)
{
	if (!bAutoDetectJumpAndLand || !Character)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	// Only entering falling while still moving upwards counts as a jump;
	// merely stepping off a ledge is a fall, not a jump.
	if (PrevMovementMode != MOVE_Falling &&
		Movement->MovementMode == MOVE_Falling &&
		Movement->Velocity.Z > 0.0f)
	{
		PlayMovementAudioEvent(EMovementAudioEvent::Jump);
	}
}
