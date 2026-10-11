// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BackgroundMusicSubsystem.generated.h"

class UAudioComponent;
class UBackgroundMusicSettings;
class USoundBase;

/** 音乐状态变化时广播（音量、倍速、是否在播）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBackgroundMusicChanged, float, Volume, float, Speed, bool, bPlaying);

/**
 *  UBackgroundMusicSubsystem —— 全局背景音乐（整个游戏进程循环播放）。
 *
 *  数据来源：UBackgroundMusicSettings（Project Settings -> Game -> Background Music）
 *  暴露的两个属性：**Volume（音量）** 与 **Speed（倍速）**；两者都可以在运行时由蓝图改：
 *      UBackgroundMusicSubsystem::Get(WorldContext)->SetVolume(0.6f);
 *      UBackgroundMusicSubsystem::Get(WorldContext)->SetSpeed(1.25f);
 *
 *  循环由资源自身的 Loop 决定；即使资源没勾 Loop，模块也会在播完后自动重播（bLoop）。
 *  模块挂在 GameInstance 上，所以切关卡不会中断。
 */
UCLASS()
class UBackgroundMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 从任意世界上下文取本子系统。 */
	UFUNCTION(BlueprintPure, Category="Background Music", meta=(WorldContext="WorldContextObject"))
	static UBackgroundMusicSubsystem* Get(const UObject* WorldContextObject);

	/** 开始 / 继续播放（幂等）。 */
	UFUNCTION(BlueprintCallable, Category="Background Music")
	void PlayMusic();

	/** 停止播放（停止后不再自动重播，除非再调 PlayMusic）。 */
	UFUNCTION(BlueprintCallable, Category="Background Music")
	void StopMusic();

	/** 设置音量倍率（1 = 原始音量）。 */
	UFUNCTION(BlueprintCallable, Category="Background Music")
	void SetVolume(float NewVolume);

	/** 设置倍速（1 = 原速，2 = 两倍速）。 */
	UFUNCTION(BlueprintCallable, Category="Background Music")
	void SetSpeed(float NewSpeed);

	UFUNCTION(BlueprintPure, Category="Background Music")
	float GetVolume() const { return Volume; }

	UFUNCTION(BlueprintPure, Category="Background Music")
	float GetSpeed() const { return Speed; }

	UFUNCTION(BlueprintPure, Category="Background Music")
	bool IsPlaying() const;

	/** 一行状态，给 Print String 用。 */
	UFUNCTION(BlueprintPure, Category="Background Music")
	FString GetBackgroundMusicDebugString() const;

	UPROPERTY(BlueprintAssignable, Category="Background Music")
	FOnBackgroundMusicChanged OnMusicChanged;

protected:
	bool HandleTick(float DeltaTime);

private:
	const UBackgroundMusicSettings* GetSettings() const;

	/** 懒创建 / 续播（世界还没准备好时会在下一帧再试）。 */
	void EnsurePlaying();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	/** 当前音量 / 倍速（初始值来自设置）。 */
	float Volume = 1.0f;
	float Speed = 1.0f;

	/** 用户是否希望它在播（StopMusic 会置 false，避免被自动重播拉回来）。 */
	bool bWantsToPlay = true;

	FTSTicker::FDelegateHandle TickHandle;
	bool bTickRegistered = false;
};
