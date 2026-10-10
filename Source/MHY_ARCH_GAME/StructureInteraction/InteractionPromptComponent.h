// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionPromptComponent.generated.h"

class UInteractionDetectorComponent;
class UInteractionPromptWidget;
class UUserWidget;
class UWidgetComponent;

/** 提示弹框显示 / 隐藏时广播（可接音效、动效）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionPromptChanged, FText, PromptText, bool, bVisible);

/**
 *  UInteractionPromptComponent - 聚焦时在【交互物体前方的世界坐标处】弹出文字提示。
 *
 *  挂在玩家（角色）上，与既有交互管线**零连线**对接：
 *
 *      玩家探测器 UInteractionDetectorComponent::OnFocusChanged(FocusActor, Prompt)
 *                          |
 *                          v
 *                  本组件：有聚焦 且 提示文字非空 -> 显示提示
 *                          失去聚焦 / 文字为空   -> 隐藏提示
 *
 *  两种呈现方式：
 *    - bWorldSpacePrompt = true（默认）：用 UWidgetComponent 把 UI 放在**物体前方**的世界坐标上，
 *      每帧朝向摄像机（billboard），因此**不跟随镜头**，而是贴着物体走；
 *    - bWorldSpacePrompt = false：屏幕空间（AddToViewport，固定在屏幕中心下方）。
 *
 *  弹框样式两种来源：
 *    1. WidgetClass = 你自己的 Widget 蓝图（继承 UInteractionPromptWidget，实现 SetPrompt）；
 *    2. 留空 + bUseBuiltInFallback = 内置的 C++ 弹框（半透明底框 + 居中文字），开箱即用。
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UInteractionPromptComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UInteractionPromptComponent();

	/** 弹框的 Widget 蓝图（继承 UInteractionPromptWidget）。留空则用内置弹框。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt")
	TSubclassOf<UInteractionPromptWidget> WidgetClass;

	/** WidgetClass 留空时用内置的 C++ 弹框兜底（保证一定有提示）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt")
	bool bUseBuiltInFallback = true;

	/** 自动绑定同角色上的 UInteractionDetectorComponent。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt")
	bool bAutoBindDetector = true;

	/** 可交互物没填 InteractionPrompt 时用它（留空 = 那种物体不弹框）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt")
	FText DefaultPromptText;

	// -- 世界坐标模式（默认）------------------------------------------------

	/**
	 * true = 世界坐标模式：UI 出现在**交互物体前方的世界位置**上，并始终朝向摄像机；
	 * false = 屏幕空间模式：UI 固定在屏幕上（旧行为，用下面的 ScreenOffsetY）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World")
	bool bWorldSpacePrompt = true;

	/** 世界坐标模式下 UI 面板的绘制尺寸（像素）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World", meta=(EditCondition="bWorldSpacePrompt"))
	FVector2D WorldPromptDrawSize = FVector2D(360.0f, 120.0f);

	/** 在物体包围盒之上再抬高多少（cm）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World", meta=(EditCondition="bWorldSpacePrompt"))
	float WorldPromptHeightOffset = 30.0f;

	/** 朝玩家方向再前移多少（cm）—— 让提示浮在物体"前方"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World", meta=(EditCondition="bWorldSpacePrompt"))
	float WorldPromptFrontOffset = 40.0f;

	/** 每帧让面板转向摄像机（关掉则保持固定朝向）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World", meta=(EditCondition="bWorldSpacePrompt"))
	bool bWorldPromptFaceCamera = true;

	/**
	 * 朝向摄像机的偏航补偿：若显示出来的文字是镜像 / 背对镜头，把它从 180 改成 0。
	 * （Widget 面板的正面默认与 +X 相反，所以默认值取 180。）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt|World", meta=(EditCondition="bWorldSpacePrompt && bWorldPromptFaceCamera"))
	float WorldPromptFacingYaw = 180.0f;

	// -- 屏幕空间模式的参数 -------------------------------------------------

	/** 屏幕空间模式下弹框的纵向偏移（像素，正数 = 屏幕中心往下）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt", meta=(EditCondition="!bWorldSpacePrompt"))
	float ScreenOffsetY = 220.0f;

	/** 弹框的 ZOrder（仅屏幕空间模式使用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Prompt", meta=(EditCondition="!bWorldSpacePrompt"))
	int32 ZOrder = 100;

	/** 显示 / 隐藏时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Interaction|Prompt")
	FOnInteractionPromptChanged OnPromptChanged;

	/** 显示提示（蓝图也可直接调）。 */
	UFUNCTION(BlueprintCallable, Category="Interaction|Prompt")
	void ShowPrompt(const FText& PromptText);

	/** 隐藏提示。 */
	UFUNCTION(BlueprintCallable, Category="Interaction|Prompt")
	void HidePrompt();

	UFUNCTION(BlueprintPure, Category="Interaction|Prompt")
	bool IsPromptVisible() const { return bPromptVisible; }

	UFUNCTION(BlueprintPure, Category="Interaction|Prompt")
	FText GetPromptText() const { return CurrentPrompt; }

	/** 当前聚焦对象（世界坐标模式用它定位）。 */
	UFUNCTION(BlueprintPure, Category="Interaction|Prompt")
	AActor* GetFocusedTarget() const { return FocusedActor; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	void HandleFocusChanged(AActor* FocusActor, FText Prompt);

private:

	/** 屏幕空间模式下的弹框。 */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ScreenWidget;

	/** 世界坐标模式下的弹框载体。 */
	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> PromptWidgetComponent;

	UPROPERTY(Transient)
	TObjectPtr<UInteractionDetectorComponent> Detector;

	/** 当前聚焦对象（世界坐标定位用）。 */
	UPROPERTY(Transient)
	TObjectPtr<AActor> FocusedActor;

	bool bPromptVisible = false;
	FText CurrentPrompt;

	/** 解析要用的 Widget 类（显式指定优先，其次内置兜底）。 */
	TSubclassOf<UInteractionPromptWidget> ResolveWidgetClass() const;

	/** 惰性创建弹框载体。 */
	void EnsurePrompt();

	/** 当前正在使用的 UUserWidget（两种模式统一入口，可能为空）。 */
	UUserWidget* GetPromptUserWidget() const;

	/** 把当前文字 / 显隐推给弹框（蓝图事件 + 内置弹框都走一遍）。 */
	void ApplyToWidget(const FText& PromptText, bool bVisible);

	/** 世界坐标模式：把面板摆到物体前方并转向摄像机。 */
	void UpdateWorldPromptTransform();
};
