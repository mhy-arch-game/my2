// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UiCueTypes.generated.h"

class USoundBase;

/**
 *  触发源类型。
 *  目前只实现"盒体体积"，其余（时间轴 / 交互后 / 序列播完…）按需求留扩展位，暂不新增。
 */
UENUM(BlueprintType)
enum class EUiCueTrigger : uint8
{
	/** 玩家进入关卡里摆放的盒体触发体积（触发后永久失效）。 */
	BoxVolume			UMETA(DisplayName="Box Volume (盒体体积)"),

	/**
	 * 某个**可交互物**（复用 interact 模块：挂 UInteractableComponent 的物体，
	 * 例如门 / 灯）**首次**切换开 / 关状态时触发一次，之后不再触发。
	 * TriggerId 填该物体在关卡里的名字（或标签），例如 men1。
	 */
	InteractFirstToggle	UMETA(DisplayName="Interactable First Toggle (首次切换状态)")
};

/**
 *  FUiCue —— 一条"元事件"。
 *
 *  它是本模块的生命周期主体：一条 cue 内部可以是一段**长文本**，
 *  播放时会被按断句符切成小句**按次序**播放；配音与字幕速度不绑死。
 */
USTRUCT(BlueprintType)
struct FUiCue
{
	GENERATED_BODY()

	/** 唯一 id（也是队列/日志里显示的名字）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	FName CueId = NAME_None;

	/** 触发方式。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	EUiCueTrigger Trigger = EUiCueTrigger::BoxVolume;

	/**
	 * 触发目标的 id：
	 *   BoxVolume           -> 关卡里 AUiCueTriggerVolume 的 TriggerId
	 *   InteractFirstToggle -> 可交互物在关卡里的名字 / 标签（如 men1）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	FName TriggerId = NAME_None;

	/** 长文本：会按 CueSet 的 SegmentDelimiters 切成小句，按次序播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue", meta=(MultiLine=true))
	FText Text;

	/** 配音（整条 cue 开始时播一次；与字幕速度不绑死）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	TSoftObjectPtr<USoundBase> Voice;

	/** 每条小句的显示时长（秒）；<= 0 时用 CueSet 的 DefaultSegmentSeconds。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue", meta=(ClampMin="0.0"))
	float SegmentSecondsOverride = 0.0f;

	/** 触发一次后永久失效（默认开）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	bool bOnce = true;
};

/** 传给表现层的单条小句。 */
USTRUCT(BlueprintType)
struct FUiCueSegment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Ui Cue")
	FName CueId = NAME_None;

	/** 这条小句的文字。 */
	UPROPERTY(BlueprintReadOnly, Category="Ui Cue")
	FText Text;

	/** 第几条（从 0 开始）/ 共几条。 */
	UPROPERTY(BlueprintReadOnly, Category="Ui Cue")
	int32 Index = 0;

	UPROPERTY(BlueprintReadOnly, Category="Ui Cue")
	int32 Count = 1;

	/** 该小句的显示时长（秒）。 */
	UPROPERTY(BlueprintReadOnly, Category="Ui Cue")
	float DurationSeconds = 0.0f;
};

/**
 *  UUiCueSet —— 全局唯一的 cue 表（DataAsset）。
 *
 *  放置：内容浏览器里新建一个 UiCueSet 资产（例如 /Game/MHY_ARCH_GAME/UI/DA_UiCueSet），
 *  模块会自动加载工程里这一份；所有"全局统一"的节奏参数都在它的**详情面板**里改，不写死在代码里。
 */
UCLASS(BlueprintType)
class UUiCueSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 上一个"元事件"播放完成后，等待多少秒再播下一条（默认 3 秒）。
	 * 队列里有多条 cue 时按这个间隔依次播放。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set", meta=(ClampMin="0.0"))
	float QueueGapSeconds = 3.0f;

	/** 每条小句的默认显示时长（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set", meta=(ClampMin="0.0"))
	float DefaultSegmentSeconds = 2.5f;

	/** 断句符：其中**每个字符**都算一个断点（标点保留在小句末尾）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set")
	FString SegmentDelimiters = TEXT("。！？!?.;；\n");

	/** 字幕是否由模块内置的兜底控件显示（没有蓝图实现接口时用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set")
	bool bUseBuiltInSubtitle = true;

	/** 内置屏幕字幕离屏幕底边的像素距离。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set", meta=(ClampMin="0.0"))
	float SubtitleBottomOffset = 140.0f;

	/**
	 * 配音由模块播放（默认开）。
	 * 一条 cue = 一段长文本 + 一段长音频；模块用音频时长决定字幕总流程，
	 * 并负责把它播完（短句切换与 cue 结束都**不会**切断它）。
	 * 若你的表现层（实现 IUiCuePresenter 的 Actor）自己播配音，把这里关掉，避免播两遍。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set")
	bool bModulePlaysVoice = true;

	/** cue 列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue Set")
	TArray<FUiCue> Cues;
};
