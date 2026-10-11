// Copyright Epic Games, Inc. All Rights Reserved.

#include "UiCueSubsystem.h"

#include "UiCuePresenterInterface.h"
#include "UiCueSubtitleWidget.h"
#include "UiCueTriggerVolume.h"

#include "InteractableComponent.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UUiCueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 模块自驱：注册核心 ticker（不依赖关卡里是否放了组件）。
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UUiCueSubsystem::HandleTick), 0.0f);
	bTickRegistered = true;
}

void UUiCueSubsystem::Deinitialize()
{
	if (bTickRegistered)
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		bTickRegistered = false;
	}

	StopVoice();

	if (IsValid(SubtitleWidget))
	{
		SubtitleWidget->RemoveFromParent();
	}
	SubtitleWidget = nullptr;

	Super::Deinitialize();
}

UUiCueSubsystem* UUiCueSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		return UGameInstance::GetSubsystem<UUiCueSubsystem>(World->GetGameInstance());
	}
	return nullptr;
}

UUiCueSet* UUiCueSubsystem::ResolveCueSet()
{
	if (UUiCueSet* Loaded = CueSet.LoadSynchronous())
	{
		return Loaded;
	}

	// "全局统一"：工程里只会有一份 UUiCueSet，没显式指定时自动找它。
	if (!bTriedAutoFind)
	{
		bTriedAutoFind = true;

		FARFilter Filter;
		Filter.ClassPaths.Add(UUiCueSet::StaticClass()->GetClassPathName());
		Filter.bRecursiveClasses = true;

		TArray<FAssetData> Found;
		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		Registry.Get().GetAssets(Filter, Found);

		if (Found.Num() > 0)
		{
			CueSet = TSoftObjectPtr<UUiCueSet>(Found[0].ToSoftObjectPath());
			UE_LOG(LogTemp, Log, TEXT("[UiCue] 自动使用 cue 表：%s"), *Found[0].GetObjectPathString());
			return CueSet.LoadSynchronous();
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[UiCue] 工程里找不到 UUiCueSet 资产（DataAsset）。新建一个并填好 Cues 后即可工作。"));
	}

	return nullptr;
}

bool UUiCueSubsystem::HandleTick(float DeltaTime)
{
	UUiCueSet* Set = ResolveCueSet();
	if (!Set)
	{
		return true;
	}

	// 1) 模块自驱：检查所有触发源 + 绑定可交互物
	CheckTriggerVolumes();
	CheckGameStartCues();
	CheckProximityCues();

	InteractBindTimer -= DeltaTime;
	if (InteractBindTimer <= 0.0f)
	{
		InteractBindTimer = 1.0f;   // 每秒扫一次，只为发现新出现的可交互物
		BindInteractTriggers();
	}

	// 2) 推进当前 cue / 间隔 / 队列
	if (bPlaying)
	{
		SegmentTimer -= DeltaTime;
		if (SegmentTimer <= 0.0f)
		{
			AdvanceSegment();
		}
	}
	else if (bWaitingGap)
	{
		GapTimer -= DeltaTime;
		if (GapTimer <= 0.0f)
		{
			bWaitingGap = false;
		}
	}
	else if (Queue.Num() > 0)
	{
		const FUiCue Next = Queue[0];
		Queue.RemoveAt(0);
		StartCue(Next);
	}

	return true;
}

void UUiCueSubsystem::CheckTriggerVolumes()
{
	APlayerController* PC = GetLocalPlayerController();
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();

	for (TActorIterator<AUiCueTriggerVolume> It(World); It; ++It)
	{
		AUiCueTriggerVolume* Volume = *It;
		if (!Volume || Volume->IsTriggered() || Volume->TriggerId.IsNone())
		{
			continue;
		}

		if (Volume->ContainsLocation(PlayerLocation))
		{
			EnqueueCueForVolume(Volume);
		}
	}
}

bool UUiCueSubsystem::EnqueueCueForVolume(AUiCueTriggerVolume* Volume)
{
	if (!Volume)
	{
		return false;
	}

	const bool bEnqueued = EnqueueCueByTriggerId(Volume->TriggerId, EUiCueTrigger::BoxVolume,
		FString::Printf(TEXT("体积 %s"), *Volume->TriggerId.ToString()));

	// 命中与否都把体积关掉：需求是"触发后永久失效"。
	Volume->HandleTriggered();
	return bEnqueued;
}

bool UUiCueSubsystem::EnqueueCueByTriggerId(FName TriggerId, EUiCueTrigger Kind, const FString& SourceLabel)
{
	UUiCueSet* Set = ResolveCueSet();
	if (!Set || TriggerId.IsNone())
	{
		return false;
	}

	for (const FUiCue& Cue : Set->Cues)
	{
		if (Cue.Trigger != Kind || Cue.TriggerId != TriggerId)
		{
			continue;
		}

		if (Cue.bOnce && FiredCueIds.Contains(Cue.CueId))
		{
			return false;
		}

		FiredCueIds.Add(Cue.CueId);
		Queue.Add(Cue);

		UE_LOG(LogTemp, Log, TEXT("[UiCue] %s -> cue '%s' 入队（队列 %d）。"),
			*SourceLabel, *Cue.CueId.ToString(), Queue.Num());
		return true;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[UiCue] %s 没有匹配的 cue（检查 DataAsset 里的 FUiCue::Trigger / TriggerId）。"),
		*SourceLabel);
	return false;
}

void UUiCueSubsystem::CheckGameStartCues()
{
	if (bGameStartDone)
	{
		return;
	}

	UUiCueSet* Set = ResolveCueSet();
	if (!Set)
	{
		return;
	}

	// 等本地玩家就绪再发，保证"进入游戏"的配音/字幕玩家能听到看到。
	if (!GetLocalPlayerController())
	{
		return;
	}

	bGameStartDone = true;

	for (const FUiCue& Cue : Set->Cues)
	{
		if (Cue.Trigger != EUiCueTrigger::GameStart)
		{
			continue;
		}

		if (Cue.bOnce && FiredCueIds.Contains(Cue.CueId))
		{
			continue;
		}

		FiredCueIds.Add(Cue.CueId);
		Queue.Add(Cue);
		UE_LOG(LogTemp, Log, TEXT("[UiCue] 进入游戏 -> cue '%s' 入队（队列 %d）。"),
			*Cue.CueId.ToString(), Queue.Num());
	}
}

void UUiCueSubsystem::CheckProximityCues()
{
	UUiCueSet* Set = ResolveCueSet();
	if (!Set)
	{
		return;
	}

	APlayerController* PC = GetLocalPlayerController();
	APawn* Player = PC ? PC->GetPawn() : nullptr;
	if (!Player)
	{
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();

	for (const FUiCue& Cue : Set->Cues)
	{
		if (Cue.Trigger != EUiCueTrigger::ProximityToActor || Cue.TriggerId.IsNone())
		{
			continue;
		}

		if (Cue.bOnce && FiredCueIds.Contains(Cue.CueId))
		{
			continue;   // 已经触发过：连距离都不用算了
		}

		AActor* Target = ResolveActorById(Cue.TriggerId, World);
		if (!Target)
		{
			continue;
		}

		// 距离 = 玩家到"目标包围盒最近点"，墙再大也是"贴到它 N 厘米"的字面意思。
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		Target->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);

		const FBox Box(Origin - Extent, Origin + Extent);
		const float Distance = FVector::Dist(PlayerLocation, Box.GetClosestPointTo(PlayerLocation));
		const float TriggerDistance = FMath::Max(Cue.ProximityDistance, 0.0f);

		if (Distance <= TriggerDistance)
		{
			EnqueueCueByTriggerId(Cue.TriggerId, EUiCueTrigger::ProximityToActor,
				FString::Printf(TEXT("靠近 %s（%.0fcm ≤ %.0fcm）"),
					*Cue.TriggerId.ToString(), Distance, TriggerDistance));
		}
	}
}

AActor* UUiCueSubsystem::ResolveActorById(FName Id, UWorld* World)
{
	if (Id.IsNone() || !World)
	{
		return nullptr;
	}

	if (TWeakObjectPtr<AActor>* Cached = ActorIdCache.Find(Id))
	{
		if (Cached->IsValid())
		{
			return Cached->Get();
		}
		ActorIdCache.Remove(Id);
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (MatchesActorId(Id, Actor))
		{
			ActorIdCache.Add(Id, Actor);
			return Actor;
		}
	}

	return nullptr;
}

bool UUiCueSubsystem::MatchesActorId(FName TriggerId, const AActor* Actor)
{
	if (!Actor || TriggerId.IsNone())
	{
		return false;
	}

	// 关卡里摆放的物体：名字通常就等于标签（men1 / jiguandeng1 …）。
	return Actor->GetFName() == TriggerId || FName(*Actor->GetActorNameOrLabel()) == TriggerId;
}

void UUiCueSubsystem::BindInteractTriggers()
{
	UUiCueSet* Set = ResolveCueSet();
	if (!Set)
	{
		return;
	}

	// 只有真的用到这种触发方式才扫。
	bool bNeedsBinding = false;
	for (const FUiCue& Cue : Set->Cues)
	{
		if (Cue.Trigger == EUiCueTrigger::InteractFirstToggle)
		{
			bNeedsBinding = true;
			break;
		}
	}
	if (!bNeedsBinding)
	{
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	int32 Bound = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		UInteractableComponent* Interactable = Actor->FindComponentByClass<UInteractableComponent>();
		if (!Interactable)
		{
			continue;
		}

		const TWeakObjectPtr<UInteractableComponent> Key(Interactable);
		if (BoundInteractables.Contains(Key))
		{
			if (Key.IsValid())
			{
				continue;
			}
			// 过期条目（组件随关卡销毁）：清掉，免得新组件复用了同一地址时被误判成"已绑定"。
			BoundInteractables.Remove(Key);
		}

		BoundInteractables.Add(Key);
		Interactable->OnToggleChanged.AddDynamic(this, &UUiCueSubsystem::HandleInteractableToggled);
		// 没有内置状态的物体（例如 kongjianchuansuoqi*）不会广播 OnToggleChanged，
		// 它只能靠"按下交互键"这一路被抓到 —— 两路谁先算谁，"首次交互"即触发。
		Interactable->OnInteractRequested.AddDynamic(this, &UUiCueSubsystem::HandleInteractRequested);
		++Bound;
	}

	if (Bound > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[UiCue] 已绑定 %d 个可交互物（首次交互触发）。"), Bound);
	}
}

void UUiCueSubsystem::HandleInteractableToggled(AActor* Interactable, bool bIsOpen)
{
	UInteractableComponent* Component = Interactable
		? Interactable->FindComponentByClass<UInteractableComponent>() : nullptr;
	HandleFirstInteract(Component, bIsOpen ? TEXT("开") : TEXT("关"));
}

void UUiCueSubsystem::HandleInteractRequested(AActor* Interactor, AActor* Interactable)
{
	UInteractableComponent* Component = Interactable
		? Interactable->FindComponentByClass<UInteractableComponent>() : nullptr;
	HandleFirstInteract(Component, TEXT("交互"));
}

void UUiCueSubsystem::HandleFirstInteract(UInteractableComponent* Component, const FString& Reason)
{
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Owner)
	{
		return;
	}

	const TWeakObjectPtr<UInteractableComponent> Key(Component);
	if (FiredInteractables.Contains(Key))
	{
		// 只认"首次交互"：按下交互键与状态变化谁先发生算谁，之后另一路也不再生效。
		return;
	}
	FiredInteractables.Add(Key);

	const FString Label = Owner->GetActorNameOrLabel();
	const FString Source = FString::Printf(TEXT("可交互物 %s（首次%s）"), *Label, *Reason);

	// TriggerId 用物体在关卡里的名字；名字没配上时再试标签（有些物体两者不同名）。
	if (!EnqueueCueByTriggerId(Owner->GetFName(), EUiCueTrigger::InteractFirstToggle, Source))
	{
		EnqueueCueByTriggerId(FName(*Label), EUiCueTrigger::InteractFirstToggle, Source);
	}
}

