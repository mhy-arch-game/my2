// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeEraPortalComponent.h"

#include "InteractableComponent.h"
#include "TimeEraComponent.h"
#include "TeleportTransitionInterface.h"
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
		World->GetTimerManager().ClearTimer(TransitionTimerHandle);
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
	// Never restart mid-transition: the player could press E again while the
	// transition is fading out.
	if (bTransitionInProgress)
	{
		OnPortalRefused.Broadcast(this, LOCTEXT("Transitioning", "上一次传送的过场还没结束。"));
		return false;
	}

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

	// Era availability is checked up front, so a refusal never starts a transition
	// (that would fade the screen out for nothing).
	UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
	if (bSwitchEra && Subsystem && Subsystem->GetEra() != TargetEra && !Subsystem->CanSwitchEra())
	{
		OnPortalRefused.Broadcast(this, LOCTEXT("EraCooldown", "时空切换仍在冷却，暂时无法传送。"));
		return false;
	}

	// --- remember everything, then notify the transition ------------------
	PendingDestination = Destination;
	PendingDestinationRotation = DestinationRotation;
	PendingTargetEra = TargetEra;
	PendingCounterpart = Counterpart;

	TransitionContext = FTeleportTransitionContext();
	TransitionContext.Traveler = TravelerActor;
	TransitionContext.Portal = GetOwner();
	TransitionContext.Counterpart = Counterpart;
	TransitionContext.FromLocation = TravelerActor->GetActorLocation();
	TransitionContext.ToLocation = Destination;
	TransitionContext.FromEra = Subsystem ? Subsystem->GetEra() : GetOwnerEra();
	TransitionContext.ToEra = TargetEra;
	TransitionContext.Duration = TransitionDelay;

	bTransitionInProgress = true;

	// Begin fires BEFORE anything moves, which is what a fade-out needs.
	DispatchTransition(/*bBegin=*/true);
	OnPortalUsed.Broadcast(TravelerActor, GetOwner(), Counterpart);

	if (TransitionDelay > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TransitionTimerHandle, this,
				&UTimeEraPortalComponent::FinishTeleport, TransitionDelay, false);
			return true;
		}
	}

	// No transition (or no world): teleport immediately, exactly as before.
	FinishTeleport();
	return true;
}

