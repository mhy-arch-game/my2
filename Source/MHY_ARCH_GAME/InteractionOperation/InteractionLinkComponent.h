// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionOperationTypes.h"
#include "InteractionLinkComponent.generated.h"

class AActor;
class UInteractableComponent;

/** 每发出一条操作广播一次，便于 HUD / 音效 / 调试观察。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionOperationDispatched, FName, Operation, int32, ReceiverCount);

/**
 *  FInteractionOperationEntry - "交互一次要发哪些操作、发给谁"。
 */
USTRUCT(BlueprintType)
struct FInteractionOperationEntry
{
	GENERATED_BODY()

	/** 要发出的抽象操作名（约定见 FInteractionOperation 注释）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FName Operation = TEXT("Open");

	/** 按频道寻址：发给所有 GetOperationChannel() 等于此名的接收方。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FName Channel = NAME_None;

	/** 显式目标，优先于频道；适合关卡里就直接连线的场合。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	TArray<TObjectPtr<AActor>> Targets;

	/**
	 * 跟随本源的开合状态：本源"开着"发 Operation，"关着"发 ClosedOperation。
	 * 打开它就能做出"我开，它也跟着开；我关，它也跟着关"的联动。
	 * （注意 UInteractableComponent::NotifyInteract 是先改状态再广播，所以这里读到的是新状态。）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	bool bMirrorSourceState = false;

	/** bMirrorSourceState 打开时，"关"要发的操作名。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation", meta=(EditCondition="bMirrorSourceState"))
	FName ClosedOperation = TEXT("Close");

	/** 随操作带过去的落点（给 MoveTo 用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FVector Location = FVector::ZeroVector;

	/** 随操作带过去的数值（强度/时长/高度…）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	float Value = 0.0f;

	/** 随操作带过去的目标对象。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	TObjectPtr<AActor> Target = nullptr;
};

/**
 *  UInteractionLinkComponent - 把"一次交互"翻译成"一批抽象操作"发给别的物品。
 *
 *  挂在本源可交互物（箱子 / 灯泡 / 门）上，与既有的交互管线自动对接：
 *  BeginPlay 时找到（没有就建一个）owner 的 UInteractableComponent 并绑定
 *  OnInteractRequested，于是"聚焦 + 按 E"就触发一次广播，**不需要任何蓝图连线**。
 *
 *      玩家按 E --> UInteractableComponent::NotifyInteract
 *                      |（先改自身开关状态，再广播）
 *                      v
 *                 本组件 HandleInteractRequested
 *                      |  遍历 Entries，构造 FInteractionOperation
 *                      v
 *              显式 Targets + 频道扫描 -> 每个接收方
 *                      |  IInteractionOperationReceiver（接口）
 *                      |  或 UInteractionOperationReceiverComponent（组件）
 *                      v
 *                  接收方自己实现：开门 / 开灯 / 移动进区域 …
 *
 *  接收方可以是**另一个同类可交互物**，这就是需求里说的"另一个物品的状态发生对应变化"。
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UInteractionLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionLinkComponent();

	/** 一次交互要发出的操作条目（可以配多条：一次交互同时影响多个物体）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	TArray<FInteractionOperationEntry> Entries;

	/** 自动接上 owner 的 UInteractableComponent（没有就建一个）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	bool bAutoBindInteractable = true;

	/**
	 * 在 BeginPlay 之后的第一帧自动发一轮 Entries（默认关）。
	 *
	 * 用途：一组"主 + 从"物体在关卡开始时就应处在同一个状态（从物跟随主物的初始开 / 关）。
	 * 之所以等到下一帧而不是 BeginPlay 当场执行：关卡里各 Actor 的 BeginPlay 顺序不确定，
	 * 抢在从物的 InteractableComponent::BeginPlay 之前写状态，会被它自己的 bStartOpen 覆盖。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	bool bSyncOnBeginPlay = false;

	/** 手动发一轮 Entries（不依赖交互，可被别的触发器调用）。返回成功投递的接收方数量。 */
	UFUNCTION(BlueprintCallable, Category="Interaction Operation")
	int32 DispatchEntries(AActor* Instigator);

	/** 立刻发一条指定操作：显式按频道广播（Targets 为空时只看频道）。 */
	UFUNCTION(BlueprintCallable, Category="Interaction Operation")
	int32 DispatchOperation(FName Operation, FName Channel, AActor* Instigator);

	/** 解析第 EntryIndex 条目会命中哪些接收方（自检：为什么没联动）。 */
	UFUNCTION(BlueprintCallable, Category="Interaction Operation")
	TArray<AActor*> ResolveEntryTargets(int32 EntryIndex) const;

	/** 每发出一条操作广播一次。 */
	UPROPERTY(BlueprintAssignable, Category="Interaction Operation")
	FOnInteractionOperationDispatched OnDispatched;

	/** 最近一次派发命中的接收方数量（调试最常用）。 */
	UFUNCTION(BlueprintPure, Category="Interaction Operation")
	int32 GetLastDispatchedCount() const { return LastDispatchedCount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleInteractRequested(AActor* Interactor, AActor* Interactable);

	/** bSyncOnBeginPlay 的下一帧回调。 */
	UFUNCTION()
	void HandleBeginPlaySync();

private:
	UPROPERTY(Transient)
	TObjectPtr<UInteractableComponent> BoundInteractable;

	int32 LastDispatchedCount = 0;

	/** 收集一个条目的接收方：显式引用 + 频道扫描（不含自己）。 */
	void GatherReceivers(const FInteractionOperationEntry& Entry, TArray<AActor*>& OutReceivers) const;

	/** 发一个条目，返回成功投递数。 */
	int32 DispatchOne(const FInteractionOperationEntry& Entry, AActor* Instigator);

	/** 投递给单个接收方：接口优先，其次接收组件。返回是否被接受。 */
	static bool Deliver(AActor* Receiver, const FInteractionOperation& Operation);
};