bool UUiCueSubsystem::TriggerCueById(FName CueId)
{
	UUiCueSet* Set = ResolveCueSet();
	if (!Set || CueId.IsNone())
	{
		return false;
	}

	for (const FUiCue& Cue : Set->Cues)
	{
		if (Cue.CueId != CueId)
		{
			continue;
		}

		if (Cue.bOnce && FiredCueIds.Contains(CueId))
		{
			return false;
		}

		FiredCueIds.Add(CueId);
		Queue.Add(Cue);
		return true;
	}

	return false;
}

void UUiCueSubsystem::ClearQueue()
{
	Queue.Reset();
}

void UUiCueSubsystem::SkipCurrentCue()
{
	if (bPlaying)
	{
		FinishCue();
	}
}

void UUiCueSubsystem::StartCue(const FUiCue& Cue)
{
	UUiCueSet* Set = ResolveCueSet();

	CurrentCue = Cue;
	Segments.Reset();
	SplitTextIntoSegments(Cue.Text.ToString(),
		Set ? Set->SegmentDelimiters : TEXT("。！？!?.;；\n"), Segments);

	if (Segments.Num() == 0)
	{
		// 没文字：直接走完（仍然播配音 + 走间隔），避免队列卡住。
		Segments.Add(FString());
	}

	SegmentIndex = 0;
	SegmentTimer = 0.0f;
	bPlaying = true;

	PresenterActor = FindPresenter();
	if (!PresenterActor && (!Set || Set->bUseBuiltInSubtitle))
	{
		EnsureSubtitleWidget();
	}

	// 配音：一条 cue 可以有多段，按数组顺序连播（不重叠）。
	// 时长分配优先"一段对一句"：段数 == 句数时，每句用它自己那段配音的时长；
	// 否则退回"配音总时长 / 句数"，保证字幕总流程与配音总流程一致。
	SegmentSeconds.Reset();
	SegmentSeconds.SetNumZeroed(Segments.Num());

	float TotalVoiceSeconds = 0.0f;
	for (const TSoftObjectPtr<USoundBase>& Entry : Cue.Voices)
	{
		if (USoundBase* Loaded = Entry.LoadSynchronous())
		{
			TotalVoiceSeconds += FMath::Max(Loaded->GetDuration(), 0.0f);
		}
	}

	CueSegmentSeconds = 0.0f;
	if (TotalVoiceSeconds > 0.01f && Segments.Num() > 0)
	{
		CueSegmentSeconds = TotalVoiceSeconds / static_cast<float>(Segments.Num());

		if (Cue.Voices.Num() == Segments.Num())
		{
			for (int32 VoiceIndex = 0; VoiceIndex < Cue.Voices.Num(); ++VoiceIndex)
			{
				if (USoundBase* Loaded = Cue.Voices[VoiceIndex].LoadSynchronous())
				{
					SegmentSeconds[VoiceIndex] = Loaded->GetDuration();
				}
			}
		}
	}

	PrimaryVoice = Cue.Voices.Num() > 0 ? Cue.Voices[0] : TSoftObjectPtr<USoundBase>();

	if (Set && Set->bModulePlaysVoice)
	{
		PlayVoices(Cue.Voices);
	}

	OnCueBegan.Broadcast(Cue.CueId);
	if (PresenterActor)
	{
		IUiCuePresenter::Execute_OnUiCueBegin(PresenterActor, Cue.CueId, PrimaryVoice);
	}

	FString SegmentDurations;
	for (int32 Index = 0; Index < SegmentSeconds.Num(); ++Index)
	{
		SegmentDurations += FString::Printf(TEXT("%.1f "), SegmentSeconds[Index]);
	}

	UE_LOG(LogTemp, Log, TEXT("[UiCue] 开始 cue '%s'（%d 句，每句时长 [%s]秒，配音 %d 段，表现层=%s）。"),
		*Cue.CueId.ToString(), Segments.Num(), *SegmentDurations.TrimEnd(),
		Cue.Voices.Num(),
		PresenterActor ? *PresenterActor->GetName() : TEXT("内置字幕"));

	ShowSegment(0);
}

