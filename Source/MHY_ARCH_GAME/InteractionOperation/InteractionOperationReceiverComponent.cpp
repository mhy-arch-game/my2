// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionOperationReceiverComponent.h"

#include "InteractableComponent.h"

#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UInteractionOperationReceiverComponent::UInteractionOperationReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractionOperationReceiverComponent::BeginPlay()
{
	Super::BeginPlay();
	bFallbackLightsOn = false;
}

// ---------------------------------------------------------------------------
// 既有 interact 内容的复用
// ---------------------------------------------------------------------------

UInteractableComponent* UInteractionOperationReceiverComponent::GetInteractable()
{
	if (!Interactable)
	{
		if (const AActor* Owner = GetOwner())
		{
			Interactable = Owner->FindComponentByClass<UInteractableComponent>();
		}
	}
	return Interactable;
}

void UInteractionOperationReceiverComponent::EnsureFallbackLights()
{
	if (FallbackLights.Num() > 0)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<ULightComponent*> Lights;
	Owner->GetComponents<ULightComponent>(Lights);

	for (ULightComponent* Light : Lights)
	{
		if (!Light)
		{
			continue;
		}
		FallbackLights.Add(Light);
		FallbackLightIntensity.Add(Light->Intensity);
	}
}

void UInteractionOperationReceiverComponent::SetFallbackLights(bool bOn)
{
	EnsureFallbackLights();

	if (FallbackLights.Num() == 0)
	{
		return;
	}

	bFallbackLightsOn = bOn;

	for (int32 Index = 0; Index < FallbackLights.Num(); ++Index)
	{
		ULightComponent* Light = FallbackLights[Index];
		if (!Light)
		{
			continue;
		}

		Light->SetVisibility(bOn);

		const float Original = FallbackLightIntensity.IsValidIndex(Index)
			? FallbackLightIntensity[Index]
			: Light->Intensity;
		Light->SetIntensity(bOn ? Original : 0.0f);
	}
}

bool UInteractionOperationReceiverComponent::ApplyOpenState(bool bOpen)
{
	// 首选原先那套：一次 SetOpen 同时管住门和灯。
	if (UInteractableComponent* Component = GetInteractable())
	{
		Component->SetOpen(bOpen);
		return true;
	}

	// 没有 InteractableComponent 时的退化路径：直接驱动 owner 上的灯。
	SetFallbackLights(bOpen);
	return FallbackLights.Num() > 0;
}

// ---------------------------------------------------------------------------
// 接收
// ---------------------------------------------------------------------------

bool UInteractionOperationReceiverComponent::CanReceive(const FInteractionOperation& Operation) const
{
	return bEnabled;
}

bool UInteractionOperationReceiverComponent::ApplyOperation(const FInteractionOperation& Operation)
{
	if (!CanReceive(Operation))
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	bool bApplied = false;
	EInteractionOperationAction AppliedAction = EInteractionOperationAction::Nothing;

	for (const FInteractionOperationBinding& Binding : Bindings)
	{
		if (Binding.Operation != Operation.Operation)
		{
			continue;
		}

		switch (Binding.Action)
		{
		case EInteractionOperationAction::SetOpen:
			bApplied |= ApplyOpenState(true);
			break;

		case EInteractionOperationAction::SetClosed:
			bApplied |= ApplyOpenState(false);
			break;

		case EInteractionOperationAction::ToggleOpen:
			if (UInteractableComponent* Component = GetInteractable())
			{
				Component->SetOpen(!Component->IsOpen());
				bApplied = true;
			}
			else
			{
				SetFallbackLights(!bFallbackLightsOn);
				bApplied = FallbackLights.Num() > 0;
			}
			break;

		case EInteractionOperationAction::SetActorHidden:
			// bActive = HIDDEN. Collision must follow VISIBILITY, not bActive: a hidden
			// object should not block, and - the bug this fixes - an object brought BACK
			// to visible must collide again. (The old code enabled collision while hidden
			// and disabled it while visible, so a restored wall became walk-through.)
			Owner->SetActorHiddenInGame(Operation.bActive);
			Owner->SetActorEnableCollision(!Operation.bActive);
			bApplied = true;
			break;

		case EInteractionOperationAction::MirrorActive:
			// bActive carries the MASTER's current state, so a single binding makes this
			// object follow it (door / lamp: the same SetOpen path the master uses).
			bApplied |= ApplyOpenState(Operation.bActive);
			break;

		case EInteractionOperationAction::SetActorVisible:
			// "Active" means VISIBLE here - the opposite polarity of SetActorHidden.
			// Collision follows visibility, same rule as SetActorHidden (only the sign differs).
			Owner->SetActorHiddenInGame(!Operation.bActive);
			Owner->SetActorEnableCollision(Operation.bActive);
			bApplied = true;
			break;

		case EInteractionOperationAction::MoveTo:
		{
			const float Requested = Binding.Duration > 0.0f ? Binding.Duration : Operation.Value;
			MoveStart = Owner->GetActorLocation();
			MoveTarget = Operation.Location;

			if (Requested <= 0.0f)
			{
				Owner->SetActorLocation(MoveTarget, false, nullptr, ETeleportType::TeleportPhysics);
				bMoving = false;
				SetComponentTickEnabled(false);
			}
			else
			{
				MoveElapsed = 0.0f;
				MoveDuration = Requested;
				bMoving = true;
				SetComponentTickEnabled(true);
			}
			bApplied = true;
			break;
		}

		case EInteractionOperationAction::Nothing:
		default:
			// 占位动作：只走蓝图事件，不做默认改变。
			break;
		}

		if (bApplied)
		{
			AppliedAction = Binding.Action;
		}
	}

	LastAppliedOperation = Operation.Operation;

	// 无论有没有命中映射都通知蓝图，便于加音效/粒子，或者完全自己处理。
	OnOperationReceived(Operation);

	if (bApplied)
	{
		UE_LOG(LogTemp, Log, TEXT("[OperationReceiver] %s: 应用操作 '%s'。"),
			*GetNameSafe(Owner), *Operation.Operation.ToString());
		OnOperationApplied.Broadcast(Operation.Operation, AppliedAction);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("[OperationReceiver] %s: 操作 '%s' 没有匹配的映射，已忽略。"),
			*GetNameSafe(Owner), *Operation.Operation.ToString());
	}

	return bApplied;
}

void UInteractionOperationReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMoving)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		bMoving = false;
		SetComponentTickEnabled(false);
		return;
	}

	MoveElapsed += DeltaTime;
	const float Alpha = MoveDuration > 0.0f ? FMath::Clamp(MoveElapsed / MoveDuration, 0.0f, 1.0f) : 1.0f;

	Owner->SetActorLocation(FMath::Lerp(MoveStart, MoveTarget, Alpha), false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		bMoving = false;
		SetComponentTickEnabled(false);
	}
}
