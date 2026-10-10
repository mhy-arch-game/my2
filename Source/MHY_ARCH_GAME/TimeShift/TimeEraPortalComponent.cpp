// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeEraPortalComponent.h"

#include "InteractableComponent.h"
#include "TimeEraComponent.h"
#include "TimeShiftSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "TimeEraPortal"

UTimeEraPortalComponent::UTimeEraPortalComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void UTimeEraPortalComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The owner's era is normally authored with a UTimeEraComponent (it also gates
	// visibility / collision of the object per era).
	EraComponent = Owner->FindComponentByClass<UTimeEraComponent>();

	// --- hook the SHARED interact interface --------------------------------
	if (bAutoUseInteractableOnOwner)
	{
		BoundInteractable = Owner->FindComponentByClass<UInteractableComponent>();
		if (!BoundInteractable)
		{
			// Nothing on the object yet: create one so a plain actor becomes
			// interactable without touching its class or its Blueprint graph.
			BoundInteractable = NewObject<UInteractableComponent>(
				Owner, UInteractableComponent::StaticClass(), TEXT("TimeEraPortalInteractable"), RF_Transient);
			if (BoundInteractable)
			{
				BoundInteractable->RegisterComponent();
				UE_LOG(LogTemp, Log, TEXT("[TimeEraPortal] %s: 自动创建 InteractableComponent"), *Owner->GetName());
			}
		}

		if (BoundInteractable)
		{
			if (!InteractionPrompt.IsEmpty())
			{
				BoundInteractable->InteractionPrompt = InteractionPrompt;
			}

			if (BoundInteractable->bUseBuiltInToggle)
			{
				if (bSuppressBuiltInToggle)
				{
					BoundInteractable->bUseBuiltInToggle = false;
					UE_LOG(LogTemp, Log, TEXT("[TimeEraPortal] %s: 已关闭 InteractableComponent 的内置开关（只做时空传送）"), *Owner->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[TimeEraPortal] %s: InteractableComponent 的内置开关与本组件会同时生效（对象既开合又传送）。只想要传送就把 bSuppressBuiltInToggle 打开。"),
						*Owner->GetName());
				}
			}

			BoundInteractable->OnInteractRequested.AddDynamic(this, &UTimeEraPortalComponent::HandleInteractRequested);
		}
	}

	// --- register as era anchor so the generic switch pairs the two objects too --
	if (bRegisterAsAnchor && !CounterpartId.IsNone())
	{
		if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
		{
			Subsystem->RegisterAnchor(Owner, CounterpartId, GetOwnerEra());
			bAnchorRegistered = true;
			RegisteredAnchorId = CounterpartId;
		}
	}
}

void UTimeEraPortalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundInteractable)
	{
		BoundInteractable->OnInteractRequested.RemoveDynamic(this, &UTimeEraPortalComponent::HandleInteractRequested);
	}

	if (bAnchorRegistered)
	{
		if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
		{
			Subsystem->UnregisterAnchor(GetOwner());
		}
		bAnchorRegistered = false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LockTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// interaction
// ---------------------------------------------------------------------------

void UTimeEraPortalComponent::HandleInteractRequested(AActor* Interactor, AActor* Interactable)
{
	TryUsePortal(Interactor);
}

bool UTimeEraPortalComponent::TryUsePortal(AActor* Traveler)
{
	AActor* TravelerActor = ResolveTraveler(Traveler);
	if (!TravelerActor)
	{
		OnPortalRefused.Broadcast(this, LOCTEXT("NoTraveler", "没有可传送的主控角色。"));
		return false;
	}

	if (IsLocked())
	{
		OnPortalRefused.Broadcast(this, LOCTEXT("Locked", "传送装置刚刚使用过，还在冷却。"));
		return false;
	}

	FText RefusalReason;
	AActor* Counterpart = ResolveCounterpartInternal(RefusalReason);
	if (!Counterpart)
	{
		OnPortalRefused.Broadcast(this, RefusalReason);
		return false;
	}

	// 1. switch to the counterpart's era FIRST: listeners (the player's
	//    UTimeShiftTravelComponent) relocate by the layout mapping, and step 2
	//    then overrides that with the exact counterpart position.
	UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
	const ETimeEra TargetEra = GetCounterpartEra();
	if (bSwitchEra && Subsystem && Subsystem->GetEra() != TargetEra)
	{
		if (!Subsystem->CanSwitchEra())
		{
			OnPortalRefused.Broadcast(this, LOCTEXT("EraCooldown", "时空切换仍在冷却，暂时无法传送。"));
			return false;
		}
		Subsystem->SetEra(TargetEra);
	}

	// 2. explicit arrival at the counterpart.
	if (ACharacter* Character = Cast<ACharacter>(TravelerActor))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			// Kill the leftover velocity so the traveller does not keep flying on arrival.
			Movement->StopMovementImmediately();
		}
	}

	const FVector Goal = ComputeArrivalLocation(Counterpart, TravelerActor);
	TravelerActor->SetActorLocation(Goal, false, nullptr, ETeleportType::TeleportPhysics);

	if (bMatchCounterpartYaw)
	{
		const FRotator ArrivalRotation(0.0f, Counterpart->GetActorRotation().Yaw, 0.0f);
		TravelerActor->SetActorRotation(ArrivalRotation);
		if (APawn* Pawn = Cast<APawn>(TravelerActor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(ArrivalRotation);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TimeEraPortal] %s -> %s (%s)"),
		*GetOwner()->GetName(), *Counterpart->GetName(),
		TargetEra == ETimeEra::Ancient ? TEXT("古") : TEXT("今"));

	ArmLock();
	OnPortalUsed.Broadcast(TravelerActor, GetOwner(), Counterpart);
	return true;
}

bool UTimeEraPortalComponent::CanUsePortal(AActor* Traveler) const
{
	if (IsLocked() || !ResolveTraveler(Traveler))
	{
		return false;
	}

	FText Reason;
	if (!ResolveCounterpartInternal(Reason))
	{
		return false;
	}

	if (bSwitchEra)
	{
		const UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
		if (Subsystem && Subsystem->GetEra() != GetCounterpartEra() && !Subsystem->CanSwitchEra())
		{
			return false;
		}
	}

	return true;
}

bool UTimeEraPortalComponent::IsLocked() const
{
	return LockEndTime > 0.0 && FPlatformTime::Seconds() < LockEndTime;
}

// ---------------------------------------------------------------------------
// counterpart resolution
// ---------------------------------------------------------------------------

AActor* UTimeEraPortalComponent::ResolveCounterpart() const
{
	FText Unused;
	return ResolveCounterpartInternal(Unused);
}

AActor* UTimeEraPortalComponent::ResolveCounterpartInternal(FText& OutRefusalReason) const
{
	const AActor* Owner = GetOwner();
	const ETimeEra WantedEra = GetCounterpartEra();

	// 1. the explicitly designated object wins.
	AActor* Counterpart = CounterpartActor.Get();

	// 2. same CounterpartId, resolved through the era-anchor registry.
	if (!Counterpart && !CounterpartId.IsNone())
	{
		if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
		{
			Counterpart = Subsystem->FindAnchor(CounterpartId, WantedEra);
		}
	}

	// 3. same CounterpartId, found among the sibling portals of the other era.
	if (!Counterpart && !CounterpartId.IsNone())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Candidate = *It;
				if (Candidate == Owner)
				{
					continue;
				}
				const UTimeEraPortalComponent* OtherPortal = Candidate->FindComponentByClass<UTimeEraPortalComponent>();
				if (OtherPortal && OtherPortal->CounterpartId == CounterpartId && OtherPortal->GetOwnerEra() == WantedEra)
				{
					Counterpart = Candidate;
					break;
				}
			}
		}
	}

	// 4. last resort: the nearest era anchor of the opposite era.
	if (!Counterpart)
	{
		if (UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this))
		{
			const FVector From = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
			Counterpart = Subsystem->FindNearestAnchor(WantedEra, From);
		}
	}

	if (!Counterpart)
	{
		OutRefusalReason = LOCTEXT("NoCounterpart", "对立时空里找不到对应的对象（请指定 CounterpartActor 或 CounterpartId）。");
		return nullptr;
	}

	if (bRequireCounterpartInOtherEra)
	{
		ETimeEra CounterpartEra;
		if (TryGetActorEra(Counterpart, CounterpartEra) && CounterpartEra == GetOwnerEra())
		{
			OutRefusalReason = LOCTEXT("SameEra", "指定的对应对象与本体处在同一时空，无法传送。");
			return nullptr;
		}
	}

	return Counterpart;
}