void UUiCueSubsystem::ShowSegment(int32 Index)
{
	UUiCueSet* Set = ResolveCueSet();

	if (!Segments.IsValidIndex(Index))
	{
		FinishCue();
		return;
	}

	FUiCueSegment Segment;
	Segment.CueId = CurrentCue.CueId;
	Segment.Text = FText::FromString(Segments[Index]);
	Segment.Index = Index;
	Segment.Count = Segments.Num();

	// 优先用"这一段配音自己的时长"，其次"配音总时长 / 句数"，再退到 cue 覆盖值 / 全局默认。
	const float Requested = (SegmentSeconds.IsValidIndex(Index) && SegmentSeconds[Index] > 0.0f)
		? SegmentSeconds[Index]
		: (CueSegmentSeconds > 0.0f
			? CueSegmentSeconds
			: (CurrentCue.SegmentSecondsOverride > 0.0f
				? CurrentCue.SegmentSecondsOverride
				: (Set ? Set->DefaultSegmentSeconds : 2.5f)));
	Segment.DurationSeconds = FMath::Max(Requested, 0.05f);

	SegmentIndex = Index;
	SegmentTimer = Segment.DurationSeconds;

	OnCueSegmentShown.Broadcast(Segment);
	if (PresenterActor)
	{
		IUiCuePresenter::Execute_OnUiCueSegment(PresenterActor, Segment);
	}
	else if (IsValid(SubtitleWidget))
	{
		SubtitleWidget->SetCueText(Segment.Text);
	}
}

