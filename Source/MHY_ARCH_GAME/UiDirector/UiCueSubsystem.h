// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UiCueTypes.h"
#include "UiCueSubsystem.generated.h"

class AUiCueTriggerVolume;
class UAudioComponent;
class UInteractableComponent;
class UUiCueSet;
class UUiCueSubtitleWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUiCueBegan, FName, CueId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUiCueSegmentShown, FUiCueSegment, Segment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUiCueEnded, FName, CueId);

/**
 *  UUiCueSubsystem —— UI 活动导演（全局唯一，模块自驱）。
 *
 *  职责：
 *    1. **自驱**判断触发：每帧检查关卡里的 AUiCueTriggerVolume，玩家进入盒子就命中
 *       （命中后该体积**永久失效**）；
 *    2. 把命中的 cue 放进**事件队列**；一条播完、等 QueueGapSeconds（默认 3 秒，
 *       在 DataAsset 详情里改）再播下一条；
 *    3. 一条 cue 内的**长文本按断句符切成小句**，按次序推送；
 *    4. 通过 IUiCuePresenter 接口把 UI 活动派发给摄像机视角的表现层
 *       （没有实现者时退化用内置屏幕字幕 + 自己播配音）。
 *
 *  生命周期以**元事件（cue）**为主：字幕速度与配音速度不绑死。
 *  触发源：盒体体积 / 首次交互 / 进入游戏 / 首次靠近某个 Actor；
 *  外部系统也可以调 TriggerCueById() 手动触发。
 */
UCLASS(config=Game)
class UUiCueSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 从任意世界上下文取本子系统。 */
	UFUNCTION(BlueprintPure, Category="Ui Cue", meta=(WorldContext="WorldContextObject"))
	static UUiCueSubsystem* Get(const UObject* WorldContextObject);

	/** cue 表（DataAsset）。留空时自动找工程里唯一的一份 UUiCueSet。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	TSoftObjectPtr<UUiCueSet> CueSet;

	/** 手动 / 外部触发一条 cue（触发源以后扩展时用它）。返回是否被接受。 */
	UFUNCTION(BlueprintCallable, Category="Ui Cue")
	bool TriggerCueById(FName CueId);

	/** 清空队列（不影响正在播放的那条）。 */
	UFUNCTION(BlueprintCallable, Category="Ui Cue")
	void ClearQueue();

	/** 立刻结束当前 cue 并进入间隔。 */
	UFUNCTION(BlueprintCallable, Category="Ui Cue")
	void SkipCurrentCue();

	UFUNCTION(BlueprintPure, Category="Ui Cue")
	bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintPure, Category="Ui Cue")
	int32 GetQueuedCount() const { return Queue.Num(); }

	/** 当前单条信息，便于调试显示。 */
	UFUNCTION(BlueprintPure, Category="Ui Cue")
	FString GetUiCueDebugString() const;

	UPROPERTY(BlueprintAssignable, Category="Ui Cue")
	FOnUiCueBegan OnCueBegan;

	UPROPERTY(BlueprintAssignable, Category="Ui Cue")
	FOnUiCueSegmentShown OnCueSegmentShown;

	UPROPERTY(BlueprintAssignable, Category="Ui Cue")
	FOnUiCueEnded OnCueEnded;

protected:
	/** 核心 ticker 回调（模块自驱）。 */
	bool HandleTick(float DeltaTime);

