// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EndOfTheGameActor.generated.h"

class UInteractableComponent;
class URotatingMovementComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  AEndOfTheGameActor —— 关卡里的「结束游戏」物件（interact 模块的可交互对象）的行为基类。
 *
 *  ★ 设计要点（踩过的坑）：本类**故意不声明 Interactable / StaticMesh 这类组件**。
 *    蓝图子类（end_of_the_game）自己声明了一个叫 Interactable 的组件，基类若再创建一个同名组件，
 *    蓝图编译会直接失败：
 *        Internal Compiler Error: Tried to create a property Interactable in scope end_of_the_game_C,
 *        but another object already exists there.   （KismetCompilerMisc.cpp）
 *    结论：**组件归蓝图 / 实例所有，基类只提供行为**。
 *
 *  本类负责：
 *    1. 找出物件上的 UInteractableComponent（一个都没有时自动补一个），提示词为空则填「触碰」；
 *    2. 交互回调 -> UGameFlowLibrary::EndGameWithFade：渐黑 -> 全黑 -> 结束游戏进程；
 *    3. 永续自转（URotatingMovementComponent，默认绕 Z 轴 45°/s）。
 *
 *  蓝图类 end_of_the_game 就是它的子类；关卡 firstvision 里已摆放实例（见 Docs/EndGame.md）。
 */
UCLASS(Blueprintable)
class AEndOfTheGameActor : public AActor
{
	GENERATED_BODY()

public:
	AEndOfTheGameActor();

	/** 渐黑用时（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game", meta=(ClampMin="0.0"))
	float FadeOutSeconds = 1.2f;

	/** 全黑保持（秒），之后结束进程。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game", meta=(ClampMin="0.0"))
	float BlackHoldSeconds = 0.6f;

	/** 物件上没有任何可交互组件时，自动补一个并把提示词设成这个（默认「触碰」）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game")
	FText FallbackInteractPrompt;

	/** 自转速度（度/秒）。默认绕 Z 轴 45°/s 慢转；设为 0 停转。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game|Spin")
	FRotator SpinRate = FRotator(0.0f, 45.0f, 0.0f);

	/** 绕自己的轴转（true，默认）还是绕世界轴转（false）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game|Spin")
	bool bSpinInLocalSpace = true;

	/** 是否自转：默认开，永续播放。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End Of The Game|Spin")
	bool bAutoSpin = true;

	/** 主动触发结束流程（不想走交互时）。重复调用无效。 */
	UFUNCTION(BlueprintCallable, Category="End Of The Game")
	void TriggerEndGame();

	/** 这个物件上的可交互组件（蓝图 / 实例上已有的优先）。 */
	UFUNCTION(BlueprintPure, Category="End Of The Game")
	UInteractableComponent* GetInteractableComponent() const;

	/** 本体网格（第一个 StaticMeshComponent —— 通常来自蓝图，也可能是自动补的方块）。 */
	UFUNCTION(BlueprintPure, Category="End Of The Game")
	UStaticMeshComponent* GetBodyMesh() const;

	/** 运行时开 / 关自转。 */
	UFUNCTION(BlueprintCallable, Category="End Of The Game|Spin")
	void SetSpinning(bool bEnabled);

	/** 运行时改自转速度（度/秒）。 */
	UFUNCTION(BlueprintCallable, Category="End Of The Game|Spin")
	void SetSpinRate(FRotator NewRate);

	/** 现在是否在转。 */
	UFUNCTION(BlueprintPure, Category="End Of The Game|Spin")
	bool IsSpinning() const;

	/** 自转组件本体（想调 PivotTranslation 之类细节时直接用它）。 */
	UFUNCTION(BlueprintPure, Category="End Of The Game|Spin")
	URotatingMovementComponent* GetSpinComponent() const { return Spin; }

protected:
	virtual void BeginPlay() override;

	/** 交互回调（interact 模块「按下交互键」）。 */
	UFUNCTION()
	void HandleInteractRequested(AActor* Interactor, AActor* InteractableActor);

	/** 永续自转：引擎自带的 URotatingMovementComponent（自动接管根组件）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="End Of The Game|Spin", meta=(AllowPrivateAccess="true"))
	TObjectPtr<URotatingMovementComponent> Spin;

	/** 运行时真正绑定的可交互组件（蓝图 / 实例上已有的，或自动补的）。 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInteractableComponent>> Interactables;

	/**
	 * 自动补方块时用的网格。放在（非 Transient 的）属性上是为了让它成为**硬引用** ——
	 * 这样 /Engine/BasicShapes/Cube 才会被 cook 收进包里；方块组件本身仍按需在运行时创建。
	 */
	UPROPERTY()
	TObjectPtr<UStaticMesh> FallbackBodyMesh;

	/** 把 SpinRate / bSpinInLocalSpace / bAutoSpin 推到 Spin 组件。 */
	void ApplySpinSettings();

	/** 找（必要时补）可交互组件与可碰撞本体，并填好提示词。 */
	void EnsureInteractionSetup();

private:
	/** 只触发一次。 */
	bool bTriggered = false;
};