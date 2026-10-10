// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProximitySequenceComponent.generated.h"

class ALevelSequenceActor;
class ULevelSequence;
class ULevelSequencePlayer;

/** 首次进入距离时广播（Distance 是当时的实测距离）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProximitySequenceTriggered, AActor*, TriggeredBy, float, Distance);

/** 序列播完、本体已经变成"阻挡"状态后广播。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProximitySequenceFinished);

/**
 *  UProximitySequenceComponent —— 目标首次进入"距本体一定距离内"时播放一段关卡序列。
 *
 *  用法（以机关的墙体为例）：把本组件加到墙体 Actor 上 → 填 Sequence（如 LS_jiguanqiang1）、
 *  调 TriggerDistance → 玩家第一次走近到该距离内时自动播一次，之后不再触发（bOnce）。
 *
 *  距离定义：目标位置到**本体包围盒最近点**的距离（FBox::GetClosestPointTo），
 *  所以墙很大也不怕 —— "走到墙边上 N 厘米内"就是字面意思；上下方向同样算。
 *
 *  复用点：延续 ProximityBarrier 的"距离判定 + 一次性"思路；播放走引擎自带的
 *  ULevelSequencePlayer，不引入新机制。
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UProximitySequenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximitySequenceComponent();

	/** 要播放的关卡序列（如 LS_jiguanqiang1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	TSoftObjectPtr<ULevelSequence> Sequence;

	/** 也可以直接驱动关卡里已有的 LevelSequenceActor（填了优先用它，不再新建播放器）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	/** 触发距离（cm）：目标到本体包围盒最近点的距离小于它就算"到达"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence", meta=(ClampMin="0.0"))
	float TriggerDistance = 600.0f;

	/** 只触发一次（首次到达）。关掉则离开后再进入可重复触发。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	bool bOnce = true;

	/** 循环播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	bool bLoop = false;

	/** 播放速率（1 = 原速）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence", meta=(ClampMin="0.01"))
	float PlayRate = 1.0f;

	/** 距离小于它多少开始"预热"？0 = 关闭。仅打日志用，方便调参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence", meta=(ClampMin="0.0"))
	float DebugLogRange = 0.0f;

	// -- 靠近时的慢速区（防止玩家在动画播完时正好站在墙里）--------------
	/**
	 * 进入慢速区时限制玩家移速。默认开。
	 * 目的：墙要 5 秒动画才移到位，玩家若全速冲过来，很可能动画结束时正好站在墙的落点上。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence|Slow Zone")
	bool bSlowPlayerNearby = true;

	/**
	 * 慢速区半径（cm）：目标到本体包围盒最近点小于它就开始限速。
	 * ≤ 0 = 直接用 TriggerDistance。建议比 TriggerDistance 稍大，让玩家"提前减速"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence|Slow Zone", meta=(EditCondition="bSlowPlayerNearby"))
	float SlowZoneDistance = 900.0f;

	/**
	 * 慢速倍率（测试用，先给个偏保守的值）：区域内水平速度上限 = 当时的
	 * CharacterMovement->MaxWalkSpeed × 本倍率。1 = 不限速。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence|Slow Zone", meta=(EditCondition="bSlowPlayerNearby", ClampMin="0.05", ClampMax="1.0"))
	float SlowSpeedMultiplier = 0.4f;

	/** 调试：限速时每帧打一行日志（默认关，会刷屏）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence|Slow Zone", meta=(EditCondition="bSlowPlayerNearby"))
	bool bDebugLogSlowZone = false;

	/**
	 * 播放前先把本体设成"未阻挡"：不可见 + 关碰撞（等动画播完再开启）。
	 * 默认关 —— 关卡里的墙原本就是"可见 + 有碰撞"，不需要组件改初始状态。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	bool bApplyInitialStateOnBeginPlay = false;

	/** 序列播完后把本体变成"阻挡"状态：显形 + 开碰撞，并保持动画的最终位置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Sequence")
	bool bBlockOnFinish = true;

	/** 触发时广播（可接音效 / 镜头 / 其它逻辑）。 */
	UPROPERTY(BlueprintAssignable, Category="Proximity Sequence")
	FOnProximitySequenceTriggered OnTriggered;

	/** 序列播放结束时广播（本体此时已经是阻挡状态）。 */
	UPROPERTY(BlueprintAssignable, Category="Proximity Sequence")
	FOnProximitySequenceFinished OnSequenceFinished;

	/** 立刻播一次（忽略距离与"已触发"，可用于调试/蓝图调用）。 */
	UFUNCTION(BlueprintCallable, Category="Proximity Sequence")
	void TriggerNow();

	/** 复位（允许再次触发）。 */
	UFUNCTION(BlueprintCallable, Category="Proximity Sequence")
	void ResetTrigger();

	UFUNCTION(BlueprintPure, Category="Proximity Sequence")
	bool IsTriggered() const { return bTriggered; }

	/** 是否已经进入"阻挡"状态（序列播完并显形 + 开碰撞）。 */
	UFUNCTION(BlueprintPure, Category="Proximity Sequence")
	bool IsBlocking() const { return bBlocked; }

	/** 玩家此刻是否正被慢速区限速。 */
	UFUNCTION(BlueprintPure, Category="Proximity Sequence")
	bool IsSlowingPlayer() const { return bSlowingPlayer; }

	/** 目标到本体包围盒最近点的距离（cm）；找不到目标返回 -1。 */
	UFUNCTION(BlueprintPure, Category="Proximity Sequence")
	float GetDistanceToTarget() const;

	/** 一行状态，给 Print String 用。 */
	UFUNCTION(BlueprintPure, Category="Proximity Sequence")
	FString GetProximitySequenceDebugString() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 玩家（本地控制器 Pawn）；拿不到就返回空。 */
	AActor* ResolveTargetActor() const;

	/** 播放序列；返回是否成功起播。 */
	bool PlaySequence();

	/** 序列播放结束：显形 + 开碰撞（bBlockOnFinish 时）。 */
	UFUNCTION()
	void HandleSequenceFinished();

	/** 把本体变成"阻挡"状态（显形 + 开碰撞）。 */
	void ApplyBlockedState();

	/** 把本体设成"未阻挡"状态（不可见 + 关碰撞）。 */
	void ApplyPreBlockState();

	/** 按当前距离维护慢速区（每帧调用；只钳速度，不留状态，所以和疾跑等系统不打架）。 */
	void UpdateSlowZone(float Distance);

	/** 本组件运行时创建的播放器（SequenceActor 为空时用）。 */
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> RuntimePlayer;

	bool bTriggered = false;

	/** 已经是"阻挡"状态（显形 + 碰撞已开）。 */
	bool bBlocked = false;

	/** 本帧是否对玩家执行了限速。 */
	bool bSlowingPlayer = false;
};
