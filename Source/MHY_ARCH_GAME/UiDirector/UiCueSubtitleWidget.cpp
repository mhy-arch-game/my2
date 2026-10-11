// Copyright Epic Games, Inc. All Rights Reserved.

#include "UiCueSubtitleWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UUiCueSubtitleWidget::RebuildWidget()
{
	// Screen subtitle: no background, white text with a dark shadow so it stays readable
	// over any scene (same look as the interaction prompt's built-in popup).
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(1100.0f)
			[
				SAssignNew(TextSlate, STextBlock)
				.Text(PendingText)
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.ShadowOffset(FVector2D(1.5f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
			]
		];
}

void UUiCueSubtitleWidget::SetCueText(const FText& InText)
{
	PendingText = InText;

	if (TextSlate.IsValid())
	{
		TextSlate->SetText(InText);
	}

	SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
