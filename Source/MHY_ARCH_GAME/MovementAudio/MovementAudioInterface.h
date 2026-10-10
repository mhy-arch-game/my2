// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MovementAudioTypes.h"
#include "MovementAudioInterface.generated.h"

class AActor;

/**
 *  IMovementAudioInterface - "角色运动状态变了"的订阅契约。
 *
 *  这是本功能**唯一面向外部系统的接口**，让"谁在听运动状态"与"谁在产生运动"解耦：
 *
 *      角色上的 UMovementAudioComponent
 *            │  状态变化（Idle / Walk / Run / Sprint / Crouch / InAir）
 *            v
 *      任何实现了 IMovementAudioInterface 的旁观者
 *            │
 *            v
 *      它自己决定放什么音乐
 *
 *  两种接法（与项目里其它接口一致：接口 or 委托，任选）：
 *    1. **蓝图实现本接口**：Class Settings → Interfaces → 加 "Movement Audio"，
 *       然后在 On Movement Audio State Changed 事件里按状态切音乐；
 *    2. **不实现接口**：直接在角色的 MovementAudio 组件上订阅
 *       On Movement State Changed 委托（同一事件，但耦合到那个具体角色实例）。
 *
 *  典型实现者：全局音乐导演（MusicDirector）、区域环境音管理器、UI / 演出控制器。
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMovementAudioInterface : public UInterface
{
	GENERATED_BODY()
};

class IMovementAudioInterface
{
	GENERATED_BODY()

public:

	/** 是否关心该角色的运动音频。默认 true；想只听特定角色就在这里判断。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MovementAudio")
	bool CanReceiveMovementAudio(AActor* Character);
	virtual bool CanReceiveMovementAudio_Implementation(AActor* Character) { return true; }

	/**
	 * 运动状态发生变化。
	 * 实现者在这里切换到 NewState 对应的音乐；PreviousState 可用于做过渡 / 淡出。
	 * 注意：第一次进入状态时也会调用一次（此时 PreviousState == NewState），
	 * 这样旁观者不用自己等"第一次变化"就能起播正确的音乐。
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MovementAudio")
	void OnMovementAudioStateChanged(EMovementAudioState NewState, EMovementAudioState PreviousState, AActor* Character);
	virtual void OnMovementAudioStateChanged_Implementation(EMovementAudioState NewState, EMovementAudioState PreviousState, AActor* Character) {}
};
