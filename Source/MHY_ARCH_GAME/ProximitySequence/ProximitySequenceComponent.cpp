// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProximitySequenceComponent.h"

#include "Engine/World.h"
#include "Math/Box.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"

UProximitySequenceComponent::UProximitySequenceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProximitySequenceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetDistanceToTarget() < 0.0f)
	{
		// 没有本地玩家（例如放在服务器侧）时没必要每帧算。
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 找不到主控角色，距离触发暂不生效。"),
			*GetNameSafe(GetOwner()));
	}

	if (bApplyInitialStateOnBeginPlay)
	{
		ApplyPreBlockState();
	}

	if (bOnce && bTriggered && !bSlowPlayerNearby)
	{
		SetComponentTickEnabled(false);
	}
}

AActor* UProximitySequenceComponent::ResolveTargetActor() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

float UProximitySequenceComponent::GetDistanceToTarget() const
{
	const AActor* Owner = GetOwner();
	const AActor* Target = ResolveTargetActor();
	if (!Owner || !Target)
	{
		return -1.0f;
	}

	// 到"包围盒最近点"的距离：墙再大，也是"贴到墙面 N 厘米"这个意思。
	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	Owner->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);

	const FBox Box(Origin - Extent, Origin + Extent);
	const FVector TargetLocation = Target->GetActorLocation();
	return FVector::Dist(TargetLocation, Box.GetClosestPointTo(TargetLocation));
}

void UProximitySequenceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Distance = GetDistanceToTarget();

	// 慢速区独立于触发：触发过一次之后到"变成实体"之前仍然限速，所以不再在这里停 tick。
	UpdateSlowZone(Distance);

	if (bOnce && bTriggered)
	{
		return;
	}

	if (Distance < 0.0f)
	{
		return;
	}

	if (DebugLogRange > 0.0f && Distance <= DebugLogRange)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 距离 %.1fcm（触发线 %.1fcm）。"),
			*GetNameSafe(GetOwner()), Distance, TriggerDistance);
	}

	if (Distance <= TriggerDistance)
	{
		TriggerNow();
	}
}

void UProximitySequenceComponent::TriggerNow()
{
	const float Distance = GetDistanceToTarget();

	if (!PlaySequence())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ProximitySequence] %s: 没能播放序列（检查 Sequence / SequenceActor 是否填好）。"),
			*GetNameSafe(GetOwner()));
		return;
	}

	bTriggered = true;

	UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 首次进入距离触发（%.1fcm ≤ %.1fcm），开始播放序列。"),
		*GetNameSafe(GetOwner()), Distance, TriggerDistance);

	OnTriggered.Broadcast(ResolveTargetActor(), Distance);

	// 注意：这里不停 tick —— 动画播放期间慢速区还得继续生效，
	// 停 tick 放在序列播完（HandleSequenceFinished）时。
}

void UProximitySequenceComponent::ResetTrigger()
{
	bTriggered = false;
	SetComponentTickEnabled(true);
}

bool UProximitySequenceComponent::PlaySequence()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 1) 关卡里已经有现成的 LevelSequenceActor 就直接驱动它。
	if (SequenceActor)
	{
		if (ULevelSequencePlayer* Existing = SequenceActor->GetSequencePlayer())
		{
			Existing->SetPlayRate(PlayRate);
			Existing->OnFinished.AddUniqueDynamic(this, &UProximitySequenceComponent::HandleSequenceFinished);
			Existing->Play();
			return true;
		}
	}

	// 2) 否则按资产自己起一个播放器（引擎自带，用完自动留在世界里）。
	ULevelSequence* LoadedSequence = Sequence.LoadSynchronous();
	if (!LoadedSequence)
	{
		return false;
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bAutoPlay = false;
	Settings.PlayRate = PlayRate;
	Settings.LoopCount.Value = bLoop ? -1 : 0;   // -1 = 无限循环，0 = 播一次
	// 播完保持动画的最终姿态（否则可能被还原到播放前的位置，"向前移动"就白播了）。
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceKeepState;

	ALevelSequenceActor* OutActor = nullptr;
	RuntimePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(World, LoadedSequence, Settings, OutActor);
	if (!RuntimePlayer)
	{
		return false;
	}

	RuntimePlayer->OnFinished.AddUniqueDynamic(this, &UProximitySequenceComponent::HandleSequenceFinished);
	RuntimePlayer->Play();
	return true;
}

