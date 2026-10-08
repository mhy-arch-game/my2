// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeShiftSubsystem.h"

#include "TimeLinkableInterface.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"

void UTimeShiftSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentEra = InitialEra;
}

void UTimeShiftSubsystem::Deinitialize()
{
	// Drop any pending cooldown ticker (the core ticker outlives this subsystem).
	if (CooldownHandle.IsValid())
	{
		FTSTicker::RemoveTicker(CooldownHandle);
		CooldownHandle.Reset();
	}
	bCooldownArmed = false;

	Anchors.Reset();
	Linkables.Reset();

	Super::Deinitialize();
}

UTimeShiftSubsystem* UTimeShiftSubsystem::Get(const UObject* WorldContextObject)
{
	if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContextObject))
	{
		return GameInstance->GetSubsystem<UTimeShiftSubsystem>();
	}
	return nullptr;
}

void UTimeShiftSubsystem::SwitchEra()
{
	SetEra(GetOtherEra());
}

void UTimeShiftSubsystem::SetEra(ETimeEra NewEra)
{
	// Refuse while already switching, while the timed lock is active, or when nothing changes.
	if (!CanSwitchEra() || NewEra == CurrentEra)
	{
		return;
	}

	// Guard against re-entrancy: listeners may (indirectly) try to switch again.
	bSwitching = true;

	CurrentEra = NewEra;

	// Listeners now gate their content (UTimeEraComponent) and relocate actors
	// (UTimeShiftTravelComponent) using the anchor pairs.
	OnEraChanged.Broadcast(CurrentEra);

	bSwitching = false;

	// The timed lock starts as soon as the switch has been applied.
	StartCooldown();
}

ETimeEra UTimeShiftSubsystem::GetOtherEra() const
{
	return CurrentEra == ETimeEra::Ancient ? ETimeEra::Modern : ETimeEra::Ancient;
}

// ---------------------------------------------------------------------------
// Era anchors
// ---------------------------------------------------------------------------

void UTimeShiftSubsystem::RegisterAnchor(AActor* Anchor, FName AnchorId, ETimeEra Era)
{
	if (!Anchor || AnchorId.IsNone())
	{
		return;
	}

	FTimeShiftAnchorPair& Pair = Anchors.FindOrAdd(AnchorId);
	if (Era == ETimeEra::Ancient)
	{
		Pair.Ancient = Anchor;
	}
	else
	{
		Pair.Modern = Anchor;
	}
}

