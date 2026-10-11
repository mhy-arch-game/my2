// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFlowLibrary.h"

#include "EndGameSubsystem.h"

void UGameFlowLibrary::EndGameWithFade(const UObject* WorldContextObject,
	float FadeOutSeconds, float BlackHoldSeconds)
{
	if (UEndGameSubsystem* Subsystem = UEndGameSubsystem::Get(WorldContextObject))
	{
		Subsystem->BeginEndGame(FadeOutSeconds, BlackHoldSeconds);
		return;
	}

	// 拿不到子系统（没有 GameInstance 的纯工具环境）：至少不要静默失败。
	UE_LOG(LogTemp, Warning, TEXT("[EndGame] 找不到 UEndGameSubsystem，结束流程未执行。"));
}

bool UGameFlowLibrary::IsEndingGame(const UObject* WorldContextObject)
{
	const UEndGameSubsystem* Subsystem = UEndGameSubsystem::Get(WorldContextObject);
	return Subsystem && Subsystem->IsEnding();
}

UEndGameSubsystem* UGameFlowLibrary::GetEndGameSubsystem(const UObject* WorldContextObject)
{
	return UEndGameSubsystem::Get(WorldContextObject);
}