// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProximitySequenceComponent.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Class.h"
#include "Math/Box.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlaybackSettings.h"

/**
 * 调试钩子：>0 = 关卡开始 N 秒后，自动对场景里所有 ProximitySequenceComponent 调一次 TriggerNow()。
 * 用途：headless / 无玩家的环境里也能驱动"播序列 → 播完开碰撞"这整条链路（正常游戏保持 0）。
 */
static TAutoConsoleVariable<int32> CVarProximitySequenceAutoTrigger(
	TEXT("ps.AutoTrigger"), 0,
	TEXT(">0 = 关卡运行 N 秒后自动对所有 ProximitySequenceComponent 调一次 TriggerNow（调试用）。"),
	ECVF_Default);

UProximitySequenceComponent::UProximitySequenceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UProximitySequenceComponent::BeginPlay()
{
	Super::BeginPlay();

	const float InitialDistance = GetDistanceToTarget();
	UE_LOG(LogTemp, Log,
		TEXT("[ProximitySequence] %s: BeginPlay（序列=%s，触发 %.0fcm，慢速区 %.0fcm / %.0f cm/s，初始距离 %.1f，有主控=%d）。"),
		*GetNameSafe(GetOwner()),
		Sequence.IsNull() ? TEXT("<未设置>") : *Sequence.ToSoftObjectPath().ToString(),
		TriggerDistance, SlowZoneDistance, SlowSpeed, InitialDistance, InitialDistance >= 0.0f ? 1 : 0);

	if (InitialDistance < 0.0f)
	{
		// 没有本地玩家（例如放在服务器侧 / headless 环境）时距离触发不成立，可用 ps.AutoTrigger 调试。
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

	// 调试钩子：ps.AutoTrigger 秒后自动触发一次（无玩家环境验证 "播序列 → 播完开碰撞" 用）。
	const int32 AutoTriggerSeconds = CVarProximitySequenceAutoTrigger.GetValueOnGameThread();
	if (AutoTriggerSeconds > 0 && !bTriggered && GetWorld() && GetWorld()->GetTimeSeconds() >= AutoTriggerSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: ps.AutoTrigger=%d 到点，调试触发。"),
			*GetNameSafe(GetOwner()), AutoTriggerSeconds);
		TriggerNow();
	}

	// 慢速区独立于触发：触发过一次之后到"变成实体"之前仍然限速，所以不再在这里停 tick。
	UpdateSlowZone(Distance);

	// 兜底：运行时创建的播放器万一不回调 OnFinished，也能靠"由播到停"补上。
	PollSequenceFinished();

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
	bFinishedHandled = false;
	bSequencePlaying = false;
	bWasPlaying = false;
	bBlocked = false;
	SetComponentTickEnabled(true);
}

bool UProximitySequenceComponent::PlaySequence()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 播放前记下碰撞状态：序列/其它系统可能把组件碰撞关掉，"开碰撞"时要能精确还原。
	SnapshotCollisionState();
	bFinishedHandled = false;
	bSequencePlaying = true;
	bWasPlaying = true;

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
	if (bFinishedHandled)
	{
		return;
	}
	bFinishedHandled = true;
	bSequencePlaying = false;
	bWasPlaying = false;

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

	// 关键：AActor::SetActorEnableCollision 只在"actor 级开关发生变化"时才通知组件
	// （Actor.cpp: 5269 处直接 return），所以它无法恢复被单独关掉的组件碰撞。
	// 真正可靠的做法是逐个图元组件把碰撞打开——恢复播放前记下的状态。
	Owner->SetActorEnableCollision(true);

	FString Detail;
	int32 Count = 0;
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Comp);
		if (!Primitive)
		{
			continue;
		}

		Primitive->SetHiddenInGame(false);

		ECollisionEnabled::Type Desired = ECollisionEnabled::QueryAndPhysics;
		const TWeakObjectPtr<UPrimitiveComponent> Key(Primitive);
		if (const TEnumAsByte<ECollisionEnabled::Type>* Saved = SavedCollision.Find(Key))
		{
			Desired = *Saved;
		}
		if (Desired == ECollisionEnabled::NoCollision)
		{
			// 关卡里这堵墙本来就是 QueryAndPhysics；快照若拿到的是"关"，也按能挡路处理。
			Desired = ECollisionEnabled::QueryAndPhysics;
		}

		Primitive->SetCollisionEnabled(Desired);

		Detail += FString::Printf(TEXT("%s=%s/%s "), *Primitive->GetName(),
			*UEnum::GetValueAsString(Primitive->GetCollisionEnabled()),
			*Primitive->GetCollisionProfileName().ToString());
		++Count;
	}

	bBlocked = true;

	UE_LOG(LogTemp, Log,
		TEXT("[ProximitySequence] %s: 已显形 + 开启碰撞（阻挡道路）。actorCollision=%d，图元 %d 个：%s"),
		*GetNameSafe(Owner), Owner->GetActorEnableCollision() ? 1 : 0, Count, *Detail);
}

void UProximitySequenceComponent::SnapshotCollisionState()
{
	SavedCollision.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Comp))
		{
			SavedCollision.Add(TWeakObjectPtr<UPrimitiveComponent>(Primitive), Primitive->GetCollisionEnabled());
		}
	}
}

void UProximitySequenceComponent::PollSequenceFinished()
{
	if (!bSequencePlaying)
	{
		return;
	}

	ULevelSequencePlayer* Player = RuntimePlayer;
	if (!Player && SequenceActor)
	{
		Player = SequenceActor->GetSequencePlayer();
	}
	if (!Player)
	{
		return;
	}

	const bool bNowPlaying = Player->IsPlaying();
	if (bWasPlaying && !bNowPlaying)
	{
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 播放器已停止但没收到 OnFinished，按结束处理。"),
			*GetNameSafe(GetOwner()));
		HandleSequenceFinished();
		return;
	}

	bWasPlaying = bNowPlaying;
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
	const float Cap = FMath::Max(0.0f, SlowSpeed);
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
		UE_LOG(LogTemp, Log, TEXT("[ProximitySequence] %s: 慢速区 %.1fcm 内（限速 %.0f cm/s，角色 MaxWalkSpeed %.0f）。"),
			*GetNameSafe(GetOwner()), Distance, Cap, Movement->MaxWalkSpeed);
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
