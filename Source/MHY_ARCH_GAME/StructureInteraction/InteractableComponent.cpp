// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractableComponent.h"

#include "Components/BoxComponent.h"
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

		// Build the proxy while the object still IS at its closed pose.
		if (bUseInteractionProxy)
		{
			CreateInteractionProxy();
		}

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

	if (bDisableCollisionWhenOpen && CollidersDisabledWhenOpen.Num() > 0)
	{
		ResolveExtraColliders();
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

FBox UInteractableComponent::ComputeClosedPoseBounds() const
{
	FBox Box(ForceInit);

	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return Box;
	}

	const FTransform RootInverse = Owner->GetRootComponent()->GetComponentTransform().Inverse();

	for (USceneComponent* Component : ToggleComponents)
	{
		if (!Component)
		{
			continue;
		}

		// UPrimitiveComponent::Bounds is WORLD space and unambiguous, so map its 8
		// corners back into the owner's space. (USceneComponent::GetLocalBounds in
		// 5.7 returns an FBoxSphereBounds instead of Min/Max out-params, and its
		// scaling is easy to get wrong.)
		const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
		if (!Primitive)
		{
			continue;
		}

		const FBoxSphereBounds World = Primitive->Bounds;
		const FVector Min = World.Origin - World.BoxExtent;
		const FVector Max = World.Origin + World.BoxExtent;

		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector Point(
				(Corner & 1) ? Max.X : Min.X,
				(Corner & 2) ? Max.Y : Min.Y,
				(Corner & 4) ? Max.Z : Min.Z);
			Box += RootInverse.TransformPosition(Point);
		}
	}

	return Box;
}

void UInteractableComponent::CreateInteractionProxy()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent() || InteractionProxy || ToggleComponents.Num() == 0)
	{
		return;
	}

	const FBox Box = ComputeClosedPoseBounds();
	if (!Box.IsValid)
	{
		return;
	}

	FVector Extent = InteractionProxyExtent.IsNearlyZero()
		? Box.GetExtent()
		: InteractionProxyExtent;
	Extent += FVector(InteractionProxyPadding);

	UBoxComponent* Proxy = NewObject<UBoxComponent>(Owner, TEXT("InteractionProxy"), RF_Transient);
	Proxy->SetupAttachment(Owner->GetRootComponent());
	Proxy->SetBoxExtent(Extent);
	Proxy->SetRelativeLocation(Box.GetCenter());
	Proxy->SetRelativeRotation(FRotator::ZeroRotator);

	// Query-only and pickable by the interaction trace, but deliberately unable to
	// block anything else - the player must be able to walk straight through it.
	Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Proxy->SetCollisionObjectType(ECC_WorldDynamic);
	Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);
	Proxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Proxy->SetGenerateOverlapEvents(false);
	Proxy->SetCanEverAffectNavigation(false);
	Proxy->SetHiddenInGame(true);
	Proxy->SetVisibility(false);
	Proxy->RegisterComponent();

	InteractionProxy = Proxy;

	UE_LOG(LogTemp, Log,
		TEXT("[Interaction] %s: interaction proxy created (extent %s) so the object stays ")
		TEXT("detectable while toggled."),
		*GetNameSafe(Owner), *Extent.ToCompactString());
}

void UInteractableComponent::SetOpen(bool bNewOpen, bool bInstant)
{
	// bTrackOpenState joins the two built-in behaviours: it makes the component hold a
	// real 0/1 state even when there is nothing to move and no light to switch.
	if (!bUseBuiltInToggle && !bToggleLights && !bTrackOpenState)
	{
		return;
	}

	if (bNewOpen == bIsOpen && !bInstant)
	{
		return;
	}

	bIsOpen = bNewOpen;

	// Only the transform toggle interpolates. Without this guard a lights-only or
	// state-only component would enable its tick and never clear bTransitioning
	// (ApplyToggleState returns early when bUseBuiltInToggle is off).
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

FTransform UInteractableComponent::GetTargetTransform() const
{
	if (!bIsOpen)
	{
		return ClosedRelativeTransform;
	}

	if (!bUseAxisRotation)
	{
		return OpenRelativeTransform;
	}

	const FVector Axis = RotationAxis.GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		// Degenerate axis: fall back to the closed pose rather than spinning wildly.
		return ClosedRelativeTransform;
	}

	// Rotate about an axis through a local pivot:
	//     M = T(P) * R * T(-P)      (applied in this component's own space)
	// so the component orbits RotationPivot instead of its own origin.
	const FQuat Rotation(Axis, FMath::DegreesToRadians(OpenAngleDegrees));
	const FTransform ToPivot(FQuat::Identity, RotationPivot);
	const FTransform Spin(Rotation, FVector::ZeroVector);
	const FTransform FromPivot(FQuat::Identity, -RotationPivot);

	return ClosedRelativeTransform * (ToPivot * Spin * FromPivot);
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

void UInteractableComponent::ResolveExtraColliders()
{
	ExtraColliderPrimitives.Reset();
	ExtraColliderInitialCollision.Reset();

	for (AActor* Actor : CollidersDisabledWhenOpen)
	{
		if (!Actor)
		{
			continue;
		}

		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents<UPrimitiveComponent>(Primitives);

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!Primitive)
			{
				continue;
			}

			ExtraColliderPrimitives.Add(Primitive);
			ExtraColliderInitialCollision.Add(Primitive->GetCollisionEnabled());
		}
	}
}

void UInteractableComponent::ApplyToggleState(bool bInstant)
{
	if (!bUseBuiltInToggle)
	{
		return;
	}

	const FTransform Target = GetTargetTransform();

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

		// Extra actors (e.g. a separately-placed door frame) follow the same rule -
		// their transform is never touched, only their collision.
		for (int32 Index = 0; Index < ExtraColliderPrimitives.Num(); ++Index)
		{
			UPrimitiveComponent* Primitive = ExtraColliderPrimitives[Index];
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
					ExtraColliderInitialCollision.IsValidIndex(Index)
						? ExtraColliderInitialCollision[Index].GetValue()
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

	const FTransform Target = GetTargetTransform();
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
	if (bUseBuiltInToggle || bToggleLights || bTrackOpenState)
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
