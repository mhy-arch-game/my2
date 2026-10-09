// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionOperationTypes.generated.h"

class AActor;

/**
 *  FInteractionOperation - 一次"抽象操作"的载荷。
 *
 *  设计意图：把"开门 / 开灯 / 移动到正确区域"这类具体行为**抽掉**，只保留一个操作名 +
 *  一小撮通用参数。发送方（可交互物）只负责"我要发一个什么操作"，接收方自己决定怎么实现。
 *
 *  约定俗成的操作名（不是枚举，方便自定义扩展）：
 *      "Open"      打开（门开、灯亮、机关启动）
 *      "Close"     关闭
 *      "Toggle"    取反
 *      "On" / "Off" 等价于 Open / Close，语义更贴近灯
 *      "MoveTo"    移动到 Location（或 Target 的位置）
 *      "Activate" / "Deactivate"  通用启用/停用
 *  名字可以任意自定义，只要收发双方说好。
 */
USTRUCT(BlueprintType)
struct FInteractionOperation
{
	GENERATED_BODY()

	/** 抽象操作名（见类注释里的约定）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FName Operation = NAME_None;

	/** 发起交互的角色（通常是玩家 Pawn）。 */
	UPROPERTY(BlueprintReadWrite, Category="Interaction Operation")
	TObjectPtr<AActor> Instigator = nullptr;

	/** 发起这次操作的可交互物本身（箱子 / 灯泡 / 门）。 */
	UPROPERTY(BlueprintReadWrite, Category="Interaction Operation")
	TObjectPtr<AActor> Source = nullptr;

	/** 可选的目标对象，给 "MoveTo"/"AttachTo" 这类需要参照物的操作用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	TObjectPtr<AActor> Target = nullptr;

	/** 通用数值：强度 / 高度 / 速度 / 时长，由收发双方约定。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	float Value = 0.0f;

	/** 通用开关位：给自定义语义用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	bool bActive = true;

	/** 通用落点：给 "MoveTo" 这类操作用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FVector Location = FVector::ZeroVector;
};

/**
 *  接收组件默认动作：把操作翻译成对 owner 的什么改变。
 *  这些动作全部复用既有的 UInteractableComponent（内置开关 + 灯光开关），
 *  所以"开门"和"开灯"是同一套状态在驱动的——这正是"基于原先 interact 内容"。
 */
UENUM(BlueprintType)
enum class EInteractionOperationAction : uint8
{
	/** 交给 owner 的 InteractableComponent::SetOpen(true)：门开、灯亮。 */
	SetOpen			UMETA(DisplayName="Set Open (开 / 亮)"),

	/** InteractableComponent::SetOpen(false)：门关、灯灭。 */
	SetClosed		UMETA(DisplayName="Set Closed (关 / 灭)"),

	/** InteractableComponent::SetOpen(!IsOpen())：取反。 */
	ToggleOpen		UMETA(DisplayName="Toggle Open (取反)"),

	/** 按操作里的 bActive 隐藏 / 显示 owner。 */
	SetActorHidden	UMETA(DisplayName="Set Actor Hidden (显隐)"),

	/** 把 owner 移动到操作里的 Location（Value > 0 时用 Value 秒插值过去）。 */
	MoveTo			UMETA(DisplayName="Move To (移动落点)"),

	/** 什么都不做，仅用于占位 / 只走蓝图事件。 */
	Nothing			UMETA(DisplayName="Nothing (只走蓝图)")
};
