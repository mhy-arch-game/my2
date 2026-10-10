// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionDetectorComponent.h"

#include "InteractableComponent.h"
#include "InteractableInterface.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialParameterCollection.h"

UInteractionDetectorComponent::UInteractionDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InteractKey = EKeys::E;

	// Typical scene object types an interactable may use.
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery1); // WorldStatic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery2); // WorldDynamic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery3); // Pawn
}

void UInteractionDetectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Player-side only: it must be able to bind the interact action and read a view
	// point, which requires a Pawn owner. On anything else it is a misplaced
	// component - disable it instead of polling forever.
	if (!Cast<APawn>(GetOwner()))
	{
		bPlayerSide = false;
		SetComponentTickEnabled(false);
		UE_LOG(LogTemp, Warning,
			TEXT("[Interaction] %s: InteractionDetector is attached to a '%s', which is not a Pawn. ")
			TEXT("It is a PLAYER-side component and has been disabled - remove it from that object's Blueprint."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(GetOwner() ? GetOwner()->GetClass() : nullptr));
		return;
	}

	bPlayerSide = true;

	// Try to bind straight away; if the pawn's input component is not ready yet,
	// TickComponent will retry until it is.
	TryBindInput();
	RegisterInteractContext();
	RefreshFocus();
}

void UInteractionDetectorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave custom depth behind once the detector goes away.
	ClearFocusOutline();

	Super::EndPlay(EndPlayReason);
}

void UInteractionDetectorComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlayerSide)
	{
		return;
	}

	if (!bInputBound)
	{
		TryBindInput();
	}

	if (!bContextRegistered)
	{
		RegisterInteractContext();
	}

	TimeSinceRefresh += DeltaTime;
	if (UpdateInterval <= 0.0f || TimeSinceRefresh >= UpdateInterval)
	{
		TimeSinceRefresh = 0.0f;
		RefreshFocus();
	}
}

void UInteractionDetectorComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		OutLocation = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	OutLocation = Owner->GetActorLocation();
	OutRotation = Owner->GetActorRotation();

	// Prefer the camera: "aimed at" must mean "under the crosshair". The capsule
	// forward vector never pitches, so it cannot express looking up or down.
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		}
	}
}

UInteractableComponent* UInteractionDetectorComponent::FindInteractableComponent(AActor* Target) const
{
	return Target ? Target->FindComponentByClass<UInteractableComponent>() : nullptr;
}

bool UInteractionDetectorComponent::IsInteractableTarget(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	// A hidden actor is not interactable. Without this, an interactable inside a group
	// that has not materialised yet (a proximity barrier wall, say) would still be
	// picked through the invisible interaction proxy box it owns.
	if (Target->IsHidden())
	{
		return false;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return true;
	}

	return FindInteractableComponent(Target) != nullptr;
}

bool UInteractionDetectorComponent::QueryCanInteract(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return IInteractableInterface::Execute_CanInteract(Target, GetOwner());
	}

	if (const UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		return Component->CanInteract(GetOwner());
	}

	return false;
}

void UInteractionDetectorComponent::QueryOnInteract(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnInteract(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyInteract(GetOwner());
	}
}

FText UInteractionDetectorComponent::QueryPrompt(AActor* Target) const
{
	if (!Target)
	{
		return FText::GetEmpty();
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		return IInteractableInterface::Execute_GetInteractionPrompt(Target);
	}

	if (const UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		return Component->GetInteractionPrompt();
	}

	return FText::GetEmpty();
}

void UInteractionDetectorComponent::QueryFocusBegin(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnFocusBegin(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyFocusGained(GetOwner());
	}
}

void UInteractionDetectorComponent::QueryFocusEnd(AActor* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->GetClass()->ImplementsInterface(UInteractableInterface::StaticClass()))
	{
		IInteractableInterface::Execute_OnFocusEnd(Target, GetOwner());
		return;
	}

	if (UInteractableComponent* Component = FindInteractableComponent(Target))
	{
		Component->NotifyFocusLost(GetOwner());
	}
}

