// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MovementAudioTypes.h"
#include "MovementAudioComponent.generated.h"

class ACharacter;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** A footstep happened. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementFootstep, FName, SurfaceName, bool, bRunning);

/** A jump started. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementJump, FName, SurfaceName);

/** A landing happened (FallSpeed is the peak falling speed, for light/heavy grading). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementLand, FName, SurfaceName, float, FallSpeed);

/** Walk <-> run changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMovementRunStateChanged, bool, bRunning);

/** 持续运动状态变化（切音乐 / 动画 / UI 都用它）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMovementStateChanged, EMovementAudioState, NewState, EMovementAudioState, PreviousState);

/**
 *  UMovementAudioComponent - the movement audio hub attached to a character.
 *
 *  It is an INTERFACE for movement sound, not a sound pack: every sound slot is an
 *  optional soft reference. With nothing assigned it still tracks the movement state
 *  and broadcasts events, so audio can be wired up later (in Blueprint, or to
 *  Wwise/MetaSounds) without changing any code.
 *
 *  What it tracks / exposes:
 *   - Walking vs running (horizontal speed against RunSpeedThreshold)
 *   - Jump (movement mode -> Falling with upward velocity)
 *   - Land  (LandedDelegate, carrying the peak fall speed)
 *   - Footsteps: timing should come from UAnimNotify_MovementAudio; a distance based
 *     fallback (bAutoFootstepByDistance) is available for testing before animations exist
 *   - Ground surface: a short downward trace resolves UPhysicalMaterial::SurfaceType,
 *     which selects the matching FMovementAudioSet (fallback = DefaultSet)
 *
 *  Playback policy: if the slot for the resolved surface has a sound, it is played at
 *  the character's location; otherwise only the event is broadcast.
 */