bool UTimeEraPortalComponent::TryGetActorEra(const AActor* Actor, ETimeEra& OutEra) const
{
	if (!Actor)
	{
		return false;
	}

	// A sibling portal knows its own era best.
	if (const UTimeEraPortalComponent* OtherPortal = Actor->FindComponentByClass<UTimeEraPortalComponent>())
	{
		OutEra = OtherPortal->GetOwnerEra();
		return true;
	}

	if (const UTimeEraComponent* OtherEra = Actor->FindComponentByClass<UTimeEraComponent>())
	{
		OutEra = OtherEra->Era;
		return true;
	}

	return false;
}

ETimeEra UTimeEraPortalComponent::GetOwnerEra() const
{
	if (bAutoDetectEra && EraComponent)
	{
		return EraComponent->Era;
	}
	return OwnerEra;
}

ETimeEra UTimeEraPortalComponent::GetCounterpartEra() const
{
	return GetOppositeEra(GetOwnerEra());
}

// ---------------------------------------------------------------------------
// placement
// ---------------------------------------------------------------------------

FVector UTimeEraPortalComponent::ComputeArrivalLocation(const AActor* Counterpart, const AActor* Traveler) const
{
	if (!Counterpart)
	{
		return FVector::ZeroVector;
	}

	// TeleportOffset is expressed in the counterpart's own space.
	FVector Goal = Counterpart->GetActorLocation()
		+ Counterpart->GetActorRotation().RotateVector(TeleportOffset);

	if (!bPlaceOnGround || GroundTraceDistance <= 0.0f)
	{
		return Goal;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Goal;
	}

	const FVector Start = Goal + FVector(0.0f, 0.0f, GroundTraceDistance);
	const FVector End = Goal - FVector(0.0f, 0.0f, GroundTraceDistance);

	FCollisionQueryParams Params(TEXT("TimeEraPortalGround"), false, GetOwner());
	Params.AddIgnoredActor(Traveler);
	Params.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		Goal.Z = Hit.ImpactPoint.Z + GetTravelerHalfHeight(Traveler) + GroundClearance;
	}

	return Goal;
}

float UTimeEraPortalComponent::GetTravelerHalfHeight(const AActor* Traveler) const
{
	if (const ACharacter* Character = Cast<ACharacter>(Traveler))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	if (const APawn* Pawn = Cast<APawn>(Traveler))
	{
		if (const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
		{
			return Root->Bounds.BoxExtent.Z;
		}
	}

	return 0.0f;
}

AActor* UTimeEraPortalComponent::ResolveTraveler(AActor* Interactor) const
{
	if (AController* Controller = Cast<AController>(Interactor))
	{
		return Controller->GetPawn();
	}
	return Interactor;
}

// ---------------------------------------------------------------------------
// per-portal lock
// ---------------------------------------------------------------------------

void UTimeEraPortalComponent::ArmLock()
{
	if (PortalCooldown <= 0.0f)
	{
		return;
	}

	LockEndTime = FPlatformTime::Seconds() + PortalCooldown;

	// Hide the prompt while the portal cannot be used, so the player never sees a
	// focus that would just be refused.
	if (bDisableInteractableWhileLocked && BoundInteractable)
	{
		BoundInteractable->SetEnabled(false);
		bInteractableDisabledByUs = true;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(LockTimerHandle, this,
			&UTimeEraPortalComponent::HandleLockElapsed, PortalCooldown, false);
	}
}

void UTimeEraPortalComponent::HandleLockElapsed()
{
	LockEndTime = 0.0;

	if (bInteractableDisabledByUs && BoundInteractable)
	{
		BoundInteractable->SetEnabled(true);
		bInteractableDisabledByUs = false;
	}
}

#undef LOCTEXT_NAMESPACE
