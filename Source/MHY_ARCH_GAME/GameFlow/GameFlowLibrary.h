// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFlowLibrary.generated.h"

class UEndGameSubsystem;

/**
 *  UGameFlowLibrary —— 「结束游戏」的蓝图入口（一个节点就能用）。
 *
 *  对外的正式接口就是下面这个 EndGameWithFade；真正的流程在 UEndGameSubsystem 里。
 */
UCLASS()
class UGameFlowLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 【结束游戏接口】渐黑 -> 保持全黑 -> 结束游戏进程。
	 * 蓝图里搜 "End Game With Fade" 直接拖出来即可；重复调用无效。
	 */
	UFUNCTION(BlueprintCallable, Category="MHY|Game Flow",
		meta=(WorldContext="WorldContextObject", AdvancedDisplay="FadeOutSeconds,BlackHoldSeconds"))
	static void EndGameWithFade(const UObject* WorldContextObject,
		float FadeOutSeconds = 1.2f, float BlackHoldSeconds = 0.6f);

	/** 是否已经在结束流程里。 */
	UFUNCTION(BlueprintPure, Category="MHY|Game Flow", meta=(WorldContext="WorldContextObject"))
	static bool IsEndingGame(const UObject* WorldContextObject);

	/** 取结束流程子系统（想自己绑 OnEndGameSequenceStarted 时用）。 */
	UFUNCTION(BlueprintPure, Category="MHY|Game Flow", meta=(WorldContext="WorldContextObject"))
	static UEndGameSubsystem* GetEndGameSubsystem(const UObject* WorldContextObject);
};