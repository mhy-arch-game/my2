// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UiCueTriggerVolume.generated.h"

class UBoxComponent;

/**
 *  AUiCueTriggerVolume —— 摆在关卡里的盒体触发体积（阻挡体积形态）。
 *
 *  用法：拖进关卡 → 拉成你要的范围 → 填 TriggerId（与 DataAsset 里某条 cue 的 TriggerId 一致）。
 *  玩家（主控角色）进入这个盒子时，模块会把对应 cue 放进队列；**触发后永久失效**。
 *
 *  判定用的是**几何包含**（把玩家位置变换到盒子局部空间比对范围），
 *  所以不依赖碰撞事件，也不会干扰玩家的移动与交互射线。
 */
UCLASS(ClassGroup=(Ui), meta=(DisplayName="UI Cue Trigger Volume"))
class AUiCueTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	AUiCueTriggerVolume();

	/** 与之匹配的 cue：DataAsset 里 FUiCue::TriggerId 等于它的那条。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	FName TriggerId = NAME_None;

	/** 触发一次后本体积永久失效（默认开）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	bool bDisableAfterTrigger = true;

	/** 视口里画出范围，方便摆位（运行时不再画）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ui Cue")
	bool bDrawDebugInEditor = true;

	/** 盒体（根组件）。注意：默认用 Trigger 预设，只做判定、不阻挡玩家。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ui Cue")
	TObjectPtr<UBoxComponent> Box;

	/** WorldLocation 是否在本体积内。 */
	UFUNCTION(BlueprintPure, Category="Ui Cue")
	bool ContainsLocation(const FVector& WorldLocation) const;

	/** 是否已经触发过（永久失效后为 true）。 */
	UFUNCTION(BlueprintPure, Category="Ui Cue")
	bool IsTriggered() const { return bTriggered; }

	/** 模块在命中后调用：标记失效。 */
	UFUNCTION(BlueprintCallable, Category="Ui Cue")
	void HandleTriggered();

	/** 蓝图里改尺寸的便捷入口。 */
	UFUNCTION(BlueprintCallable, Category="Ui Cue")
	void SetVolumeExtent(FVector NewExtent);

private:
	bool bTriggered = false;
};
