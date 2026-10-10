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

	// --- 配对自检：把"实际用到的时空"和它的来源直接说出来 --------------------
	// 这一项配错会让配对方向和垂直位移符号一起错，所以不值得让设计者靠猜。
	const bool bEraFromComponent = bAutoDetectEra && EraComponent;

	UE_LOG(LogTemp, Log,
		TEXT("[TimeEraPortal] %s: 所属时空 = %s（来源：%s）；配对方式 = %s；目标时空 = %s。"),
		*Owner->GetName(),
		GetOwnerEra() == ETimeEra::Ancient ? TEXT("古 Ancient") : TEXT("今 Modern"),
		bEraFromComponent ? TEXT("TimeEraComponent")
			: (bAutoDetectEra ? TEXT("OwnerEra（回退：没找到 TimeEraComponent）") : TEXT("OwnerEra（bAutoDetectEra 已关）")),
		TargetMode == ETimeEraPortalTargetMode::VerticalOffset ? TEXT("Vertical Offset") : TEXT("Counterpart Object"),
		GetCounterpartEra() == ETimeEra::Ancient ? TEXT("古 Ancient") : TEXT("今 Modern"));

	if (bAutoDetectEra && !EraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TimeEraPortal] %s: bAutoDetectEra 开着，但对象上没有 TimeEraComponent，已回退到 OwnerEra。")
			TEXT("若希望它随时空显隐，请补一个 Time Era 组件；否则请确认这里的 OwnerEra 是对的。"),
			*Owner->GetName());
	}

	if (bAutoDetectEra && EraComponent && EraComponent->Era != OwnerEra)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[TimeEraPortal] %s: OwnerEra(%s) 与 TimeEraComponent::Era(%s) 不一致，实际以 TimeEraComponent 为准。")
			TEXT("把两者改成一致，或关掉 bAutoDetectEra。"),
			*Owner->GetName(),
			OwnerEra == ETimeEra::Ancient ? TEXT("古") : TEXT("今"),
			EraComponent->Era == ETimeEra::Ancient ? TEXT("古") : TEXT("今"));
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