void UUiCueSubsystem::AdvanceSegment()
{
	if (Segments.IsValidIndex(SegmentIndex + 1))
	{
		ShowSegment(SegmentIndex + 1);
		return;
	}

	FinishCue();
}

void UUiCueSubsystem::FinishCue()
{
	UUiCueSet* Set = ResolveCueSet();

	const FName FinishedId = CurrentCue.CueId;
	bPlaying = false;
	SegmentTimer = 0.0f;
	Segments.Reset();
	SegmentIndex = 0;

	OnCueEnded.Broadcast(FinishedId);
	if (PresenterActor)
	{
		IUiCuePresenter::Execute_OnUiCueEnd(PresenterActor, FinishedId);
	}
	else if (IsValid(SubtitleWidget))
	{
		SubtitleWidget->SetCueText(FText::GetEmpty());
	}

	// 按需求：不切断配音 —— 一条 cue 的总时长本来就等于它的音频时长，
	// 所以这里既不 StopVoice 也不需要等它，让音频自然播完即可。
	CueSegmentSeconds = 0.0f;
	SegmentSeconds.Reset();
	PrimaryVoice = nullptr;
	PresenterActor = nullptr;
	CurrentCue = FUiCue();

	// 元事件之间固定间隔（默认 3 秒；数值在 DataAsset 详情里改）。
	bWaitingGap = true;
	GapTimer = Set ? Set->QueueGapSeconds : 3.0f;
}

