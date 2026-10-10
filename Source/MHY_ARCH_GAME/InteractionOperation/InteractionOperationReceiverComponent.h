// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractionOperationTypes.h"
#include "InteractionOperationReceiverComponent.generated.h"

class AActor;
class UInteractableComponent;
class ULightComponent;

/** 每次成功应用一个操作时广播（做额外表现：音效、粒子、镜头）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionOperationApplied, FName, Operation, EInteractionOperationAction, Action);

/**
 *  FInteractionOperationBinding - "收到哪个操作时，对 owner 做什么"。
 */
USTRUCT(BlueprintType)
struct FInteractionOperationBinding
{
	GENERATED_BODY()

	/** 匹配的操作名，对应发送方条目的 Operation / ClosedOperation。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FName Operation = TEXT("Open");

	/** 收到该操作时对 owner 执行的动作。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	EInteractionOperationAction Action = EInteractionOperationAction::SetOpen;

	/** MoveTo 专用：插值时长（秒）。<= 0 时改用操作自带的 Value。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	float Duration = 0.0f;
};

/**
 *  UInteractionOperationReceiverComponent - 接收"抽象操作"并改变 owner 的状态。
 *
 *  这是"让另一个物品发生对应变化"的**零蓝图方案**：挂到被操作物（另一扇门、另一盏灯、
 *  需要被搬进区域的箱子…）上，填一张"操作名 -> 动作"的表即可。
 *
 *  动作全部建立在**原先的 interact 内容**之上：
 *
 *   - 若 owner 上有 UInteractableComponent（原先那套内置开关 + 灯光开关），
 *     SetOpen/SetClosed/ToggleOpen 直接调用它的 SetOpen()，
 *     所以"开门"和"开灯"依旧是同一个状态在驱动；
 *   - 若 owner 上没有 InteractableComponent，但有灯光组件，则退化为直接驱动这些灯
 *     （可见性 + 记录下来的原始强度），让一盏"光秃秃的灯"也能被联动。
 *
 *  寻址：本组件通过 Channel 暴露自己；发送方按同名频道广播即可命中。
 *  想在蓝图里做自定义行为（而不是用这张映射表），可以直接让蓝图类实现
 *  IInteractionOperationReceiver 接口，或监听 OnOperationApplied / OnOperationReceived。
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UInteractionOperationReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionOperationReceiverComponent();

	/** 频道名：发送方按频道广播时匹配它。留空 = 只能被显式引用。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	FName Channel = NAME_None;

	/** 操作名 -> 动作 的映射表；没匹配上的操作会被忽略。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	TArray<FInteractionOperationBinding> Bindings;

	/** 总开关：关掉后本组件拒绝所有操作。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction Operation")
	bool bEnabled = true;

	/** 每次成功应用一个操作时广播。 */
	UPROPERTY(BlueprintAssignable, Category="Interaction Operation")
	FOnInteractionOperationApplied OnOperationApplied;

	/** 收到任何操作时都会走到（即使映射表里没匹配），用来加自己的表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Interaction Operation")
	void OnOperationReceived(const FInteractionOperation& Operation);

	/** 本组件的频道名（发送方扫描时读它）。 */
	UFUNCTION(BlueprintPure, Category="Interaction Operation")
	FName GetChannel() const { return Channel; }

	/** 是否接受这次操作。 */
	UFUNCTION(BlueprintCallable, Category="Interaction Operation")
	bool CanReceive(const FInteractionOperation& Operation) const;

	/** 应用操作；返回是否命中并执行了至少一个映射。 */
	UFUNCTION(BlueprintCallable, Category="Interaction Operation")
	bool ApplyOperation(const FInteractionOperation& Operation);

	/** 最近一次命中的操作名 / 动作（调试用）。 */
	UFUNCTION(BlueprintPure, Category="Interaction Operation")
	FName GetLastAppliedOperation() const { return LastAppliedOperation; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	FName LastAppliedOperation = NAME_None;

	/** owner 上的 InteractableComponent（惰性查找）。 */
	UPROPERTY(Transient)
	TObjectPtr<UInteractableComponent> Interactable;

	/** 没有 InteractableComponent 时的灯光兜底：记录原始强度。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> FallbackLights;

	TArray<float> FallbackLightIntensity;

	/** 灯光兜底当前的开状态。 */
	bool bFallbackLightsOn = false;

	/** MoveTo 的插值状态。 */
	FVector MoveStart = FVector::ZeroVector;
	FVector MoveTarget = FVector::ZeroVector;
	float MoveElapsed = 0.0f;
	float MoveDuration = 0.0f;
	bool bMoving = false;

	UInteractableComponent* GetInteractable();
	void EnsureFallbackLights();
	void SetFallbackLights(bool bOn);
	bool ApplyOpenState(bool bOpen);
};
