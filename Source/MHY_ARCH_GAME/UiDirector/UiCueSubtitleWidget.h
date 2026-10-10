// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UiCueSubtitleWidget.generated.h"

class STextBlock;

/**
 *  UUiCueSubtitleWidget —— 模块内置的**屏幕字幕**兜底控件。
 *
 *  不依赖任何 Widget 蓝图：直接用 Slate 画"透明底 + 白字 + 深色投影"的居中字幕，
 *  由 UUiCueSubsystem 在没有蓝图实现 IUiCuePresenter 时自动使用。
 *
 *  想换成自己的样式：做一个实现 IUiCuePresenter 的 Actor（例如放在关卡里的 HUD 管理器），
 *  实现三个事件即可；此时内置字幕不再出现。
 */
UCLASS()
class UUiCueSubtitleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 更新字幕文字（空文字 = 收起）。 */
	void SetCueText(const FText& InText);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<STextBlock> TextSlate;
	FText PendingText;
};