void UTimeShiftSubsystem::UnregisterAnchor(AActor* Anchor)
{
	if (!Anchor)
	{
		return;
	}

	// An anchor may also have been serving as an era layout reference.
	UnregisterLayoutOrigin(Anchor);

	for (auto It = Anchors.CreateIterator(); It; ++It)
	{
		if (It.Value().Ancient.Get() == Anchor)
		{
			It.Value().Ancient = nullptr;
		}
		if (It.Value().Modern.Get() == Anchor)
		{
			It.Value().Modern = nullptr;
		}

		// Drop entries that no longer have either side.
		if (!It.Value().Ancient.IsValid() && !It.Value().Modern.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

AActor* UTimeShiftSubsystem::FindAnchor(FName AnchorId, ETimeEra Era) const
{
	if (const FTimeShiftAnchorPair* Pair = Anchors.Find(AnchorId))
	{
		return (Era == ETimeEra::Ancient) ? Pair->Ancient.Get() : Pair->Modern.Get();
	}
	return nullptr;
}

AActor* UTimeShiftSubsystem::FindNearestAnchor(ETimeEra Era, const FVector& Location, float MaxDistance) const
{
	AActor* Best = nullptr;
	double BestDistSq = (MaxDistance > 0.0f)
		? static_cast<double>(MaxDistance) * static_cast<double>(MaxDistance)
		: TNumericLimits<double>::Max();

	for (const TPair<FName, FTimeShiftAnchorPair>& Entry : Anchors)
	{
		AActor* Candidate = (Era == ETimeEra::Ancient) ? Entry.Value.Ancient.Get() : Entry.Value.Modern.Get();
		if (!Candidate)
		{
			continue;
		}

		const double DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Location);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	return Best;
}

FName UTimeShiftSubsystem::GetAnchorId(AActor* Anchor) const
{
	if (!Anchor)
	{
		return NAME_None;
	}

	for (const TPair<FName, FTimeShiftAnchorPair>& Entry : Anchors)
	{
		if (Entry.Value.Ancient.Get() == Anchor || Entry.Value.Modern.Get() == Anchor)
		{
			return Entry.Key;
		}
	}

	return NAME_None;
}

// ---------------------------------------------------------------------------
// Era layout references (1:1 layout mapping)
// ---------------------------------------------------------------------------

void UTimeShiftSubsystem::RegisterLayoutOrigin(AActor* LayoutRoot, ETimeEra Era)
{
	if (!LayoutRoot)
	{
		return;
	}

	if (Era == ETimeEra::Ancient)
	{
		AncientLayoutOrigin = LayoutRoot;
	}
	else
	{
		ModernLayoutOrigin = LayoutRoot;
	}
}

void UTimeShiftSubsystem::UnregisterLayoutOrigin(AActor* LayoutRoot)
{
	if (AncientLayoutOrigin.Get() == LayoutRoot)
	{
		AncientLayoutOrigin = nullptr;
	}
	if (ModernLayoutOrigin.Get() == LayoutRoot)
	{
		ModernLayoutOrigin = nullptr;
	}
}

AActor* UTimeShiftSubsystem::GetLayoutOrigin(ETimeEra Era) const
{
	return (Era == ETimeEra::Ancient) ? AncientLayoutOrigin.Get() : ModernLayoutOrigin.Get();
}

bool UTimeShiftSubsystem::GetLayoutTransform(ETimeEra Era, FTransform& OutTransform) const
{
	AActor* LayoutRoot = GetLayoutOrigin(Era);
	if (!LayoutRoot)
	{
		return false;
	}

	OutTransform = LayoutRoot->GetActorTransform();
	return true;
}

// ---------------------------------------------------------------------------
// Timed lock (cooldown)
// ---------------------------------------------------------------------------

bool UTimeShiftSubsystem::CanSwitchEra() const
{
	return !bSwitching && !IsOnCooldown();
}

bool UTimeShiftSubsystem::IsOnCooldown() const
{
	return FPlatformTime::Seconds() < CooldownEndTime;
}

float UTimeShiftSubsystem::GetCooldownRemaining() const
{
	const double Remaining = CooldownEndTime - FPlatformTime::Seconds();
	return FMath::Max(0.0f, static_cast<float>(Remaining));
}

float UTimeShiftSubsystem::GetCooldownRatio() const
{
	if (CooldownDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(GetCooldownRemaining() / CooldownDuration, 0.0f, 1.0f);
}

void UTimeShiftSubsystem::StartCooldown()
{
	// Drop any previous countdown first so restarts do not stack tickers.
	if (CooldownHandle.IsValid())
	{
		FTSTicker::RemoveTicker(CooldownHandle);
		CooldownHandle.Reset();
	}

	if (CooldownDuration <= 0.0f)
	{
		CooldownEndTime = 0.0;
		bCooldownArmed = false;
		return;
	}

	CooldownEndTime = FPlatformTime::Seconds() + CooldownDuration;
	bCooldownArmed = true;

	OnCooldownStarted.Broadcast();

	CooldownHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UTimeShiftSubsystem::HandleCooldownElapsed),
		CooldownDuration);
}

void UTimeShiftSubsystem::ClearCooldown()
{
	if (CooldownHandle.IsValid())
	{
		FTSTicker::RemoveTicker(CooldownHandle);
		CooldownHandle.Reset();
	}

	CooldownEndTime = 0.0;

	if (bCooldownArmed)
	{
		bCooldownArmed = false;
		OnCooldownFinished.Broadcast();
	}
}

void UTimeShiftSubsystem::SetCooldownDuration(float NewDuration)
{
	CooldownDuration = FMath::Max(0.0f, NewDuration);
}

bool UTimeShiftSubsystem::HandleCooldownElapsed(float DeltaTime)
{
	CooldownEndTime = 0.0;
	CooldownHandle.Reset();

	if (bCooldownArmed)
	{
		bCooldownArmed = false;
		OnCooldownFinished.Broadcast();
	}

	// Run once.
	return false;
}

// ---------------------------------------------------------------------------
// RESERVED: cross-era linkage plumbing.
// The switching feature does not drive this yet; it exists so the future
// "move an object in one era -> the other era reacts" feature can be completed
// without touching the switching code.
// ---------------------------------------------------------------------------

void UTimeShiftSubsystem::RegisterLinkable(AActor* Linkable)
{
	if (Linkable)
	{
		Linkables.AddUnique(Linkable);
	}
}

void UTimeShiftSubsystem::UnregisterLinkable(AActor* Linkable)
{
	Linkables.RemoveAll([Linkable](const TWeakObjectPtr<AActor>& Weak)
	{
		return !Weak.IsValid() || Weak.Get() == Linkable;
	});
}

void UTimeShiftSubsystem::BroadcastLinkState(FName LinkId, const FTimeLinkState& State)
{
	for (const TWeakObjectPtr<AActor>& Weak : Linkables)
	{
		AActor* Actor = Weak.Get();
		if (!Actor)
		{
			continue;
		}

		if (ITimeLinkableInterface* Linkable = Cast<ITimeLinkableInterface>(Actor))
		{
			if (Linkable->GetTimeLinkId() == LinkId)
			{
				Linkable->OnLinkedStateChanged(State);
			}
		}
	}
}

TArray<AActor*> UTimeShiftSubsystem::GetLinkablesById(FName LinkId) const
{
	TArray<AActor*> Result;

	for (const TWeakObjectPtr<AActor>& Weak : Linkables)
	{
		AActor* Actor = Weak.Get();
		if (!Actor)
		{
			continue;
		}

		if (const ITimeLinkableInterface* Linkable = Cast<ITimeLinkableInterface>(Actor))
		{
			if (Linkable->GetTimeLinkId() == LinkId)
			{
				Result.Add(Actor);
			}
		}
	}

	return Result;
}