private:
	/** 当前 cue 与队列。 */
	bool bPlaying = false;
	FUiCue CurrentCue;
	TArray<FUiCue> Queue;

	/** 已经触发过的 cue（bOnce 用；模块是全局的，所以本会话内永久失效）。 */
	TSet<FName> FiredCueIds;

	/** 已经绑过交互事件的可交互物（避免重复绑定）。 */
	TSet<TWeakObjectPtr<UInteractableComponent>> BoundInteractables;

	/** 已经"首次交互"过的可交互物（本会话内只触发一次）。 */
	TSet<TWeakObjectPtr<UInteractableComponent>> FiredInteractables;

	/** 开场（GameStart）是否已经发过 —— 只发一次。 */
	bool bGameStartDone = false;

	/** 按 id 找 Actor 的缓存（靠近触发每帧都要用，避免每帧遍历关卡）。 */
	TMap<FName, TWeakObjectPtr<AActor>> ActorIdCache;

	/** 绑定扫描的节流计时（秒）。 */
	float InteractBindTimer = 0.0f;

	/** 当前 cue 的小句。 */
	TArray<FString> Segments;
	int32 SegmentIndex = 0;
	float SegmentTimer = 0.0f;

	/**
	 * 当前 cue 每条小句的时长（秒）。有配音时：
	 *   配音段数 == 小句数 -> 每句就用它自己那段配音的时长（最准）；
	 *   否则            -> 配音总时长 / 小句数，保证字幕总流程与配音总流程一致。
	 */
	TArray<float> SegmentSeconds;
	float CueSegmentSeconds = 0.0f;

	/** 当前 cue 的配音队列（按顺序连播）。 */
	TArray<TSoftObjectPtr<USoundBase>> PendingVoices;
	int32 PendingVoiceIndex = 0;

	/** 第一段配音（转发给表现层用）。 */
	TSoftObjectPtr<USoundBase> PrimaryVoice;

	/** 两条 cue 之间的间隔倒计时。 */
	bool bWaitingGap = false;
	float GapTimer = 0.0f;

	FTSTicker::FDelegateHandle TickHandle;
	bool bTickRegistered = false;
	bool bTriedAutoFind = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UUiCueSubtitleWidget> SubtitleWidget;

	/** 本帧选定的表现层（实现了 IUiCuePresenter 的 Actor，可为空 = 用内置字幕）。 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PresenterActor;

	UUiCueSet* ResolveCueSet();

	/** 检查关卡里的触发体积（玩家进入 -> 入队 + 永久失效）。 */
	void CheckTriggerVolumes();

	/** 命中的体积 -> 找到匹配 cue 并入队。 */
	bool EnqueueCueForVolume(AUiCueTriggerVolume* Volume);

	/** 按 TriggerId + 触发方式找 cue 并入队（两种触发方式共用）。 */
	bool EnqueueCueByTriggerId(FName TriggerId, EUiCueTrigger Kind, const FString& SourceLabel);

	/** 周期性把关卡里所有可交互物的 OnToggleChanged 绑上（只绑一次）。 */
	void BindInteractTriggers();

	/** 可交互物状态变化回调（首次交互才算有效）。 */
	UFUNCTION()
	void HandleInteractableToggled(AActor* Interactable, bool bIsOpen);

	/** 可交互物"按下交互键"回调（首次交互才算有效）。 */
	UFUNCTION()
	void HandleInteractRequested(AActor* Interactor, AActor* Interactable);

	/** 两路交互信号共用的"首次交互"处理。 */
	void HandleFirstInteract(UInteractableComponent* Component, const FString& Reason);

	/** 开场触发：本地玩家就绪后把所有 GameStart 的 cue 入队（只做一次）。 */
	void CheckGameStartCues();

	/** 靠近触发：玩家到目标 Actor 包围盒最近点 ≤ ProximityDistance 就入队。 */
	void CheckProximityCues();

	/** 按名字 / 标签找关卡里的 Actor（带缓存）。 */
	AActor* ResolveActorById(FName Id, UWorld* World);

	/** Actor 的名字 / 标签是否等于给定的 id。 */
	static bool MatchesActorId(FName TriggerId, const AActor* Actor);

	void StartCue(const FUiCue& Cue);
	void ShowSegment(int32 Index);
	void AdvanceSegment();
	void FinishCue();

	/** 按断句符切句（标点保留在小句末尾）。 */
	void SplitTextIntoSegments(const FString& InText, const FString& Delimiters, TArray<FString>& OutSegments) const;

	/** 找实现接口的表现层（每帧最多找一个）。 */
	AActor* FindPresenter() const;

	/** 内置屏幕字幕（没有表现层时）。 */
	void EnsureSubtitleWidget();
	class APlayerController* GetLocalPlayerController() const;

	/** 播一条 cue 的配音队列（按顺序连播，不重叠；按需求不切断）。 */
	void PlayVoices(const TArray<TSoftObjectPtr<USoundBase>>& Voices);
	/** 播队列里的下一段。 */
	void PlayNextVoice();
	/** 一段配音播完 -> 继续下一段。 */
	UFUNCTION()
	void HandleVoiceFinished();
	/** 仅用于模块销毁时清理。 */
	void StopVoice();
};
