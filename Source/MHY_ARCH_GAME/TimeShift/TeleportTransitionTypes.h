// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TimeShiftTypes.h"
#include "TeleportTransitionTypes.generated.h"

class AActor;

/**
 *  FTeleportTransitionContext - 一次传送过场所需要的一切信息。
 *
 *  过场系统（相机 / 淡入淡出 / UI / 动画）只需要这一个结构，不必去问传送组件内部状态：
 *  从哪来、到哪去、哪个时空到哪个时空、大概多久、谁在传送、哪对物体。
 */
USTRUCT(BlueprintType)
struct FTeleportTransitionContext
{
	GENERATED_BODY()

	/** 被传送的角色（主控角色）。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	TObjectPtr<AActor> Traveler = nullptr;

	/** 发起这次传送的装置（TimeEraPortal 的 owner）。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	TObjectPtr<AActor> Portal = nullptr;

	/** 对应物；VerticalOffset（仅 Z 不同）模式没有对应物，为 null。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	TObjectPtr<AActor> Counterpart = nullptr;

	/** 传送前的位置。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	FVector FromLocation = FVector::ZeroVector;

	/** 传送后的落点。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	FVector ToLocation = FVector::ZeroVector;

	/** 传送前角色所在的时空。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	ETimeEra FromEra = ETimeEra::Ancient;

	/** 传送后角色将要在的时空。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	ETimeEra ToEra = ETimeEra::Modern;

	/** 过场时长（秒）。0 = 没有过场，传送是瞬时的。 */
	UPROPERTY(BlueprintReadWrite, Category="Teleport Transition")
	float Duration = 0.0f;
};