void UUiCueSubsystem::SplitTextIntoSegments(const FString& InText, const FString& Delimiters,
	TArray<FString>& OutSegments) const
{
	OutSegments.Reset();

	if (InText.IsEmpty())
	{
		return;
	}

	FString Current;
	for (int32 CharIndex = 0; CharIndex < InText.Len(); ++CharIndex)
	{
		const TCHAR Ch = InText[CharIndex];
		Current.AppendChar(Ch);

		// 断点：命中断句符就把当前小句收尾（标点保留在小句末尾）。
		if (Delimiters.Contains(FString(1, &Ch)))
		{
			FString Trimmed = Current.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				OutSegments.Add(Trimmed);
			}
			Current.Reset();
		}
	}

	FString Tail = Current.TrimStartAndEnd();
	if (!Tail.IsEmpty())
	{
		OutSegments.Add(Tail);
	}
}

AActor* UUiCueSubsystem::FindPresenter() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (Candidate && Candidate->GetClass()->ImplementsInterface(UUiCuePresenter::StaticClass()))
		{
			return Candidate;
		}
	}

	return nullptr;
}

APlayerController* UUiCueSubsystem::GetLocalPlayerController() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	return World ? World->GetFirstPlayerController() : nullptr;
}

void UUiCueSubsystem::EnsureSubtitleWidget()
{
	if (IsValid(SubtitleWidget))
	{
		return;
	}

	APlayerController* PC = GetLocalPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	SubtitleWidget = CreateWidget<UUiCueSubtitleWidget>(PC, UUiCueSubtitleWidget::StaticClass());
	if (!SubtitleWidget)
	{
		return;
	}

	SubtitleWidget->AddToViewport(50);

	// 屏幕字幕：底部居中，高度偏移在 DataAsset 里调。
	UUiCueSet* Set = ResolveCueSet();
	const float BottomOffset = Set ? Set->SubtitleBottomOffset : 140.0f;

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SubtitleWidget->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CanvasSlot->SetPosition(FVector2D(0.0f, -BottomOffset));
		CanvasSlot->SetAutoSize(true);
	}

	SubtitleWidget->SetCueText(FText::GetEmpty());
}