void UInteractionDetectorComponent::TryInteract()
{
	if (!bPlayerSide)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Interaction] %s: TryInteract ignored - this detector is not on a Pawn."),
			*GetNameSafe(GetOwner()));
		return;
	}

	AActor* Target = FocusedActor;

	// The polled focus can be up to UpdateInterval seconds stale, and an object
	// that just reacted (moved, or had its collision switched off) may have been
	// dropped. Only re-search when the current focus is unusable - preferring it
	// avoids throwing away a perfectly good target just because a key was pressed.
	if (!Target || !QueryCanInteract(Target))
	{
		RefreshFocus();
		Target = FocusedActor;
	}

	if (!Target || !QueryCanInteract(Target))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Interaction] %s: interact pressed but nothing interactable is focused."),
			*GetNameSafe(GetOwner()));
		return;
	}

	QueryOnInteract(Target);

	// The interaction may have changed the state (e.g. Locked), so re-evaluate
	// the focus immediately instead of waiting for the next poll.
	RefreshFocus();
}

FText UInteractionDetectorComponent::GetCurrentPrompt() const
{
	return QueryPrompt(FocusedActor);
}

void UInteractionDetectorComponent::RefreshFocus()
{
	if (!GetOwner())
	{
		return;
	}

	TArray<AActor*> Candidates;
	GatherCandidates(Candidates);

	AActor* Picked = PickBestCandidate(Candidates);
	DebugDrawPick(Picked);
	SetFocusedActor(Picked);
}

