// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "GravityZoneComponent.generated.h"

class UCharacterMovementComponent;

/** Broadcast when an actor enters or leaves the zone (AppliedGravityScale is its new value). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGravityZoneActorChanged, AActor*, Actor, float, AppliedGravityScale);

/** Authored movement values captured the moment an actor enters the zone. */
struct FGravityZoneEntry
{
	float GravityScale = 1.0f;
	float JumpZVelocity = 0.0f;
};

/**
 *  UGravityZoneComponent - a box-shaped region with reduced gravity.
 *
 *  The component IS the zone: place it, size the box, and every character whose
 *  capsule overlaps it has UCharacterMovementComponent::GravityScale multiplied
 *  by GravityScaleInside. On exit the character's own authored gravity scale
 *  (and jump velocity, when scaled) is restored exactly - the component never
 *  assumes the default value of 1.
 *
 *  It is query-only and hidden in game, so it never blocks movement.
 *
 *  NOTE on overlapping zones: each zone caches and restores its own values, so
 *  two zones overlapping the same character are NOT additive - the last one to
 *  apply wins, and leaving restores the authored value. Keep zones disjoint.
 */
UCLASS(ClassGroup=(Gravity), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UGravityZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UGravityZoneComponent();

	/** Multiplier applied to GravityScale while inside. 1 = unchanged, 0.3 = floaty, 0 = weightless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone", meta=(ClampMin="0.0", ClampMax="10.0"))
	float GravityScaleInside = 0.3f;

	/** Only affect Pawns (typical). Turn off to also affect anything with a CharacterMovementComponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone")
	bool bAffectPawnsOnly = true;

	/**
	 * Also scale JumpZVelocity by sqrt(GravityScaleInside).
	 *
	 * 这是"改变重力但不改变跳跃高度"的关键开关，两个方向都成立：
	 *   重力 ×k、起跳速度 ×√k  ⇒  最高点 h = v²/(2g) 不变，
	 *   而上升时间 t = v/g 变成 t/√k —— k>1 时上升下落都更快（干脆），
	 *   k<1 时滞空更久（飘）。
	 *
	 * 所以 **GravityScaleInside > 1 且本开关为 true** 就是需求里的
	 * "保留最高可达高度、但上升下落速度加快"；用 ConfigurePreservingJumpHeight()
	 * 可以直接表达这个意图。
	 *
	 * 关闭时不动起跳速度：k<1 会跳得更高，k>1 会跳得更低。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone")
	bool bScaleJumpVelocity = false;

	/**
	 * 目标最高跳跃高度（cm）。> 0 时启用，并**优先于 bScaleJumpVelocity**。
	 *
	 * 与 bScaleJumpVelocity 的分工：
	 *   - bScaleJumpVelocity 是"相对"的 —— 保留**正常重力（倍率 1）**下的高度；
	 *   - TargetJumpHeight 是"绝对"的 —— 不管本区重力多少，最高点都正好等于这个高度。
	 *
	 * 用于"本区原本靠极低重力跳得很高，现在想**保留这个高度、只让上升下落更快**"：
	 * 把原来的高度量出来填这里，再把 GravityScaleInside 调大，空中时间即变成
	 * sqrt(旧倍率 / 新倍率) 倍（例如 0.08 → 0.32 就是快一倍）。
	 *
	 * 起跳速度按本区**实际生效**的重力反算：v = sqrt(2 · |GetGravityZ()| · 高度)。
	 * （UE5 的 UCharacterMovementComponent::GetGravityZ() 内部已经把 GravityScale 乘进去了，
	 *  取到的就是角色此刻真正受到的重力，不要再乘一次。）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gravity Zone", meta=(ClampMin="0.0", Units="cm"))
	float TargetJumpHeight = 0.0f;

	/** Whether Actor is currently inside the zone and being affected by it. */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	bool IsActorInside(const AActor* Actor) const;

	/** How many actors are currently tracked as inside the zone. */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	int32 GetTrackedActorCount() const { return Tracked.Num(); }

	/**
	 * 一键表达"同样跳多高，但上升下落更快（或更飘）"。
	 *
	 * 依据 h = v²/(2g)：重力 ×k 的同时把起跳速度 ×√k，最高点不变，
	 * 而上升/下落时间变成 1/√k。k = 1.8 时快约 1.34 倍，k = 2.5 时快约 1.58 倍。
	 *
	 * @param GravityMultiplier  k；>1 = 更重更干脆，<1 = 更飘
	 * @param bAlsoScaleJumpVelocity 是否按 √k 一并放大起跳速度（要"高度不变"就必须传 true）
	 */
	UFUNCTION(BlueprintCallable, Category="Gravity Zone")
	void ConfigurePreservingJumpHeight(float GravityMultiplier, bool bAlsoScaleJumpVelocity);

	/** 跳跃高度的变化倍率：1 = 高度不变（bScaleJumpVelocity 打开时恒为 1）。 */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	float GetJumpHeightScale() const;

	/** 上升/下落时间的变化倍率：重力 ×k 且起跳 ×√k 时为 1/√k（越小越干脆）。 */
	UFUNCTION(BlueprintPure, Category="Gravity Zone")
	float GetAirTimeScale() const;

	UPROPERTY(BlueprintAssignable, Category="Gravity Zone")
	FOnGravityZoneActorChanged OnActorEntered;

	UPROPERTY(BlueprintAssignable, Category="Gravity Zone")
	FOnGravityZoneActorChanged OnActorExited;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/** Authored values per tracked actor, captured on entry. */
	TMap<TWeakObjectPtr<AActor>, FGravityZoneEntry> Tracked;

	static UCharacterMovementComponent* GetMovement(const AActor* Actor);

	/** Capture + apply reduced gravity (no-op if already tracked or not affected). */
	void ApplyTo(AActor* Actor);

	/** Write the captured authored values back. */
	void RestoreFrom(AActor* Actor, const FGravityZoneEntry& Entry);
};
