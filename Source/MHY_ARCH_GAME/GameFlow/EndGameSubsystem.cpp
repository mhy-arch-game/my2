// Copyright Epic Games, Inc. All Rights Reserved.

#include "EndGameSubsystem.h"

#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Images/SImage.h"

void UEndGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 模块自驱：跟 UiCue 一样用核心 ticker 推进流程，不需要关卡里放任何东西。
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UEndGameSubsystem::HandleTick), 0.0f);
	bTickRegistered = true;
}

void UEndGameSubsystem::Deinitialize()
{
	if (bTickRegistered)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		bTickRegistered = false;
	}

	RemoveBlackScreen();
	SetPlayerInputFrozen(false);

	Super::Deinitialize();
}

UEndGameSubsystem* UEndGameSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return UGameInstance::GetSubsystem<UEndGameSubsystem>(World->GetGameInstance());
	}
	return nullptr;
}

void UEndGameSubsystem::BeginEndGame(float InFadeOutSeconds, float InBlackHoldSeconds)
{
	if (bEnding)
	{
		return;   // 已经在结束流程里：重复触发直接忽略
	}

	bEnding = true;
	bQuitRequested = false;
	QuitFallbackTimer = 0.0f;
	Elapsed = 0.0f;
	FadeAlpha = 0.0f;
	FadeOutSeconds = FMath::Max(InFadeOutSeconds, 0.0f);
	HoldSeconds = FMath::Max(InBlackHoldSeconds, 0.0f);

	CreateBlackScreen();
	SetPlayerInputFrozen(true);
	OnEndGameSequenceStarted.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[EndGame] 开始结束流程：渐黑 %.2fs + 全黑保持 %.2fs。"),
		FadeOutSeconds, HoldSeconds);
}

void UEndGameSubsystem::CancelEndGame()
{
	if (!bEnding)
	{
		return;
	}

	bEnding = false;
	bQuitRequested = false;
	FadeAlpha = 0.0f;
	Elapsed = 0.0f;

	RemoveBlackScreen();
	SetPlayerInputFrozen(false);
}

bool UEndGameSubsystem::HandleTick(float DeltaTime)
{
	if (!bEnding)
	{
		return true;
	}

	// 已经请求退出：QuitGame 在某些平台 / 配置下不会立刻生效，超时就硬退。
	if (bQuitRequested)
	{
		QuitFallbackTimer -= DeltaTime;
		if (QuitFallbackTimer <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[EndGame] QuitGame 未生效，改用 RequestExit 强制退出进程。"));
			FPlatformMisc::RequestExit(/*Force=*/true);
		}
		return true;
	}

	Elapsed += DeltaTime;

	FadeAlpha = (FadeOutSeconds > 0.0f) ? FMath::Clamp(Elapsed / FadeOutSeconds, 0.0f, 1.0f) : 1.0f;
	ApplyFadeAlpha();

	if (Elapsed >= FadeOutSeconds + HoldSeconds)
	{
		QuitProcess();
	}

	return true;
}

void UEndGameSubsystem::CreateBlackScreen()
{
	if (BlackScreen.IsValid())
	{
		return;
	}

	UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (!Viewport)
	{
		// 没有视口（-nullrhi / 专用服务器）时照样能走完流程，只是没有黑屏可看。
		UE_LOG(LogTemp, Log, TEXT("[EndGame] 当前没有游戏视口，跳过黑屏（流程照常结束）。"));
		return;
	}

	// 直接往视口里塞一张全屏纯黑 SImage：比 UMG 控件轻，而且一定铺满、一定在最上层。
	// 笔刷用 FCoreStyle 的共享 WhiteBrush（自己 new 一个临时 FSlateColorBrush 会因为
	// TAttribute<const FSlateBrush*> 的 static_cast 报错），再用 ColorAndOpacity 调成"黑 + 透明度"。
	BlackScreen = SNew(SImage)
		.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.ColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));

	// ZOrder 5000：交互提示（ZOrder 100 以内）和 UiCue 字幕（50）都会被盖住。
	Viewport->AddViewportWidgetContent(BlackScreen.ToSharedRef(), /*ZOrder=*/5000);
}

void UEndGameSubsystem::ApplyFadeAlpha()
{
	if (!BlackScreen.IsValid())
	{
		return;
	}

	if (TSharedPtr<SImage> Image = StaticCastSharedPtr<SImage>(BlackScreen))
	{
		Image->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, FadeAlpha));
	}
}

void UEndGameSubsystem::RemoveBlackScreen()
{
	if (!BlackScreen.IsValid())
	{
		return;
	}

	if (UGameViewportClient* Viewport = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(BlackScreen.ToSharedRef());
	}
	BlackScreen.Reset();
}

void UEndGameSubsystem::SetPlayerInputFrozen(bool bFrozen)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	if (bFrozen)
	{
		bPrevIgnoreMove = PC->IsMoveInputIgnored();
		bPrevIgnoreLook = PC->IsLookInputIgnored();
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);
	}
	else
	{
		PC->SetIgnoreMoveInput(bPrevIgnoreMove);
		PC->SetIgnoreLookInput(bPrevIgnoreLook);
	}
}

void UEndGameSubsystem::QuitProcess()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;

	UE_LOG(LogTemp, Log, TEXT("[EndGame] 结束游戏进程。"));

	if (World)
	{
		UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
	}
	else
	{
		FPlatformMisc::RequestExit(/*Force=*/true);
		return;
	}

	// 兜底：真的退出去了就再也不会 tick 到；没退成就在 1 秒后硬退。
	bQuitRequested = true;
	QuitFallbackTimer = 1.0f;
}