void UInteractionDetectorComponent::DebugDrawPick(AActor* Picked) const
{
	if (!bDrawDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	const FVector End = Picked
		? Picked->GetActorLocation()
		: ViewLocation + ViewRotation.Vector() * TraceDistance;

	const float Duration = UpdateInterval > 0.0f ? UpdateInterval : 0.1f;
	const FColor Color = Picked ? FColor::Green : FColor::Red;

	// Note: GREEN now really means "a valid interactable was picked". The trace
	// itself is deliberately drawn without Kismet colours, since those are green
	// for ANY geometry hit and made an occluded door look like a passing check.
	DrawDebugLine(World, ViewLocation, End, Color, false, Duration, 0, 2.0f);
	DrawDebugPoint(World, End, 12.0f, Color, false, Duration);
}

void UInteractionDetectorComponent::GatherCandidates(TArray<AActor*>& OutCandidates)
{
	OutCandidates.Reset();

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(GetOwner());

	if (PickMode == EInteractionPickMode::SphereOverlap)
	{
		// Nearby: centred on the pawn, because "close enough" is about the body.
		UKismetSystemLibrary::SphereOverlapActors(
			this,
			Owner->GetActorLocation(),
			InteractionRadius,
			ProbeObjectTypes,
			nullptr,
			IgnoredActors,
			OutCandidates);
	}
	else
	{
		// Aimed: traced from the view point so it follows the crosshair.
		FVector ViewLocation;
		FRotator ViewRotation;
		GetViewPoint(ViewLocation, ViewRotation);

		const FVector End = ViewLocation + ViewRotation.Vector() * TraceDistance;

		if (bPierceOccluders)
		{
			// Collect EVERY interactable along the ray, not just the first hit.
			// The nearest hit is usually an occluder (a door frame jamb/rebate sits
			// in front of one face of a door), so taking only hit #1 made the door
			// impossible to open from that side. ScoreCandidate still decides which
			// candidate wins, so the aimed-at / nearest one is used.
			TArray<FHitResult> Hits;
			UKismetSystemLibrary::LineTraceMulti(
				this,
				ViewLocation,
				End,
				UEngineTypes::ConvertToTraceType(ECC_Visibility),
				false,
				IgnoredActors,
				EDrawDebugTrace::None,
				Hits,
				true);

			AActor* FirstOccluder = nullptr;
			for (const FHitResult& Hit : Hits)
			{
				AActor* HitActor = Hit.GetActor();
				if (!HitActor)
				{
					continue;
				}

				if (IsInteractableTarget(HitActor))
				{
					if (!OutCandidates.Contains(HitActor))
					{
						OutCandidates.Add(HitActor);
					}
				}
				else if (!FirstOccluder)
				{
					FirstOccluder = HitActor;
				}
			}

			// Say out loud what the ray had to get past, so a misidentified occluder
			// is visible in the log instead of only in the crosshair.
			if (OutCandidates.Num() > 0)
			{
				LastReportedBlocker.Reset();

				if (FirstOccluder)
				{
					UE_LOG(LogTemp, Log,
						TEXT("[Interaction] %s: pierced non-interactable '%s' to reach '%s'."),
						*GetNameSafe(GetOwner()), *FirstOccluder->GetName(), *OutCandidates[0]->GetName());
				}
			}
			else if (Hits.Num() > 0)
			{
				// Nothing interactable anywhere on the ray. Naming what it DID hit is the
				// fastest way to find out why an object is unreachable from one side
				// (a wall the door is sunk into, a frame casing, a glass pane...).
				AActor* Blocker = Hits[0].GetActor();
				if (Blocker && Blocker != LastReportedBlocker.Get())
				{
					LastReportedBlocker = Blocker;
					UE_LOG(LogTemp, Log,
						TEXT("[Interaction] %s: aimed pick found NO interactable - the ray hit '%s' first."),
						*GetNameSafe(GetOwner()), *Blocker->GetName());
				}
			}
		}
		else
		{
			FHitResult Hit;
			const bool bHit = UKismetSystemLibrary::LineTraceSingle(
				this,
				ViewLocation,
				End,
				UEngineTypes::ConvertToTraceType(ECC_Visibility),
				false,
				IgnoredActors,
				EDrawDebugTrace::None,
				Hit,
				true);

			if (bHit && Hit.GetActor())
			{
				OutCandidates.Add(Hit.GetActor());
			}
		}
	}

	// LineTrace found nothing: fall back to a short overlap so an unexpected
	// occluder cannot make a nearby, aimed-at object permanently unreachable.
	if (PickMode != EInteractionPickMode::SphereOverlap && bFallbackToOverlap && OutCandidates.Num() == 0)
	{
		UKismetSystemLibrary::SphereOverlapActors(
			this,
			Owner->GetActorLocation(),
			InteractionRadius,
			ProbeObjectTypes,
			nullptr,
			IgnoredActors,
			OutCandidates);

		if (OutCandidates.Num() > 0)
		{
			UE_LOG(LogTemp, Log,
				TEXT("[Interaction] %s: aimed pick found nothing, fell back to overlap r=%g (%d candidate(s))."),
				*GetNameSafe(GetOwner()), InteractionRadius, OutCandidates.Num());
		}
	}

	// Keep the CURRENT focus alive while it is still a legal target.
	// Without this, anything that moves out of the picking trace loses focus the
	// moment it reacts - a door swinging open leaves the crosshair, so the next
	// key press could never toggle it back to its initial state.
	AActor* Current = FocusedActor.Get();
	if (Current && !OutCandidates.Contains(Current) && IsStillValidTarget(Current))
	{
		OutCandidates.Add(Current);
	}
}

bool UInteractionDetectorComponent::IsStillValidTarget(AActor* Candidate) const
{
	if (!Candidate || Candidate == GetOwner())
	{
		return false;
	}

	if (!IsInteractableTarget(Candidate) || !QueryCanInteract(Candidate))
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
	const float Distance = ToCandidate.Size();

	// Generous bound covering both picking modes.
	if (Distance > FMath::Max(TraceDistance, InteractionRadius))
	{
		return false;
	}

	const float Dot = FVector::DotProduct(ViewRotation.Vector(), ToCandidate.GetSafeNormal());
	if (bRequireFacing && Dot < MinFacingCosine)
	{
		return false;
	}

	return true;
}

AActor* UInteractionDetectorComponent::PickBestCandidate(const TArray<AActor*>& Candidates) const
{
	AActor* Best = nullptr;
	float BestScore = -FLT_MAX;

	for (AActor* Candidate : Candidates)
	{
		const float Score = ScoreCandidate(Candidate);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

float UInteractionDetectorComponent::ScoreCandidate(AActor* Candidate) const
{
	if (!Candidate || Candidate == GetOwner())
	{
		return -FLT_MAX;
	}

	// Must actually be interactable right now.
	if (!IsInteractableTarget(Candidate) || !QueryCanInteract(Candidate))
	{
		return -FLT_MAX;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);

	const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
	const float Distance = ToCandidate.Size();
	const float Dot = FVector::DotProduct(ViewRotation.Vector(), ToCandidate.GetSafeNormal());

	// Reject anything the player is not aiming at closely enough.
	if (bRequireFacing && Dot < MinFacingCosine)
	{
		return -FLT_MAX;
	}

	// Aiming dominates, distance breaks ties.
	float Score = Dot * 1000.0f - Distance;

	// Keep the current focus unless a candidate is clearly better, so an object
	// that just reacted stays toggle-able.
	if (Candidate == FocusedActor.Get())
	{
		Score += FocusStickinessBonus;
	}

	return Score;
}

void UInteractionDetectorComponent::SetFocusedActor(AActor* NewFocus)
{
	if (NewFocus == FocusedActor)
	{
		return;
	}

	// Restore the outline BEFORE the old target undoes its own highlight, so the
	// two systems cannot leave a stale stencil behind.
	ClearFocusOutline();

	if (FocusedActor)
	{
		QueryFocusEnd(FocusedActor);
	}

	FocusedActor = NewFocus;

	if (FocusedActor)
	{
		QueryFocusBegin(FocusedActor);

		// The actor the player is aiming at gets the thick outline tier.
		ApplyFocusOutline(FocusedActor);
	}

	OnFocusChanged.Broadcast(FocusedActor, GetCurrentPrompt());
}

void UInteractionDetectorComponent::TryBindInput()
{
	if (bInputBound || !InteractAction)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Pawn->InputComponent))
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this,
			&UInteractionDetectorComponent::HandleInteractInput);
		bInputBound = true;
	}
}

