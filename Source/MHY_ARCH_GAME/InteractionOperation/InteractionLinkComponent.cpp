// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionLinkComponent.h"

#include "InteractionOperationReceiverComponent.h"
#include "InteractionOperationReceiverInterface.h"
#include "InteractableComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"

UInteractionLinkComponent::UInteractionLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void UInteractionLinkComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoBindInteractable)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BoundInteractable = Owner->FindComponentByClass<UInteractableComponent>();
	if (!BoundInteractable)
	{
		// Nothing on the object yet: create one so a plain actor becomes interactable
		// without touching its class or its Blueprint graph.
		BoundInteractable = NewObject<UInteractableComponent>(
			Owner, UInteractableComponent::StaticClass(), TEXT("InteractionLinkInteractable"), RF_Transient);
		if (BoundInteractable)
		{
			BoundInteractable->RegisterComponent();
			UE_LOG(LogTemp, Log, TEXT("[OperationLink] %s: 自动创建 InteractableComponent"), *Owner->GetName());
		}
	}

	if (BoundInteractable)
	{
		BoundInteractable->OnInteractRequested.AddDynamic(this, &UInteractionLinkComponent::HandleInteractRequested);
	}

	if (bSyncOnBeginPlay)
	{
		if (UWorld* World = GetWorld())
		{
			// Next tick, not now: every actor's BeginPlay must have run first, otherwise a
			// slave's own bStartOpen would overwrite the state we just mirrored onto it.
			World->GetTimerManager().SetTimerForNextTick(
				this, &UInteractionLinkComponent::HandleBeginPlaySync);
		}
	}
}

void UInteractionLinkComponent::HandleBeginPlaySync()
{
	DispatchEntries(nullptr);
}

void UInteractionLinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundInteractable)
	{
		BoundInteractable->OnInteractRequested.RemoveDynamic(this, &UInteractionLinkComponent::HandleInteractRequested);
	}

	Super::EndPlay(EndPlayReason);
}

void UInteractionLinkComponent::HandleInteractRequested(AActor* Interactor, AActor* Interactable)
{
	DispatchEntries(Interactor);
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------

int32 UInteractionLinkComponent::DispatchEntries(AActor* Instigator)
{
	int32 Total = 0;
	for (const FInteractionOperationEntry& Entry : Entries)
	{
		Total += DispatchOne(Entry, Instigator);
	}

	LastDispatchedCount = Total;

	if (Total == 0)
	{
		// Almost always a wiring problem rather than a game-logic one, so name it.
		UE_LOG(LogTemp, Warning,
			TEXT("[OperationLink] %s: 交互发出了 %d 个条目，但没有任何接收方被命中。")
			TEXT("检查 Channel 是否与接收方一致（可用 ResolveEntryTargets 自检）。"),
			*GetNameSafe(GetOwner()), Entries.Num());
	}

	return Total;
}

int32 UInteractionLinkComponent::DispatchOne(const FInteractionOperationEntry& Entry, AActor* Instigator)
{
	// The source's own state drives mirroring; UInteractableComponent::NotifyInteract
	// already flipped it before broadcasting, so this is the NEW state.
	bool bSourceOpen = true;
	if (BoundInteractable)
	{
		bSourceOpen = BoundInteractable->IsOpen();
	}

	FInteractionOperation Operation;
	Operation.Operation = (Entry.bMirrorSourceState && !bSourceOpen) ? Entry.ClosedOperation : Entry.Operation;
	Operation.Instigator = Instigator;
	Operation.Source = GetOwner();
	Operation.Target = Entry.Target;
	Operation.Value = Entry.Value;
	Operation.Location = Entry.Location;
	Operation.bActive = bSourceOpen;

	TArray<AActor*> Receivers;
	GatherReceivers(Entry, Receivers);

	int32 Count = 0;
	for (AActor* Receiver : Receivers)
	{
		if (Deliver(Receiver, Operation))
		{
			++Count;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OperationLink] %s -> '%s' (channel '%s'): %d receiver(s)."),
		*GetNameSafe(GetOwner()), *Operation.Operation.ToString(), *Entry.Channel.ToString(), Count);

	OnDispatched.Broadcast(Operation.Operation, Count);
	return Count;
}

int32 UInteractionLinkComponent::DispatchOperation(FName Operation, FName Channel, AActor* Instigator)
{
	FInteractionOperationEntry Entry;
	Entry.Operation = Operation;
	Entry.Channel = Channel;
	return DispatchOne(Entry, Instigator);
}

void UInteractionLinkComponent::GatherReceivers(const FInteractionOperationEntry& Entry, TArray<AActor*>& OutReceivers) const
{
	OutReceivers.Reset();

	// 1) explicit references win.
	for (const TObjectPtr<AActor>& Explicit : Entry.Targets)
	{
		if (Explicit && Explicit != GetOwner())
		{
			OutReceivers.AddUnique(Explicit);
		}
	}

	// 2) channel addressing: any other actor that answers on the same channel.
	if (Entry.Channel.IsNone())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == GetOwner() || OutReceivers.Contains(Candidate))
		{
			continue;
		}

		FName CandidateChannel = NAME_None;

		if (Candidate->GetClass()->ImplementsInterface(UInteractionOperationReceiver::StaticClass()))
		{
			CandidateChannel = IInteractionOperationReceiver::Execute_GetOperationChannel(Candidate);
		}
		else if (const UInteractionOperationReceiverComponent* Receiver =
			Candidate->FindComponentByClass<UInteractionOperationReceiverComponent>())
		{
			CandidateChannel = Receiver->GetChannel();
		}

		if (!CandidateChannel.IsNone() && CandidateChannel == Entry.Channel)
		{
			OutReceivers.Add(Candidate);
		}
	}
}

bool UInteractionLinkComponent::Deliver(AActor* Receiver, const FInteractionOperation& Operation)
{
	if (!Receiver)
	{
		return false;
	}

	// Interface first: a Blueprint (or C++) receiver owns its own behaviour.
	if (Receiver->GetClass()->ImplementsInterface(UInteractionOperationReceiver::StaticClass()))
	{
		if (!IInteractionOperationReceiver::Execute_CanReceiveOperation(Receiver, Operation))
		{
			return false;
		}

		IInteractionOperationReceiver::Execute_ApplyInteractionOperation(Receiver, Operation);
		return true;
	}

	// Component fallback: zero-Blueprint wiring, reusing the built-in interact logic.
	if (UInteractionOperationReceiverComponent* ReceiverComponent =
		Receiver->FindComponentByClass<UInteractionOperationReceiverComponent>())
	{
		if (!ReceiverComponent->CanReceive(Operation))
		{
			return false;
		}

		ReceiverComponent->ApplyOperation(Operation);
		return true;
	}

	return false;
}

TArray<AActor*> UInteractionLinkComponent::ResolveEntryTargets(int32 EntryIndex) const
{
	TArray<AActor*> Result;
	if (Entries.IsValidIndex(EntryIndex))
	{
		GatherReceivers(Entries[EntryIndex], Result);
	}
	return Result;
}
