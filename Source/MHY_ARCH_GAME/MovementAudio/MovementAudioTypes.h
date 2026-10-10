// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "MovementAudioTypes.generated.h"

class USoundBase;

/**
 *  Movement events that can carry audio.
 */
UENUM(BlueprintType)
enum class EMovementAudioEvent : uint8
{
	/** A footstep (timing normally comes from an AnimNotify). */
	Footstep	UMETA(DisplayName = "Footstep"),

	/** Leaving the ground. */
	Jump		UMETA(DisplayName = "Jump"),

	/** Touching the ground again. */
	Land		UMETA(DisplayName = "Land")
};

/**
 *  One audio set: the sounds used for a given ground surface.
 *
 *  Every slot is a soft reference and may stay empty - the component then simply
 *  broadcasts the event without playing anything, so concrete sound assets can be
 *  added later (or the events can be routed to Wwise / MetaSounds) without touching
 *  any code.
 */
USTRUCT(BlueprintType)
struct FMovementAudioSet
{
	GENERATED_BODY()

	/** Physical surface this set applies to (SurfaceType_Default = the fallback set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TEnumAsByte<EPhysicalSurface> Surface;

	/** Sound played for a footstep on this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Footstep;

	/** Sound played when jumping off this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Jump;

	/** Sound played when landing on this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Land;

	/** Picks the sound matching an event (null when that slot is empty). */
	TSoftObjectPtr<USoundBase> GetSoundForEvent(EMovementAudioEvent Event) const
	{
		switch (Event)
		{
		case EMovementAudioEvent::Jump:		return Jump;
		case EMovementAudioEvent::Land:		return Land;
		case EMovementAudioEvent::Footstep:
		default:							return Footstep;
		}
	}
};

/**
 *  角色的**持续**运动状态。
 *
 *  与 EMovementAudioEvent（一次性事件：脚步 / 起跳 / 落地）互补：
 *  那个是"点"，这个是"段"——用来在状态切换时换音乐 / 环境音。
 *
 *  优先级（从高到低）：InAir > Crouch > Sprint > Run > Walk > Idle。
 *  即：滞空时一律算 InAir；蹲着时一律算 Crouch（蹲着不可能疾跑）；否则按水平速度分档。
 */
UENUM(BlueprintType)
enum class EMovementAudioState : uint8
{
	/** 基本站立不动。 */
	Idle		UMETA(DisplayName = "Idle (站立)"),

	/** 低速移动。 */
	Walk		UMETA(DisplayName = "Walk (行走)"),

	/** 超过 RunSpeedThreshold。 */
	Run			UMETA(DisplayName = "Run (奔跑)"),

	/** 超过 SprintSpeedThreshold（配合疾跑组件时就是疾跑）。 */
	Sprint		UMETA(DisplayName = "Sprint (疾跑)"),

	/** 蹲伏中。 */
	Crouch		UMETA(DisplayName = "Crouch (蹲伏)"),

	/** 滞空（起跳 / 下落）。 */
	InAir		UMETA(DisplayName = "In Air (滞空)")
};

/**
 *  某个持续状态下要**循环**播放的音乐。
 *
 *  与 FMovementAudioSet 一样：软引用可以留空，留空 = 该状态不放音乐（只广播状态）。
 *  音乐是否循环取决于声音资源自身（SoundWave 的 Looping），本结构不管。
 */
USTRUCT(BlueprintType)
struct FMovementStateMusic
{
	GENERATED_BODY()

	/** 这个条目对应哪个运动状态。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	EMovementAudioState State = EMovementAudioState::Idle;

	/** 该状态下循环播放的音乐（软引用，可留空）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio")
	TSoftObjectPtr<USoundBase> Music;

	/** 该状态音乐的独立音量倍率（再乘以组件上的 VolumeMultiplier）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MovementAudio", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.0f;
};