UCLASS(ClassGroup=(Audio), meta=(BlueprintSpawnableComponent))
class UMovementAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementAudioComponent();

	// -- events (subscribe from Blueprint / audio systems) ------------------

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementFootstep OnFootstep;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementJump OnJump;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementLand OnLand;

	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementRunStateChanged OnRunStateChanged;

	/** 持续运动状态变化时广播（第一次进入状态也会广播一次，便于立即起播音乐）。 */
	UPROPERTY(BlueprintAssignable, Category="MovementAudio")
	FOnMovementStateChanged OnMovementStateChanged;

	// -- triggers (called by the AnimNotify, or any external system) ---------

	/** Play/broadcast a footstep for the surface under the character. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayFootstep();

	/** Play/broadcast the jump sound. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayJump();

	/** Play/broadcast the landing sound (uses the peak falling speed of the last air time). */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayLand();

	/** Generic entry point used by the AnimNotify. */
	UFUNCTION(BlueprintCallable, Category="MovementAudio")
	void PlayMovementAudioEvent(EMovementAudioEvent Event);

	// -- queries ------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="MovementAudio")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category="MovementAudio")
	bool IsInAir() const { return bInAir; }

	/** Name of the surface under the character (empty when nothing was hit). */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	FName GetCurrentSurfaceName() const;

	/** Name of the surface at an arbitrary world location. */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	FName GetSurfaceNameAtLocation(const FVector& WorldLocation) const;

	/** 当前持续运动状态。 */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	EMovementAudioState GetMovementState() const { return MovementState; }

	/** 一行人类可读状态（状态 / 速度与三个阈值 / 蹲伏 / 音乐 / 是否派发接口），给 Print String 用。 */
	UFUNCTION(BlueprintPure, Category="MovementAudio")
	FString GetMovementAudioDebugString() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -- sounds (leave empty until assets exist) ---------------------------
	/** Fallback set used when no surface-specific set matches. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	FMovementAudioSet DefaultSet;

	/** Per-surface sets (grass, stone, metal...). Missing surfaces fall back to DefaultSet. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TArray<FMovementAudioSet> SurfaceSets;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.0f;

	/** Random pitch range applied to each sound for variation. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.01"))
	float PitchMin = 0.95f;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds", meta=(ClampMin="0.01"))
	float PitchMax = 1.05f;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, Category="MovementAudio|Sounds")
	TObjectPtr<USoundConcurrency> Concurrency;

	// -- detection ---------------------------------------------------------
	/** Detect jump / land from the character's movement automatically. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	bool bAutoDetectJumpAndLand = true;

	/** Horizontal speed above which the character counts as running. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="0.0"))
	float RunSpeedThreshold = 300.0f;

	/**
	 * Fallback footstep timing by travelled distance.
	 * Turn this off once the AnimNotify drives footsteps from the animation.
	 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	bool bAutoFootstepByDistance = false;

	/** Distance between automatic footsteps. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="1.0"))
	float FootstepDistance = 180.0f;

	/** How far below the character to look for the ground surface. */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection", meta=(ClampMin="0.0"))
	float SurfaceTraceDistance = 150.0f;

	/** Channel used by the surface trace (must be blocked by the ground). */
	UPROPERTY(EditAnywhere, Category="MovementAudio|Detection")
	TEnumAsByte<ECollisionChannel> SurfaceTraceChannel = ECC_Visibility;

	// -- 持续运动状态 / 状态音乐 ------------------------------------------
	/** 水平速度高于它算"行走"，再低算站立。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States", meta=(ClampMin="0.0"))
	float WalkSpeedThreshold = 10.0f;

	/** 水平速度高于它算"疾跑"（应介于 RunSpeedThreshold 与你的疾跑速度之间）。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States", meta=(ClampMin="0.0"))
	float SprintSpeedThreshold = 500.0f;

	/** 状态变化时**按状态循环播放音乐**。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States")
	bool bPlayMusicPerState = true;

	/** 每个状态的循环音乐（软引用，可留空 = 该状态不放音乐）。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States")
	TArray<FMovementStateMusic> StateMusic;

	/** 音乐淡入 / 淡出时长（秒）。0 = 硬切；默认值 ≈ 交叉淡化。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States", meta=(ClampMin="0.0"))
	float MusicFadeTime = 0.35f;

	/**
	 * 把状态变化派发给世界里实现了 IMovementAudioInterface 的对象。
	 * 打开后，"音乐导演"之类的旁观者不需要知道具体角色是谁。
	 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States")
	bool bDispatchToInterfaceListeners = true;

	/** 状态变化时打一行日志（排查"状态没变 / 音乐没换"用）。 */
	UPROPERTY(EditAnywhere, Category="MovementAudio|States")
	bool bLogStateChanges = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedCharacter;

	bool bRunning = false;
	bool bInAir = false;

	/** Peak falling speed observed during the current air time. */
	float PeakFallSpeed = 0.0f;

	float DistanceSinceLastFootstep = 0.0f;
	FVector LastLocation = FVector::ZeroVector;

	/** 当前持续运动状态。 */
	EMovementAudioState MovementState = EMovementAudioState::Idle;

	/**
	 * 第一次算出的状态必须**强制应用一次**，否则初始就是 Idle 时
	 * SetMovementState(Idle) 会被"没变化"挡掉，Idle 音乐永远起播不了。
	 */
	bool bMovementStateApplied = false;

	/** 正在播放的状态音乐。自动销毁，所以判定要用 IsValid。 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	/** 由速度 + 滞空 + 蹲伏推导当前状态。 */
	EMovementAudioState ComputeMovementState(float Speed2D) const;

	/** 状态变化统一入口：广播 → 换音乐 → 派发接口 → 日志。 */
	void SetMovementState(EMovementAudioState NewState);

	/** 按状态切换循环音乐（旧的淡出、新的淡入）。 */
	void UpdateStateMusic(EMovementAudioState NewState);

	/** 淡出并释放当前状态音乐。 */
	void StopStateMusic();

	/** 把状态变化派发给 IMovementAudioInterface 的实现者。 */
	void DispatchStateToInterfaceListeners(EMovementAudioState NewState, EMovementAudioState PreviousState);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	/** Refresh running / in-air state and the distance-based footstep fallback. */
	void UpdateMovementState(float DeltaTime);

	/** Find the sound set for a surface (falls back to DefaultSet). */
	const FMovementAudioSet& ResolveSet(EPhysicalSurface Surface) const;

	/** Resolve the ground surface under a location. */
	EPhysicalSurface ResolveSurfaceAtLocation(const FVector& WorldLocation) const;

	/** Play a soft sound at a location (silently does nothing when unset). */
	void PlaySoundAt(const TSoftObjectPtr<USoundBase>& Sound, const FVector& WorldLocation);
};
