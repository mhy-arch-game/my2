// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractionPromptWidget.generated.h"

class STextBlock;

/**
 *  UInteractionPromptWidget - HUD prompt for the focused interactable.
 *
 *  Presentation hook (see Docs/StructureInteraction.md): this class defines the
 *  C++ surface only. Create a Widget Blueprint from it and implement SetPrompt
 *  to lay out the text / show-hide animation.
 *
 *  It is driven by UInteractionPromptComponent, which listens to the player's
 *  UInteractionDetectorComponent::OnFocusChanged - no Blueprint wiring needed.
 */
UCLASS(abstract)
class UInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Update the prompt text and visibility. */
	UFUNCTION(BlueprintImplementableEvent, Category="Interaction")
	void SetPrompt(const FText& PromptText, bool bVisible);
};

/**
 *  UInteractionPromptFallbackWidget - 开箱即用的弹框。
 *
 *  不依赖任何 Widget 蓝图：直接用 Slate 画一个"半透明底框 + 居中文字"的弹框，
 *  由 UInteractionPromptComponent 在没有指定 WidgetClass 时自动使用，
 *  保证"聚焦到有提示文字的可交互物就一定看得见提示"。
 *
 *  想换成自己的样式：做一个继承 UInteractionPromptWidget 的 Widget 蓝图，
 *  实现 SetPrompt 事件（设置 TextBlock 的文字 + 显隐），再把它填到
 *  UInteractionPromptComponent::WidgetClass 即可（此时本兜底不再生效）。
 */
UCLASS()
class UInteractionPromptFallbackWidget : public UInteractionPromptWidget
{
	GENERATED_BODY()

public:

	/** C++ 侧的表现更新（组件会同时调用它和蓝图的 SetPrompt 事件）。 */
	void ApplyPrompt(const FText& PromptText, bool bVisible);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	TSharedPtr<STextBlock> TextSlate;

	FText PendingText;
	bool bPendingVisible = false;
};
