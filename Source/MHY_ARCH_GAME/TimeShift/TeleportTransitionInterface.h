// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TeleportTransitionTypes.h"
#include "TeleportTransitionInterface.generated.h"

class AActor;

/**
 *  ITeleportTransitionInterface - 传送过场的订阅契约。
 *
 *  传送（UTimeEraPortalComponent）只负责"通知"，不负责"演什么"。
 *  任何实现了本接口的对象都能收到一次传送的开始与结束，用来接：
 *
 *    - **过场动画**：开始 → 播放淡出 / 相机推拉 / 角色动画；结束 → 淡入 / 收尾；
 *    - **UI 提示**：开始 → 显示"传送中…"遮罩或进度；结束 → 收起；
 *    - **输入封锁**：开始 → 禁输入，结束 → 恢复（配合 TimeEraPortal 的 IsTransitioning()）。
 *
 *  时序（见 UTimeEraPortalComponent::TransitionDelay）：
 *
 *      OnTeleportTransitionBegin   ← 还没有移动，世界状态未变
 *              ↓  等待 Duration 秒（可以进行淡出）
 *      （切换时空 + 移动到落点）
 *              ↓
 *      OnTeleportTransitionEnd     ← 已经落点
 *
 *  Duration = 0 时两个通知紧挨着发出（没有可见过场），行为与没有过场系统时一致。
 *
 *  典型实现者：一个全局的"过场导演" Actor，或挂在角色上的相机/UI 管理组件。
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UTeleportTransitionInterface : public UInterface
{
	GENERATED_BODY()
};

class ITeleportTransitionInterface
{
	GENERATED_BODY()

public:

	/** 是否要参与这次传送过场。默认 true；只想处理特定角色/装置就在这里过滤。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Teleport Transition")
	bool CanReceiveTeleportTransition(AActor* Traveler);
	virtual bool CanReceiveTeleportTransition_Implementation(AActor* Traveler) { return true; }

	/** 传送开始：此时**还没有**移动，适合开始淡出 / 播动画 / 禁输入。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Teleport Transition")
	void OnTeleportTransitionBegin(const FTeleportTransitionContext& Context);
	virtual void OnTeleportTransitionBegin_Implementation(const FTeleportTransitionContext& Context) {}

	/** 传送结束：此时**已经**落点，适合淡入 / 收尾 / 恢复输入。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Teleport Transition")
	void OnTeleportTransitionEnd(const FTeleportTransitionContext& Context);
	virtual void OnTeleportTransitionEnd_Implementation(const FTeleportTransitionContext& Context) {}
};