void UUiCueSubsystem::PlayVoices(const TArray<TSoftObjectPtr<USoundBase>>& Voices)
{
	PendingVoices = Voices;
	PendingVoiceIndex = 0;
	PlayNextVoice();
}

void UUiCueSubsystem::PlayNextVoice()
{
	// 用一个常驻组件 + SetSound + Play 来连播：bAutoDestroy 的组件会在结束时先销毁，
	// OnAudioFinished 就不可靠了；常驻组件能稳定回调，于是"多段配音"能无缝接上。
	while (PendingVoices.IsValidIndex(PendingVoiceIndex))
	{
		USoundBase* Sound = PendingVoices[PendingVoiceIndex].LoadSynchronous();
		++PendingVoiceIndex;

		if (!Sound)
		{
			continue;   // 跳过空条目
		}

		if (IsValid(VoiceComponent))
		{
			VoiceComponent->SetSound(Sound);
			VoiceComponent->Play(0.0f);
		}
		else
		{
			// 第一次播放：**必须传实际音频**建组件（传 nullptr 会建不出来），
			// 并且要关掉 AutoDestroy，OnAudioFinished 才会在每段结束时可靠回调。
			VoiceComponent = UGameplayStatics::CreateSound2D(this, Sound,
				/*VolumeMultiplier=*/1.0f, /*PitchMultiplier=*/1.0f, /*StartTime=*/0.0f,
				/*Concurrency=*/nullptr, /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);

			if (IsValid(VoiceComponent))
			{
				VoiceComponent->OnAudioFinished.AddUniqueDynamic(this, &UUiCueSubsystem::HandleVoiceFinished);
				VoiceComponent->Play(0.0f);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[UiCue] 配音组件创建失败（音频设备不可用？）：%s"), *Sound->GetName());
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[UiCue] 播放配音 %d/%d：%s"),
			PendingVoiceIndex, PendingVoices.Num(), *Sound->GetName());
		return;
	}
}

void UUiCueSubsystem::HandleVoiceFinished()
{
	// 顺序连播下一段；全部播完就自然结束（需求：不切断任何一段）。
	PlayNextVoice();
}

void UUiCueSubsystem::StopVoice()
{
	if (IsValid(VoiceComponent))
	{
		VoiceComponent->Stop();
	}
	VoiceComponent = nullptr;
}

FString UUiCueSubsystem::GetUiCueDebugString() const
{
	return FString::Printf(
		TEXT("UiCue | playing=%s | cue=%s | segment=%d/%d | queue=%d | gap=%s | presenter=%s"),
		bPlaying ? TEXT("yes") : TEXT("no"),
		*CurrentCue.CueId.ToString(),
		Segments.Num() > 0 ? SegmentIndex + 1 : 0, Segments.Num(),
		Queue.Num(),
		bWaitingGap ? *FString::Printf(TEXT("%.2fs"), GapTimer) : TEXT("-"),
		PresenterActor ? *PresenterActor->GetName() : TEXT("built-in"));
}
