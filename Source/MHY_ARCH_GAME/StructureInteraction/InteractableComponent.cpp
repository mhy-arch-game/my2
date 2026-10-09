// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractableComponent.h"

#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

UInteractableComponent::UInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bUseBuiltInToggle)
	{
		ResolveToggleComponents();

		// Remember the authored collision so closing can restore it exactly.
		InitialCollision.Reset();
		for (USceneComponent* Component : ToggleComponents)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			InitialCollision.Add(Primitive
				? Primitive->GetCollisionEnabled()
				: ECollisionEnabled::NoCollision);
		}
	}

	if (bToggleLights)
	{
		ResolveLightComponents();

		// Remember the authored intensity so switching on restores it.
		InitialLightIntensity.Reset();
		for (ULightComponent* Light : LightComponents)
		{
			InitialLightIntensity.Add(Light ? Light->Intensity : 0.0f);
		}
	}

	bIsOpen = bStartOpen;

	ApplyToggleState(/*bInstant=*/true);
	ApplyLightState();
}

void UInteractableComponent::ResolveToggleComponents()
{
	// Explicit object references win over names.
	if (ToggleComponents.Num() > 0)
	{
		ToggleComponents.RemoveAll([](const TObjectPtr<USceneComponent>& Item) { return Item == nullptr; });
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<USceneComponent*> Candidates;
	Owner->GetComponents<USceneComponent>(Candidates);

	for (const FName& Name : ToggleComponentNames)
	{
		for (USceneComponent* Candidate : Candidates)
		{
			if (Candidate && Candidate->GetFName() == Name)
			{
				ToggleComponents.Add(Candidate);
				break;
			}
		}
	}

	if (ToggleComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Interaction] %s: bUseBuiltInToggle is on but no Toggle Components resolved. ")
			TEXT("Set Toggle Component Names to the component name shown in the Components panel."),
			*GetNameSafe(Owner));
	}
}

void UInteractableComponent::SetOpen(bool bNewOpen, bool bInstant)
{
	if (!bUseBuiltInToggle && !bToggleLights)
	{
		return;
	}

	if (bNewOpen == bIsOpen && !bInstant)
	{
		return;
	}

	bIsOpen = bNewOpen;

	if (bUseBuiltInToggle)
	{
		if (bInstant || ToggleDuration <= 0.0f)
		{
			ApplyToggleState(/*bInstant=*/true);
		}
		else
		{
			bTransitioning = true;
			SetComponentTickEnabled(true);
		}
	}

	// Lights snap immediately; they do not interpolate.
	ApplyLightState();

	OnToggleChanged.Broadcast(GetOwner(), bIsOpen);
}

void UInteractableComponent::ResolveLightComponents()
{
	if (LightComponents.Num() > 0)
	{
		LightComponents.RemoveAll([](const TObjectPtr<ULightComponent>& Item) { return Item == nullptr; });
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<ULightComponent*> Candidates;
	Owner->GetComponents<ULightComponent>(Candidates);

	for (const FName& Name : LightComponentNames)
	{
		for (ULightComponent* Candidate : Candidates)
		{
			if (Candidate && Candidate->GetFName() == Name)
			{
				LightComponents.Add(Candidate);
				break;
			}
		}
	}

	if (LightComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Interaction] %s: bToggleLights is on but no light components resolved. ")
			TEXT("Set Light Component Names to the component name shown in the Components panel."),
			*GetNameSafe(Owner));
	}
}

void UInteractableComponent::ApplyLightState()
{
	if (!bToggleLights)
	{
		return;
	}

	const bool bOn = bIsOpen;

	for (int32 Index = 0; Index < LightComponents.Num(); ++Index)
	{
		ULightComponent* Light = LightComponents[Index];
		if (!Light)
		{
			continue;
		}

		Light->SetVisibility(bOn);

		const float Original = InitialLightIntensity.IsValidIndex(Index)
			? InitialLightIntensity[Index]
			: Light->Intensity;
		Light->SetIntensity(bOn ? Original : 0.0f);
	}
}

void UInteractableComponent::ApplyToggleState(bool bInstant)
{
	if (!bUseBuiltInToggle)
	{
		return;
	}

	const FTransform& Target = bIsOpen ? OpenRelativeTransform : ClosedRelativeTransform;

	if (bInstant)
	{
		for (USceneComponent* Component : ToggleComponents)
		{
			if (Component)
			{
				Component->SetRelativeTransform(Target);
			}
		}

		bTransitioning = false;
		SetComponentTickEnabled(false);
	}

	// Collision follows the logical state, not the interpolation.
	if (bDisableCollisionWhenOpen)
	{
		for (int32 Index = 0; Index < ToggleComponents.Num(); ++Index)
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(ToggleComponents[Index]);
			if (!Primitive)
			{
				continue;
			}

			if (bIsOpen)
			{
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			else
			{
				const ECollisionEnabled::Type Original =
					InitialCollision.IsValidIndex(Index)
						? InitialCollision[Index].GetValue()
						: ECollisionEnabled::QueryAndPhysics;
				Primitive->SetCollisionEnabled(Original);
			}
		}
	}
}

void UInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bTransitioning)
	{
		return;
	}

	const FTransform& Target = bIsOpen ? OpenRelativeTransform : ClosedRelativeTransform;
	const float Alpha = ToggleDuration > 0.0f
		? FMath::Clamp(DeltaTime / ToggleDuration, 0.0f, 1.0f)
		: 1.0f;

	bool bAllSettled = true;

	for (USceneComponent* Component : ToggleComponents)
	{
		if (!Component)
		{
			continue;
		}

		FTransform Next = Component->GetRelativeTransform();
		Next.BlendWith(Target, Alpha);
		Component->SetRelativeTransform(Next);

		if (!Next.Equals(Target, 0.01f))
		{
			bAllSettled = false;
		}
	}

	if (bAllSettled)
	{
		ApplyToggleState(/*bInstant=*/true);
	}
}

bool UInteractableComponent::CanInteract(AActor*) const
{
	return bEnabled;
}

void UInteractableComponent::NotifyInteract(AActor* Interactor)
{
	if (!bEnabled)
	{
		return;
	}

	// Built-in convenience first, then the event so Blueprint can add extras.
	if (bUseBuiltInToggle || bToggleLights)
	{
		SetOpen(!bIsOpen);
	}

	OnInteractRequested.Broadcast(Interactor, GetOwner());
}

void UInteractableComponent::NotifyFocusGained(AActor* Interactor)
{
	OnFocusGained.Broadcast(Interactor, GetOwner());
}

void UInteractableComponent::NotifyFocusLost(AActor* Interactor)
{
	OnFocusLost.Broadcast(Interactor, GetOwner());
}
