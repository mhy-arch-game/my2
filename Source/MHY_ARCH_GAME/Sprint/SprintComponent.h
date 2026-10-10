// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "SprintComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UCharacterMovementComponent;

/** 疾跑状态变化时广播一次（接镜头 FOV / 动画 / 音效）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSprintChanged, bool, bSprinting);

/**
 *  USprintComponent - 按住（或切换）按键进入疾跑。
 *
 *  挂到**玩家角色**上即可，零蓝图接线：
 *
 *    Shift 按下 --> 本组件（自建运行时 mapping context，见下）
 *                    |
 *                    v
 *        UCharacterMovementComponent::MaxWalkSpeed = SprintSpeed
 *                    |
 *                    v
 *        松开 --> 恢复 BeginPlay 时记下的原始 MaxWalkSpeed
 *
 *  为什么要在运行时自建 InputMappingContext？
 *  本项目的 E 键曾经失效，根因就是 `IMC_Interaction` 注册在关卡根本没用的
 *  PlayerController 上（见 Docs/InteractionPickingFix.md）。这里沿用同一个已被验证的
 *  做法（UInteractionDetectorComponent::RegisterInteractContext）：
 *  组件自己 NewObject 一个 UInputMappingContext 并 AddMappingContext 到本地玩家，
 *  所以**不需要改 IMC_Default**，装了就能用。等你把 Shift 正式做进项目 IMC 之后，
 *  把 bRegisterSprintContext 关掉即可。
 *
 *  速度的取/还原遵循本项目一贯的"记录原始值再还原"模式（同 GravityZone）：
 *  BeginPlay 记下角色原本的 MaxWalkSpeed，不疾跑时还回去，
 *  所以不会覆盖别的系统对速度的调整（跌倒、受伤减速等）。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class USprintComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USprintComponent();

	// -- 速度 ---------------------------------------------------------------
	/** 疾跑时的 MaxWalkSpeed。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint", meta=(ClampMin="0.0"))
	float SprintSpeed = 650.0f;

	/** 不疾跑时用的速度；0 = 用角色原本的 MaxWalkSpeed（推荐）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint", meta=(ClampMin="0.0"))
	float NormalSpeedOverride = 0.0f;

	// -- 输入 ---------------------------------------------------------------
	/** 输入动作；留空则运行时自建一个 Boolean 动作（无需任何资产）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	TObjectPtr<UInputAction> SprintAction;

	/** 触发键，默认左 Shift。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	FKey SprintKey;

	/** 同时把右 Shift 也映射上。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bIncludeRightShift = true;

	/** true = 按住才疾跑；false = 按一下切换。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bHoldToSprint = true;

	/** 运行时自建 mapping context（不改项目 IMC）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bRegisterSprintContext = true;

	/** 上述运行时 context 的优先级。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	int32 SprintContextPriority = 0;

	// -- 条件 ---------------------------------------------------------------
	/** 只在朝前跑时疾跑（掉头就退出疾跑）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bRequireForwardInput = false;

	/** 上面的"朝前"判定阈值：加速度方向与该角色前向的点积下限。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint", meta=(EditCondition="bRequireForwardInput", ClampMin="-1.0", ClampMax="1.0"))
	float MinForwardDot = 0.25f;

	/** 蹲下时不允许疾跑。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bIgnoreWhenCrouched = true;

	/** 滞空时保持疾跑速度。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bKeepSpeedInAir = true;

	// -- 调试 / 开关 --------------------------------------------------------
	/** 总开关：关掉后拒绝一切疾跑（过场、剧情限制等）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bSprintEnabled = true;

	/** 状态变化时打日志。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bLogStateChanges = true;

	/** 屏幕上打一行实时状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sprint")
	bool bDrawOnScreenDebug = false;

	// -- API ----------------------------------------------------------------
	/** 当前是否在疾跑。 */
	UFUNCTION(BlueprintPure, Category="Sprint")
	bool IsSprinting() const { return bSprinting; }

	/** 键是否被按住（切换模式下表示"逻辑上想疾跑"）。 */
	UFUNCTION(BlueprintPure, Category="Sprint")
	bool IsSprintRequested() const { return bSprintRequested; }

	/** 直接设置疾跑状态（会走同一套速度/事件逻辑）。 */
	UFUNCTION(BlueprintCallable, Category="Sprint")
	void SetSprinting(bool bNewSprinting);

	/** 开关整个功能。 */
	UFUNCTION(BlueprintCallable, Category="Sprint")
	void SetSprintEnabled(bool bEnabled) { bSprintEnabled = bEnabled; if (!bEnabled) { SetSprinting(false); } }

	/** 速度来源（调试）：0 = 角色原本值，1 = SprintSpeed，2 = NormalSpeedOverride。 */
	UFUNCTION(BlueprintPure, Category="Sprint")
	float GetAuthoredWalkSpeed() const { return AuthoredMaxWalkSpeed; }

	/** 一行人类可读状态，给 Print String / UE_LOG 用。 */
	UFUNCTION(BlueprintPure, Category="Sprint")
	FString GetSprintDebugString() const;

	/** 疾跑状态变化时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Sprint")
	FOnSprintChanged OnSprintChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 键按住 / 逻辑上想疾跑。 */
	bool bSprintRequested = false;

	/** 实际是否在疾跑。 */
	bool bSprinting = false;

	bool bInputBound = false;
	bool bContextRegistered = false;

	/** BeginPlay 记下的角色原始 MaxWalkSpeed。 */
	float AuthoredMaxWalkSpeed = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;

	/** 运行时自建的输入动作 / 映射上下文。 */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> RuntimeSprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeSprintContext;

	/** 按下（切换模式在这里翻转；按住模式只置位）。 */
	void HandleSprintPressed();

	/** 按住期间每帧触发（只在按住模式下维持状态）。 */
	void HandleSprintHeld();

	/** 松开。 */
	void HandleSprintReleased();

	/** 把 bSprintRequested 与实际条件合成为最终状态。 */
	void EvaluateSprintState();

	/** 重试绑定输入（Pawn 的 EnhancedInputComponent 可能还没建好）。 */
	void TryBindInput();

	/** 重试注册运行时 mapping context（可能还没被 possessed）。 */
	void RegisterSprintContext();

	/** 前进方向的判定。 */
	bool IsMovingForward() const;

	/** 把速度写进移动组件。 */
	void ApplySpeed();
};
