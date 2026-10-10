// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BackgroundMusicSettings.generated.h"

class USoundBase;

/**
 *  UBackgroundMusicSettings —— 背景音乐的全局设置（Project Settings 里直接改）。
 *
 *  位置：Project Settings -> Game -> Background Music
 *  暴露两个属性：**音量**（Volume）与**倍速**（Speed，即播放速率/音高倍率）。
 *
 *  数值存进 Config/DefaultGame.ini 的 [/Script/MHY_ARCH_GAME.BackgroundMusicSettings] 段，
 *  所以不进代码、可随工程提交；运行时也可以由蓝图调
 *  UBackgroundMusicSubsystem::SetVolume / SetSpeed 临时改。
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Background Music"))
class UBackgroundMusicSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBackgroundMusicSettings();

	/** 背景音乐资源（SoundWave / SoundCue 均可；建议勾 Loop）。 */
	UPROPERTY(EditAnywhere, config, Category="Background Music")
	TSoftObjectPtr<USoundBase> Music;

	/** 音量倍率（1 = 原始音量）。 */
	UPROPERTY(EditAnywhere, config, Category="Background Music", meta=(ClampMin="0.0", ClampMax="4.0"))
	float Volume = 1.0f;

	/** 倍速：1 = 原速，2 = 两倍速（同时会把音高提高一倍）。 */
	UPROPERTY(EditAnywhere, config, Category="Background Music", meta=(ClampMin="0.05", ClampMax="4.0"))
	float Speed = 1.0f;

	/** 进入游戏就自动播放。 */
	UPROPERTY(EditAnywhere, config, Category="Background Music")
	bool bAutoPlay = true;

	/** 循环播放（资源本身没勾 Loop 时由模块兜底重播）。 */
	UPROPERTY(EditAnywhere, config, Category="Background Music")
	bool bLoop = true;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