void UProximitySequenceComponent::HandleSequenceFinished()
{
	UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 序列播放结束。"), *GetNameSafe(GetOwner()));

	if (bBlockOnFinish)
	{
		ApplyBlockedState();
	}

	OnSequenceFinished.Broadcast();

	// 触发过 + 已经变成实体：本组件没有别的事了。
	if (bOnce)
	{
		SetComponentTickEnabled(false);
	}
}

void UProximitySequenceComponent::ApplyBlockedState()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// 位移由动画资产负责（关卡序列里那 300cm 的关键帧）；这里把"挡路"坐实：显形 + 开碰撞。
	Owner->SetActorHiddenInGame(false);
	Owner->SetActorEnableCollision(true);
	bBlocked = true;

	UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 已显形 + 开启碰撞（阻挡道路）。"), *GetNameSafe(Owner));
}

void UProximitySequenceComponent::ApplyPreBlockState()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Owner->SetActorHiddenInGame(true);
	Owner->SetActorEnableCollision(false);
	bBlocked = false;

	UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 初始状态 = 不可见 + 关碰撞（等首次靠近播动画）。"),
		*GetNameSafe(Owner));
}

void UProximitySequenceComponent::UpdateSlowZone(float Distance)
{
	bSlowingPlayer = false;

	if (!bSlowPlayerNearby)
	{
		return;
	}

	// 墙已经是实体了，恢复正常移速：慢速区只是为了别让人在"变成实体"的那一刻站在墙里。
	if (bBlocked)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(ResolveTargetActor());
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const float Zone = SlowZoneDistance > 0.0f ? SlowZoneDistance : TriggerDistance;
	if (Distance < 0.0f || Distance > Zone)
	{
		return;
	}

	// 每帧只把"水平速度"钳到上限，不写任何持久状态：
	// 离开区域自然恢复，也不用担心和疾跑抢 MaxWalkSpeed。
	// 倍率乘的是"当时的 MaxWalkSpeed"，所以疾跑在区域内只是上限更高，仍然被限速。
	const float Cap = Movement->MaxWalkSpeed * FMath::Clamp(SlowSpeedMultiplier, 0.0f, 1.0f);
	const FVector Velocity = Movement->Velocity;
	const FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);
	if (Horizontal.SizeSquared() > Cap * Cap)
	{
		const FVector Clamped = Horizontal.GetSafeNormal() * Cap;
		Movement->Velocity = FVector(Clamped.X, Clamped.Y, Velocity.Z);
	}

	bSlowingPlayer = true;

	if (bDebugLogSlowZone)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 慢速区 %.1fcm 内（上限 %.0f = %.0f x %.2f）。"),
			*GetNameSafe(GetOwner()), Distance, Cap, Movement->MaxWalkSpeed, SlowSpeedMultiplier);
	}
}

FString UProximitySequenceComponent::GetProximitySequenceDebugString() const
{
	return FString::Printf(
		TEXT("ProximitySequence %s | distance=%.1f (trigger<=%.1f) | triggered=%s | blocking=%s | slow=%s | sequence=%s | actor=%s"),
		*GetNameSafe(GetOwner()),
		GetDistanceToTarget(), TriggerDistance,
		bTriggered ? TEXT("yes") : TEXT("no"),
		bBlocked ? TEXT("yes") : TEXT("no"),
		bSlowingPlayer ? TEXT("yes") : TEXT("no"),
		Sequence.IsNull() ? TEXT("<未设置>") : *Sequence.ToString(),
		SequenceActor ? *SequenceActor->GetName() : TEXT("none"));
}