bool UTimeEraPortalComponent::ResolveDestination(FVector& OutLocation, FRotator& OutRotation,
	ETimeEra& OutEra, AActor*& OutCounterpart, FText& OutRefusalReason) const
{
	OutEra = GetCounterpartEra();
	OutCounterpart = nullptr;

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutRefusalReason = LOCTEXT("NoOwner", "传送装置没有 owner。");
		return false;
	}

	if (TargetMode == ETimeEraPortalTargetMode::VerticalOffset)
	{
		// 两个时空只差 Z：落点由自己的位置直接推出来，不需要对应物。
		// 符号按自己所属时空取（VerticalOffset 定义 = 今 − 古），所以两半填同一个值即可。
		const float SignedOffset = (GetOwnerEra() == ETimeEra::Ancient) ? VerticalOffset : -VerticalOffset;

		OutLocation = Owner->GetActorLocation() + FVector(0.0f, 0.0f, SignedOffset);
		OutRotation = Owner->GetActorRotation();
		return true;
	}

	AActor* Counterpart = ResolveCounterpartInternal(OutRefusalReason);
	if (!Counterpart)
	{
		return false;
	}

	OutLocation = Counterpart->GetActorLocation();
	OutRotation = Counterpart->GetActorRotation();
	OutCounterpart = Counterpart;
	return true;
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
	FVector Destination = FVector::ZeroVector;
	FRotator DestinationRotation = FRotator::ZeroRotator;
	ETimeEra TargetEra = GetCounterpartEra();
	AActor* Counterpart = nullptr;

	if (!ResolveDestination(Destination, DestinationRotation, TargetEra, Counterpart, RefusalReason))
	{
		OnPortalRefused.Broadcast(this, RefusalReason);
		return false;
	}

	// 1. switch era FIRST: listeners (the player's UTimeShiftTravelComponent) relocate
	//    by the layout mapping, and step 3 then overrides that with the exact
	//    destination. Doing it the other way round would undo step 3.
	UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
	if (bSwitchEra && Subsystem && Subsystem->GetEra() != TargetEra)
	{
		if (!Subsystem->CanSwitchEra())
		{
			OnPortalRefused.Broadcast(this, LOCTEXT("EraCooldown", "时空切换仍在冷却，暂时无法传送。"));
			return false;
		}
		Subsystem->SetEra(TargetEra);
	}

	// 2. stop the leftover velocity so the traveller does not keep flying on arrival.
	if (ACharacter* Character = Cast<ACharacter>(TravelerActor))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	// 3. place at the resolved destination (works for both target modes).
	const FVector Goal = ComputeArrivalLocation(Destination, DestinationRotation, TravelerActor);
	TravelerActor->SetActorLocation(Goal, false, nullptr, ETeleportType::TeleportPhysics);

	if (bMatchCounterpartYaw)
	{
		const FRotator ArrivalRotation(0.0f, DestinationRotation.Yaw, 0.0f);
		TravelerActor->SetActorRotation(ArrivalRotation);
		if (APawn* Pawn = Cast<APawn>(TravelerActor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(ArrivalRotation);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TimeEraPortal] %s -> %s (%s, %s, %.0f/%.0f/%.0f)"),
		*GetOwner()->GetName(),
		Counterpart ? *Counterpart->GetName() : TEXT("<仅 Z 不同的另一半>"),
		TargetEra == ETimeEra::Ancient ? TEXT("古") : TEXT("今"),
		TargetMode == ETimeEraPortalTargetMode::VerticalOffset ? TEXT("VerticalOffset") : TEXT("Counterpart"),
		Goal.X, Goal.Y, Goal.Z);

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
	FVector Destination = FVector::ZeroVector;
	FRotator DestinationRotation = FRotator::ZeroRotator;
	ETimeEra DestinationEra = GetCounterpartEra();
	AActor* Counterpart = nullptr;

	if (!ResolveDestination(Destination, DestinationRotation, DestinationEra, Counterpart, Reason))
	{
		return false;
	}

	if (bSwitchEra)
	{
		const UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
		if (Subsystem && Subsystem->GetEra() != DestinationEra && !Subsystem->CanSwitchEra())
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
	if (TargetMode == ETimeEraPortalTargetMode::VerticalOffset)
	{
		// 这种模式没有"对应对象"：落点由 VerticalOffset 推导（见 ResolveDestination）。
		OutRefusalReason = LOCTEXT("VerticalNoCounterpart", "垂直位移模式没有对应对象（落点由自己位置 + Z 位移推导）。");
		return nullptr;
	}

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

FString UTimeEraPortalComponent::GetPortalDebugString() const
{
	FText Reason;
	FVector Destination = FVector::ZeroVector;
	FRotator DestinationRotation = FRotator::ZeroRotator;
	ETimeEra DestinationEra = GetCounterpartEra();
	AActor* Counterpart = nullptr;

	const bool bResolved = ResolveDestination(Destination, DestinationRotation, DestinationEra, Counterpart, Reason);

	const FString EraSource = (bAutoDetectEra && EraComponent)
		? TEXT("TimeEraComponent")
		: (bAutoDetectEra ? TEXT("OwnerEra(fallback)") : TEXT("OwnerEra"));

	return FString::Printf(
		TEXT("Portal %s | mode=%s | ownerEra=%s (%s) | targetEra=%s | verticalOffset=%.1f | dest=%s | counterpart=%s | locked=%s"),
		*GetNameSafe(GetOwner()),
		TargetMode == ETimeEraPortalTargetMode::VerticalOffset ? TEXT("VerticalOffset") : TEXT("Counterpart"),
		GetOwnerEra() == ETimeEra::Ancient ? TEXT("Ancient") : TEXT("Modern"),
		*EraSource,
		GetCounterpartEra() == ETimeEra::Ancient ? TEXT("Ancient") : TEXT("Modern"),
		VerticalOffset,
		bResolved ? *Destination.ToCompactString() : *FString::Printf(TEXT("<解析失败: %s>"), *Reason.ToString()),
		Counterpart ? *Counterpart->GetName() : TEXT("none"),
		IsLocked() ? TEXT("yes") : TEXT("no"));
}

// ---------------------------------------------------------------------------
// placement
// ---------------------------------------------------------------------------

FVector UTimeEraPortalComponent::ComputeArrivalLocation(const FVector& BaseLocation,
	const FRotator& BaseRotation, const AActor* Traveler) const
{
	// TeleportOffset is expressed in the destination's own space - for a vertical pair
	// that is the same object one era away, so its rotation is still the right frame.
	FVector Goal = BaseLocation + BaseRotation.RotateVector(TeleportOffset);

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