void UTimeEraPortalComponent::FinishTeleport()
{
	bTransitionInProgress = false;

	AActor* TravelerActor = TransitionContext.Traveler;
	if (!TravelerActor)
	{
		DispatchTransition(/*bBegin=*/false);
		return;
	}

	// 1. switch era FIRST: listeners (the player's UTimeShiftTravelComponent) relocate
	//    by the layout mapping, and step 3 then overrides that with the exact
	//    destination. Doing it the other way round would undo step 3.
	//    Re-checked here because the cooldown may have started during the delay.
	UTimeShiftSubsystem* Subsystem = UTimeShiftSubsystem::Get(this);
	if (bSwitchEra && Subsystem && Subsystem->GetEra() != PendingTargetEra)
	{
		if (!Subsystem->CanSwitchEra())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[TimeEraPortal] %s: 过场结束时时空切换仍在冷却，本次传送取消（已广播过场结束收尾）。"),
				*GetNameSafe(GetOwner()));
			DispatchTransition(/*bBegin=*/false);
			return;
		}
		Subsystem->SetEra(PendingTargetEra);
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
	const FVector Goal = ComputeArrivalLocation(PendingDestination, PendingDestinationRotation, TravelerActor);
	TravelerActor->SetActorLocation(Goal, false, nullptr, ETeleportType::TeleportPhysics);

	// The context carries the FINAL landing point (ground snapping applied).
	TransitionContext.ToLocation = Goal;

	if (bMatchCounterpartYaw)
	{
		const FRotator ArrivalRotation(0.0f, PendingDestinationRotation.Yaw, 0.0f);
		TravelerActor->SetActorRotation(ArrivalRotation);
		if (APawn* Pawn = Cast<APawn>(TravelerActor))
		{
			if (AController* Controller = Pawn->GetController())
			{
				Controller->SetControlRotation(ArrivalRotation);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[TimeEraPortal] %s -> %s (%s, %s, %.0f/%.0f/%.0f, 过场 %.2fs)"),
		*GetOwner()->GetName(),
		PendingCounterpart ? *PendingCounterpart->GetName() : TEXT("<仅 Z 不同的另一半>"),
		PendingTargetEra == ETimeEra::Ancient ? TEXT("古") : TEXT("今"),
		TargetMode == ETimeEraPortalTargetMode::VerticalOffset ? TEXT("VerticalOffset") : TEXT("Counterpart"),
		Goal.X, Goal.Y, Goal.Z,
		TransitionDelay);

	ArmLock();
	DispatchTransition(/*bBegin=*/false);
}

void UTimeEraPortalComponent::DispatchTransition(bool bBegin)
{
	// 1) Blueprint delegates on this component (bind in a level/character Blueprint).
	if (bBegin)
	{
		OnTransitionBegin.Broadcast(TransitionContext);
	}
	else
	{
		OnTransitionEnd.Broadcast(TransitionContext);
	}

	// 2) Interface listeners anywhere in the world: a transition director / UI manager
	//    does not need to know which portal fired.
	if (!bDispatchTransitionToInterfaceListeners)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return;
	}

	int32 Count = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Listener = *It;
		if (!Listener || Listener == Owner)
		{
			continue;
		}

		if (!Listener->GetClass()->ImplementsInterface(UTeleportTransitionInterface::StaticClass()))
		{
			continue;
		}

		if (!ITeleportTransitionInterface::Execute_CanReceiveTeleportTransition(
			Listener, TransitionContext.Traveler))
		{
			continue;
		}

		if (bBegin)
		{
			ITeleportTransitionInterface::Execute_OnTeleportTransitionBegin(Listener, TransitionContext);
		}
		else
		{
			ITeleportTransitionInterface::Execute_OnTeleportTransitionEnd(Listener, TransitionContext);
		}
		++Count;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[TimeEraPortal] %s: 过场%s已派发给 %d 个接口监听者。"),
		*GetNameSafe(Owner), bBegin ? TEXT("开始") : TEXT("结束"), Count);
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

	// Keep the capsule OUT of the destination device. The mesh is usually tall and thin
	// (21x21x100 here) while the capsule is 34 in radius, so dropping the traveller on the
	// device origin swallows the whole device: in first person it disappears and the
	// interaction breaks with it. Push the arrival sideways along the device's own +X by
	// (device radius + capsule radius + clearance) so it lands on the ground beside it.
	if (bArriveClearOfDevice)
	{
		const AActor* Device = PendingCounterpart && IsValid(PendingCounterpart)
			? PendingCounterpart.Get()
			: GetOwner();
		const float Escape = GetDeviceHorizontalRadius(Device)
			+ GetTravelerRadius(Traveler)
			+ FMath::Max(ArrivalClearance, 0.0f);
		if (Escape > 0.0f)
		{
			Goal += BaseRotation.RotateVector(FVector(Escape, 0.0f, 0.0f));
		}
	}

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

	// 传送装置不是地面：装置网格挡住 Visibility，若不忽略，从"落点 + 距离"往下打的射线
	// 会先停在【目标装置自己的顶面】上，角色就被放到"装置顶上"而不是装置处。
	// 实测 kongjianchuansuoqi 的装置高 100cm ⇒ 每次传送都比配对位置高 100cm。
	// VerticalOffset 模式没有 Counterpart 可以忽略，所以按"所有传送装置"整体忽略。
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (AActor* Other = *It)
		{
			if (Other->FindComponentByClass<UTimeEraPortalComponent>())
			{
				Params.AddIgnoredActor(Other);
			}
		}
	}

	const float HalfHeight = GetTravelerHalfHeight(Traveler);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// 只允许"向下贴地"：命中的面若在预定落点之上（天花板 / 其它物体），
		// 不能把角色向上抬过"脚底正好落在预定平面"这个上限。
		const float SnappedZ = Hit.ImpactPoint.Z + HalfHeight + GroundClearance;
		Goal.Z = FMath::Min(SnappedZ, Goal.Z + HalfHeight + GroundClearance);
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

float UTimeEraPortalComponent::GetTravelerRadius(const AActor* Traveler) const
{
	if (const ACharacter* Character = Cast<ACharacter>(Traveler))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Capsule->GetScaledCapsuleRadius();
		}
	}

	if (const APawn* Pawn = Cast<APawn>(Traveler))
	{
		if (const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
		{
			return FMath::Min(Root->Bounds.BoxExtent.X, Root->Bounds.BoxExtent.Y);
		}
	}

	return 0.0f;
}

float UTimeEraPortalComponent::GetDeviceHorizontalRadius(const AActor* Device) const
{
	if (!Device)
	{
		return 0.0f;
	}

	const FVector Center = Device->GetActorLocation();
	float Radius = 0.0f;

	TArray<UPrimitiveComponent*> Primitives;
	Device->GetComponents<UPrimitiveComponent>(Primitives);

	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		// Skip the invisible interaction proxy and anything not actually drawn.
		if (!Primitive || !Primitive->IsVisible())
		{
			continue;
		}

		const FBoxSphereBounds World = Primitive->Bounds;
		const FVector Min = World.Origin - World.BoxExtent;
		const FVector Max = World.Origin + World.BoxExtent;

		// Horizontal corners only: the escape is a 2D push, Z is the ground snap's job.
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const FVector Point(
				(Corner & 1) ? Max.X : Min.X,
				(Corner & 2) ? Max.Y : Min.Y,
				Center.Z);
			Radius = FMath::Max(Radius, FVector::Dist2D(Point, Center));
		}
	}

	return Radius;
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
