// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProximityBarrierComponent.generated.h"

class AActor;
class ULightComponent;
class UPrimitiveComponent;
class USoundBase;

/** Fired once, when the barrier materialises and becomes impassable. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProximityBarrierSealed, AActor*, Barrier, AActor*, TriggerActor);

/**
 *  UProximityBarrierComponent - distance-triggered, ONE-SHOT "materialise and seal".
 *
 *  角色沿指定方向离开得足够远之后，原本隐藏且无碰撞的整组东西（墙体 + 门 + 灯光等）
 *  显形并变成不可越过；此后**永久保留**，不会再隐藏或移动。
 *
 *  How it works
 *  ------------
 *  - 该组一开始是隐藏 + 无碰撞的。组件在 BeginPlay 时先把**当前编排状态当作"显形后的样子"
 *    记录下来**（可见性 / 每个图元的碰撞 / 每个灯的强度和开关），然后把整组隐藏掉。
 *    这样你在编辑器里可以按"成品的模样"摆放和调灯，不用反着配。
 *  - 触发条件是**有符号距离**：以 owner 的位置为原点，沿 owner 旋转后的 LocalAxis 投影：
 *
 *        D = Dot(Player - OwnerLocation, OwnerRotation * LocalAxis)
 *
 *    bTriggerOnNegativeSide=false 时 D >= +TriggerDistance 触发；
 *    bTriggerOnNegativeSide=true  时 D <= -TriggerDistance 触发。
 *    只有"往对应方向远离"才会计数 —— 从反方向靠近、或距离绝对值够大但符号不对，都不触发。
 *  - 触发后 SealNow()：整组显形 + 恢复（或强制）碰撞，bSealed 锁存，停止轮询，
 *    再也不会回到隐藏态。
 *
 *  挂在墙体 actor 上即可（β方案：手工挂组件）；门 / 灯若是**别的 actor**，填到 ExtraActors，
 *  或者把它们 attach 到墙体上并保持 bIncludeAttachedActors 打开。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UProximityBarrierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProximityBarrierComponent();

	// -- 触发条件（有符号距离） ---------------------------------------------
	/** 测距方向，在 owner 的局部空间解释（内部会归一化）。符号决定哪一侧算"远离"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	FVector LocalAxis = FVector(1.0f, 0.0f, 0.0f);

	/** 触发阈值（>= 0）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier", meta=(ClampMin="0.0"))
	float TriggerDistance = 1000.0f;

	/**
	 * false：沿 +LocalAxis 远离超过阈值才算（D >= +TriggerDistance）。
	 * true ：沿 -LocalAxis 方向远离超过阈值才算（D <= -TriggerDistance）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bTriggerOnNegativeSide = false;

	/**
	 * 要求角色**先进入**阈值以内，之后再远离才触发。
	 * 打开可以避免"出生点本来就在远处"导致一开局就封死；默认关闭 = 纯距离判定。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bRequireInsideFirst = false;

	/** 用哪个玩家的 pawn 测距。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier", meta=(ClampMin="0"))
	int32 PlayerIndex = 0;

	/** 轮询间隔（秒）。0 = 每帧。触发后不再轮询。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier", meta=(ClampMin="0.0"))
	float UpdateInterval = 0.05f;

	/** 调试：画出测距轴与触发平面。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bDrawDebug = false;

	/** 调试：每轮在屏幕上打出一行状态（用组件本身当 key，不会刷屏）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bDrawOnScreenDebug = false;

	// -- 这一组包含哪些东西 --------------------------------------------------
	/** 一起显形/变实的额外 actor（门、灯挂在别的 actor 上时填这里）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	TArray<TObjectPtr<AActor>> ExtraActors;

	/** 自动带上 attach 到这些 actor 上的子 actor。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bIncludeAttachedActors = true;

	// -- 显形后的状态 --------------------------------------------------------
	/** 显形时，把原本无碰撞的图元强制成 QueryAndPhysics，保证"不可越过"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bForceCollisionWhenShown = true;

	/** 触发后停止轮询（已经永久保留，不需要再查）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	bool bStopPollingAfterTrigger = true;

	/** 可选：封死时播放的声音。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Proximity Barrier")
	TObjectPtr<USoundBase> SealSound;

	// -- API ----------------------------------------------------------------
	/** 是否已经显形并封死（永久）。 */
	UFUNCTION(BlueprintPure, Category="Proximity Barrier")
	bool IsSealed() const { return bSealed; }

	/** 当前有符号距离（沿 LocalAxis 投影）。 */
	UFUNCTION(BlueprintPure, Category="Proximity Barrier")
	float GetSignedDistance() const;

	/** 忽略距离条件，立刻显形并永久封死。 */
	UFUNCTION(BlueprintCallable, Category="Proximity Barrier")
	void SealNow();

	/**
	 * 一行人类可读的状态，供 Blueprint 的 Print String / UE_LOG 使用：
	 *   Barrier <owner> | sealed=no | signed=812.3 | need >=1000.0 | group 1 actor(s) ...
	 */
	UFUNCTION(BlueprintPure, Category="Proximity Barrier")
	FString GetBarrierDebugString() const;

	/** 触发阈值在当前方向上的实际值（调试用，负方向时是负数）。 */
	UFUNCTION(BlueprintPure, Category="Proximity Barrier")
	float GetEffectiveTriggerDistance() const
	{
		return bTriggerOnNegativeSide ? -TriggerDistance : TriggerDistance;
	}

	/** Fired once, when the group materialises. */
	UPROPERTY(BlueprintAssignable, Category="Proximity Barrier")
	FOnProximityBarrierSealed OnSealed;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Latched: once true the group stays materialised forever. */
	bool bSealed = false;

	/** Player was inside the threshold at least once (for bRequireInsideFirst). */
	bool bWasInside = false;

	float TimeSinceCheck = 0.0f;

	/** Every actor taking part in the group (owner + extras + attached). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> TargetActors;

	/** Primitive components of the group, with their authored collision. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> Primitives;

	TArray<TEnumAsByte<ECollisionEnabled::Type>> AuthoredCollision;

	/** Light components of the group, with their authored on/off and intensity. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> Lights;

	TArray<uint8> AuthoredLightVisible;
	TArray<float> AuthoredLightIntensity;

	/** The owner's world-space measuring axis (LocalAxis rotated by the actor). */
	FVector GetWorldAxis() const;

	/** Push the hidden / non-colliding state onto the whole group. */
	void ApplyHiddenState();

	/** Materialise the whole group with the authored (or forced) collision. */
	void ApplyShownState();

	/** Gather the group and remember the authored state as the "shown" state. */
	void CollectTargets();

	/**
	 * The SIDE test only: whether the signed distance is far enough on the requested
	 * side. The "must have been inside first" rule lives in the poll loop, because it
	 * needs to record state.
	 */
	bool EvaluateTrigger(float SignedDistance) const;
};
