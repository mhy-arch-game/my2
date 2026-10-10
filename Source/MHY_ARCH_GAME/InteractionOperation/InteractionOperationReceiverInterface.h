// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionOperationTypes.h"
#include "InteractionOperationReceiverInterface.generated.h"

/**
 *  IInteractionOperationReceiver - "接受一个抽象操作"的契约。
 *
 *  这是本功能**唯一面向可交互物/被操作物的接口**：
 *      [可交互物 A]  --interact-->  UInteractionLinkComponent
 *                                        |  发出 FInteractionOperation
 *                                        v
 *      [被操作物 B]  IInteractionOperationReceiver::ApplyInteractionOperation
 *                                        |
 *                             B 自己实现：开门 / 开灯 / 移动进区域 / …
 *
 *  两种实现方式（与 UInteractionDetectorComponent 的做法一致，接口或组件任选其一）：
 *    1. **蓝图实现本接口**（已声明 Blueprintable）→ 在事件图里接 Apply Interaction Operation；
 *    2. **挂 UInteractionOperationReceiverComponent** → 用"操作名 → 动作"的映射表零蓝图联动，
 *       动作直接复用 UInteractableComponent 的内置开关与灯光。
 *
 *  寻址方式有两种，可以混用：
 *    - 显式引用：发送方直接在条目里填 Targets；
 *    - 频道：接收方返回一个 Channel 名，发送方按同名频道广播（适合关卡里相距很远、
 *      或者运行时才生成的物体）。
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractionOperationReceiver : public UInterface
{
	GENERATED_BODY()
};

class IInteractionOperationReceiver
{
	GENERATED_BODY()

public:

	/** 本接收方的频道名。返回 NAME_None 表示"只能被显式引用"。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction Operation")
	FName GetOperationChannel();
	virtual FName GetOperationChannel_Implementation() { return NAME_None; }

	/** 是否接受这次操作（可以在这里做"锁住/条件未满足"之类的拦截）。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction Operation")
	bool CanReceiveOperation(const FInteractionOperation& Operation);
	virtual bool CanReceiveOperation_Implementation(const FInteractionOperation& Operation) { return true; }

	/** 执行操作。接收方自己决定怎么实现。 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction Operation")
	void ApplyInteractionOperation(const FInteractionOperation& Operation);
	virtual void ApplyInteractionOperation_Implementation(const FInteractionOperation& Operation) {}
};