void UInteractionDetectorComponent::RegisterInteractContext()
{
	if (bContextRegistered || !bRegisterInteractContext || !InteractAction)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		// Possession may not have happened yet; TickComponent retries.
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (!RuntimeInteractContext)
	{
		// Owned by this component so it stays alive for the session.
		RuntimeInteractContext = NewObject<UInputMappingContext>(
			this, TEXT("InteractionRuntimeContext"));
		RuntimeInteractContext->MapKey(InteractAction, InteractKey);
	}

	Subsystem->AddMappingContext(RuntimeInteractContext, InteractContextPriority);
	bContextRegistered = true;

	UE_LOG(LogTemp, Log,
		TEXT("[Interaction] %s registered runtime interact context (%s -> %s)."),
		*GetNameSafe(GetOwner()), *InteractKey.ToString(), *GetNameSafe(InteractAction));
}

void UInteractionDetectorComponent::HandleInteractInput()
{
	TryInteract();
}

// ---------------------------------------------------------------------------
// Focus outline: the aimed-at actor gets a THICKER outline than the resting one
// ---------------------------------------------------------------------------

void UInteractionDetectorComponent::PushOutlineThickness(float Thickness)
{
	if (!OutlineParameterCollection)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UKismetMaterialLibrary::SetScalarParameterValue(
			World, OutlineParameterCollection, OutlineThicknessParameter, Thickness);
	}
}

void UInteractionDetectorComponent::ApplyFocusOutline(AActor* Target)
{
	// Always start from a restored state so backups cannot stack up.
	ClearFocusOutline();

	if (!bApplyFocusOutline || !Target)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Primitives;
	Target->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive)
		{
			continue;
		}

		// Hidden collision proxies should not produce a stray outline.
		if (bOutlineVisiblePrimitivesOnly && !Primitive->IsVisible())
		{
			continue;
		}

		FInteractionOutlineBackup Backup;
		Backup.Component = Primitive;
		Backup.bRenderCustomDepth = Primitive->bRenderCustomDepth;
		Backup.StencilValue = Primitive->CustomDepthStencilValue;
		OutlineBackups.Add(Backup);

		if (bUseStencilOutline)
		{
			// Raise to the focused tier: the outline material branches on the
			// stencil value to draw a thicker line.
			Primitive->SetRenderCustomDepth(true);
			Primitive->SetCustomDepthStencilValue(FocusedOutlineStencil);
		}
	}

	PushOutlineThickness(FocusedOutlineThickness);
}

void UInteractionDetectorComponent::ClearFocusOutline()
{
	for (FInteractionOutlineBackup& Backup : OutlineBackups)
	{
		if (UPrimitiveComponent* Primitive = Backup.Component.Get())
		{
			if (bUseStencilOutline)
			{
				Primitive->SetRenderCustomDepth(Backup.bRenderCustomDepth);
				Primitive->SetCustomDepthStencilValue(Backup.StencilValue);
			}
		}
	}

	OutlineBackups.Reset();

	// Back to the resting (thin) thickness.
	PushOutlineThickness(RestingOutlineThickness);
}
