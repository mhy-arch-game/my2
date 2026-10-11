// Copyright Epic Games, Inc. All Rights Reserved.

#include "GravityZoneComponent.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

namespace
{
	/**
	 * 每个角色一份"纯原始"物理参数 + 当前生效的重力区数量。
	 *
	 * 为什么不能各存各的：两个区域盒子重叠时，角色会同时被两边追踪。
	 * 若各自在进入时快照"当前值"，第二个区快照到的就是第一个区**改过**的值，
	 * 离开时先后写回，最后会把被改过的值当"原始值"留下 —— 表现就是
	 * "走出跳跃区域后仍然能超跳"（本项目 jumping_area / jumping_area2 就是重叠的）。
	 *
	 * 键是角色自己的 CharacterMovementComponent（弱引用，角色销毁后自然失效）。
	 */
	struct FGravityZoneSharedState
	{
		float AuthoredGravityScale = 1.0f;
		float AuthoredJumpZVelocity = 0.0f;
		int32 ActiveZones = 0;
	};

	TMap<TWeakObjectPtr<UCharacterMovementComponent>, FGravityZoneSharedState> GGravityZoneShared;
}

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

	// Never leave a character floating: release everyone still tracked（计数减一，
	// 只有最后一个区域被销毁时才写回原始值）。
	TMap<TWeakObjectPtr<AActor>, FGravityZoneEntry> Pending = MoveTemp(Tracked);
	Tracked.Empty();

	for (const TPair<TWeakObjectPtr<AActor>, FGravityZoneEntry>& Pair : Pending)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			ReleaseFrom(Actor);
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

	Tracked.Remove(OtherActor);

	ReleaseFrom(OtherActor);

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

	FGravityZoneSharedState& Shared = GGravityZoneShared.FindOrAdd(Movement);
	if (Shared.ActiveZones == 0)
	{
		// 第一个影响该角色的重力区：记下**纯原始**值（不是别的区域改过的值）。
		Shared.AuthoredGravityScale = Movement->GravityScale;
		Shared.AuthoredJumpZVelocity = Movement->JumpZVelocity;
	}
	++Shared.ActiveZones;

	FGravityZoneEntry Entry;
	Entry.GravityScale = Shared.AuthoredGravityScale;
	Entry.JumpZVelocity = Shared.AuthoredJumpZVelocity;
	Tracked.Add(Actor, Entry);

	// 一律从"纯原始值"出发乘倍率：重叠区不会互相叠乘。
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

	UE_LOG(LogTemp, Log,
		TEXT("[GravityZone] %s: %s 进入（重力 %.2f -> %.2f，起跳 %.0f -> %.0f，区域内生效区数 %d）。"),
		*GetName(), *Actor->GetActorNameOrLabel(), Entry.GravityScale, Movement->GravityScale,
		Entry.JumpZVelocity, Movement->JumpZVelocity, Shared.ActiveZones);

	OnActorEntered.Broadcast(Actor, Movement->GravityScale);
}

void UGravityZoneComponent::ReleaseFrom(AActor* Actor)
{
	UCharacterMovementComponent* Movement = GetMovement(Actor);
	if (!Movement)
	{
		return;
	}

	TWeakObjectPtr<UCharacterMovementComponent> Key(Movement);
	FGravityZoneSharedState* Shared = GGravityZoneShared.Find(Key);
	if (!Shared)
	{
		return;
	}

	Shared->ActiveZones = FMath::Max(Shared->ActiveZones - 1, 0);

	const bool bLastZone = Shared->ActiveZones == 0;
	if (bLastZone)
	{
		// 最后一个区域也离开了：写回**纯原始**值（不管本区域是否改过起跳速度，
		// 写回原始值都是安全的；没改过时它是等值操作）。
		Movement->GravityScale = Shared->AuthoredGravityScale;
		Movement->JumpZVelocity = Shared->AuthoredJumpZVelocity;
		GGravityZoneShared.Remove(Key);
	}

	UE_LOG(LogTemp, Log,
		TEXT("[GravityZone] %s: %s 离开（剩余生效区数 %d，重力 %.2f，起跳 %.0f%s）。"),
		*GetName(), *Actor->GetActorNameOrLabel(), Shared ? Shared->ActiveZones : 0,
		Movement->GravityScale, Movement->JumpZVelocity,
		bLastZone ? TEXT("，已写回原始值") : TEXT("，仍被其它区影响，暂不写回"));
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
