// Copyright Epic Games, Inc. All Rights Reserved.

#include "BackgroundMusicSettings.h"

UBackgroundMusicSettings::UBackgroundMusicSettings()
{
	// 默认值在这里给出，实际数值请改 Config/DefaultGame.ini 或在 Project Settings 里改。
	Volume = 1.0f;
	Speed = 1.0f;
	bAutoPlay = true;
	bLoop = true;
}
