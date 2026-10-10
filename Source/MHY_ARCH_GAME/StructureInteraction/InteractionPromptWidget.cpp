// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionPromptWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UInteractionPromptFallbackWidget::RebuildWidget()
{
	// A native Slate popup - built here instead of in a Widget Blueprint so the prompt
	// works with zero content: a translucent rounded box with the prompt text on it.
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(24.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			// 透明底：不画任何底色（原来这里是 0.7 的黑底方框）。
			.BorderBackgroundColor(FLinearColor::Transparent)
			.Padding(FMargin(22.0f, 10.0f))
			[
				SAssignNew(TextSlate, STextBlock)
				.Text(PendingText)
				// 白字；亮背景上靠深色投影保持可读（投影不是底色，仍然"透明底"）。
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.ShadowOffset(FVector2D(1.5f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
			]
		];
}

void UInteractionPromptFallbackWidget::ApplyPrompt(const FText& PromptText, bool bVisible)
{
	PendingText = PromptText;
	bPendingVisible = bVisible;

	if (TextSlate.IsValid())
	{
		TextSlate->SetText(PromptText);
	}

	// Toggle the whole user widget: the Slate tree is built once, showing/hiding is free.
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
