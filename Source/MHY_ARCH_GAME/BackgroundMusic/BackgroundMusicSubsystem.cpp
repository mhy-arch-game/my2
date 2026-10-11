// Copyright Epic Games, Inc. All Rights Reserved.

#include "BackgroundMusicSubsystem.h"

#include "BackgroundMusicSettings.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/UObjectGlobals.h"

void UBackgroundMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 初始音量 / 倍速取全局设置。
	if (const UBackgroundMusicSettings* Settings = GetSettings())
	{
		Volume = Settings->Volume;
		Speed = Settings->Speed;
		bWantsToPlay = Settings->bAutoPlay;
	}

	// 全世界通用：用核心 ticker 保活（世界此刻可能还没准备好，EnsurePlaying 会在下一帧重试）。
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBackgroundMusicSubsystem::HandleTick), 0.0f);
	bTickRegistered = true;
}

void UBackgroundMusicSubsystem::Deinitialize()
{
	if (bTickRegistered)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		bTickRegistered = false;
	}

	if (IsValid(MusicComponent))
	{
		MusicComponent->Stop();
	}
	MusicComponent = nullptr;

	Super::Deinitialize();
}

UBackgroundMusicSubsystem* UBackgroundMusicSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return UGameInstance::GetSubsystem<UBackgroundMusicSubsystem>(World->GetGameInstance());
	}
	return nullptr;
}

const UBackgroundMusicSettings* UBackgroundMusicSubsystem::GetSettings() const
{
	return GetDefault<UBackgroundMusicSettings>();
}

bool UBackgroundMusicSubsystem::HandleTick(float DeltaTime)
{
	EnsurePlaying();
	return true;
}

void UBackgroundMusicSubsystem::EnsurePlaying()
{
	if (!bWantsToPlay)
	{
		return;
	}

	const UBackgroundMusicSettings* Settings = GetSettings();
	if (!Settings || Settings->Music.IsNull())
	{
		return;
	}

	// 已经在播：只把当前音量 / 倍速同步过去（设置改了也会生效）。
	if (IsValid(MusicComponent) && MusicComponent->IsPlaying())
	{
		MusicComponent->SetVolumeMultiplier(Volume);
		MusicComponent->SetPitchMultiplier(Speed);
		return;
	}

	// 需要（重新）起播：世界 / 本地玩家就绪前直接放过，下一帧再试。
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	USoundBase* Music = Settings->Music.LoadSynchronous();
	if (!Music)
	{
		return;
	}

	// bAutoDestroy=false：由本模块持有，切关卡 / 重播都靠它。
	if (IsValid(MusicComponent))
	{
		// 资源本身没勾 Loop 时，播完了在这里续上。
		MusicComponent->SetVolumeMultiplier(Volume);
		MusicComponent->SetPitchMultiplier(Speed);
		MusicComponent->Play(0.0f);
		return;
	}

	MusicComponent = UGameplayStatics::SpawnSound2D(this, Music, /*VolumeMultiplier=*/Volume,
		/*PitchMultiplier=*/Speed, /*StartTime=*/0.0f, /*Concurrency=*/nullptr,
		/*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);

	if (IsValid(MusicComponent))
	{
		UE_LOG(LogTemp, Log, TEXT("[BackgroundMusic] 开始循环播放 %s（音量 %.2f，倍速 %.2f）。"),
			*Music->GetName(), Volume, Speed);
	}
}

void UBackgroundMusicSubsystem::PlayMusic()
{
	bWantsToPlay = true;
	EnsurePlaying();
}

void UBackgroundMusicSubsystem::StopMusic()
{
	bWantsToPlay = false;

	if (IsValid(MusicComponent))
	{
		MusicComponent->Stop();
	}

	OnMusicChanged.Broadcast(Volume, Speed, false);
}

void UBackgroundMusicSubsystem::SetVolume(float NewVolume)
{
	Volume = FMath::Max(NewVolume, 0.0f);

	if (IsValid(MusicComponent))
	{
		MusicComponent->SetVolumeMultiplier(Volume);
	}

	OnMusicChanged.Broadcast(Volume, Speed, IsPlaying());
}

void UBackgroundMusicSubsystem::SetSpeed(float NewSpeed)
{
	Speed = FMath::Clamp(NewSpeed, 0.05f, 4.0f);

	if (IsValid(MusicComponent))
	{
		MusicComponent->SetPitchMultiplier(Speed);
	}

	OnMusicChanged.Broadcast(Volume, Speed, IsPlaying());
}

bool UBackgroundMusicSubsystem::IsPlaying() const
{
	return IsValid(MusicComponent) && MusicComponent->IsPlaying();
}

FString UBackgroundMusicSubsystem::GetBackgroundMusicDebugString() const
{
	const UBackgroundMusicSettings* Settings = GetSettings();
	return FString::Printf(
		TEXT("BackgroundMusic | playing=%s | volume=%.2f | speed=%.2f | want=%s | asset=%s"),
		IsPlaying() ? TEXT("yes") : TEXT("no"),
		Volume, Speed,
		bWantsToPlay ? TEXT("yes") : TEXT("no"),
		(Settings && !Settings->Music.IsNull()) ? *Settings->Music.ToString() : TEXT("<未设置>"));
}
