// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EndGameSubsystem.generated.h"

class APlayerController;
class SWidget;

/** 结束流程开始时广播（表现层可以据此停音乐 / 关 HUD / 停 UiCue）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEndGameSequenceStarted);

/**
 *  UEndGameSubsystem —— 「结束游戏」的统一入口（GameInstanceSubsystem，模块自驱）。
 *
 *  流程：触发 -> 全屏渐黑（默认 1.2 秒）-> 保持全黑（默认 0.6 秒）-> 结束游戏进程。
 *  渐黑期间冻结玩家的移动 / 视角输入，并把黑屏盖在 HUD 之上；重复触发自动忽略。
 *
 *  三种调用方式（任选）：
 *    1. 蓝图节点：MHY | Game Flow -> End Game With Fade（静态库函数，一个节点搞定）；
 *    2. 蓝图：取到本子系统后调 Begin End Game（想自己接前后逻辑时用）；
 *    3. C++：UEndGameSubsystem::Get(WorldContext)->BeginEndGame();
 *
 *  关卡 firstvision 里已经摆好了现成的物件：蓝图类 endofthegame（AEndOfTheGameActor），
 *  interact 模块的可交互对象，交互提示词「触碰」，按 E 就走这条流程。
 */
UCLASS()
class UEndGameSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 从任意世界上下文取本子系统（拿不到返回 nullptr）。 */
	UFUNCTION(BlueprintPure, Category="End Game", meta=(WorldContext="WorldContextObject"))
	static UEndGameSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * 开始结束流程：渐黑 FadeOutSeconds -> 全黑保持 BlackHoldSeconds -> 退出进程。
	 * 已经在结束时再次调用会被忽略；两段都填 0 就是「立刻黑屏 + 立刻退出」。
	 */
	UFUNCTION(BlueprintCallable, Category="End Game")
	void BeginEndGame(float FadeOutSeconds = 1.2f, float BlackHoldSeconds = 0.6f);

	/** 中断流程并撤掉黑屏（只给特殊需求用，正常结束流程用不到）。 */
	UFUNCTION(BlueprintCallable, Category="End Game")
	void CancelEndGame();

	UFUNCTION(BlueprintPure, Category="End Game")
	bool IsEnding() const { return bEnding; }

	/** 当前黑屏不透明度（0 = 透明，1 = 全黑）。 */
	UFUNCTION(BlueprintPure, Category="End Game")
	float GetFadeAlpha() const { return FadeAlpha; }

	/** 结束流程已经开始（表现层可用来停音乐 / 关 HUD）。 */
	UPROPERTY(BlueprintAssignable, Category="End Game")
	FOnEndGameSequenceStarted OnEndGameSequenceStarted;

private:
	bool HandleTick(float DeltaTime);

	/** 建 / 拆全屏黑屏（Slate SImage，直接盖在视口最上层）。 */
	void CreateBlackScreen();
	void RemoveBlackScreen();
	void ApplyFadeAlpha();

	/** 冻结 / 恢复玩家移动与视角输入。 */
	void SetPlayerInputFrozen(bool bFrozen);

	/** 请求退出进程（QuitGame，并带超时兜底）。 */
	void QuitProcess();

	bool bEnding = false;
	float FadeAlpha = 0.0f;
	float Elapsed = 0.0f;
	float FadeOutSeconds = 1.2f;
	float HoldSeconds = 0.6f;

	/** 已经请求过退出：再给自己一点时间，仍然活着就硬退。 */
	bool bQuitRequested = false;
	float QuitFallbackTimer = 0.0f;

	FTSTicker::FDelegateHandle TickHandle;
	bool bTickRegistered = false;

	/** 全屏黑屏（视口内容，不是 UMG 控件）。 */
	TSharedPtr<SWidget> BlackScreen;

	/** 流程开始前的输入忽略状态，中断时原样恢复。 */
	bool bPrevIgnoreMove = false;
	bool bPrevIgnoreLook = false;
};