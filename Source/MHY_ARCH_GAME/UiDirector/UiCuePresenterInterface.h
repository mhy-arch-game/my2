// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UiCueTypes.h"
#include "UiCuePresenterInterface.generated.h"

class USoundBase;

/**
 *  IUiCuePresenter —— UI 活动接口（"在摄像机视角触发的 UI 事件"）。
 *
 *  任意 Actor 实现它（C++ 或蓝图）即可接收导演模块派发的 UI 活动：
 *    OnUiCueBegin   -> 元事件开始（可在这里播放配音、准备动效）
 *    OnUiCueSegment -> 显示一条小句（长文本已按断句符切好，依次推送）
 *    OnUiCueEnd     -> 元事件结束（收起字幕）
 *
 *  没有实现者时，模块会退化用内置的屏幕字幕控件（透明底白字）+ 自己播配音。
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UUiCuePresenter : public UInterface
{
	GENERATED_BODY()
};

class IUiCuePresenter
{
	GENERATED_BODY()

public:
	/** 元事件开始（Voice 可为空）。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ui Cue")
	void OnUiCueBegin(FName CueId, const TSoftObjectPtr<USoundBase>& Voice);
	virtual void OnUiCueBegin_Implementation(FName CueId, const TSoftObjectPtr<USoundBase>& Voice) {}

	/** 显示一条小句；持续 Segment.DurationSeconds 秒后模块会推下一条或收尾。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ui Cue")
	void OnUiCueSegment(const FUiCueSegment& Segment);
	virtual void OnUiCueSegment_Implementation(const FUiCueSegment& Segment) {}

	/** 元事件结束（收起字幕）。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ui Cue")
	void OnUiCueEnd(FName CueId);
	virtual void OnUiCueEnd_Implementation(FName CueId) {}
};
