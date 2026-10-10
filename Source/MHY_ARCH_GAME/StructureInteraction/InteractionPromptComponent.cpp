// Copyright Epic Games, Inc. All Rights Reserved.

#include "InteractionPromptComponent.h"

#include "InteractionDetectorComponent.h"
#include "InteractionPromptWidget.h"

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"

UInteractionPromptComponent::UInteractionPromptComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractionPromptComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoBindDetector)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Detector = Owner->FindComponentByClass<UInteractionDetectorComponent>();
	if (Detector)
	{
		Detector->OnFocusChanged.AddDynamic(this, &UInteractionPromptComponent::HandleFocusChanged);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[InteractionPrompt] %s: 没有找到 UInteractionDetectorComponent，聚焦提示不会工作。")
			TEXT("把它挂到同一个角色上，或关掉 bAutoBindDetector 自己调 ShowPrompt/HidePrompt。"),
			*GetNameSafe(Owner));
	}
}

void UInteractionPromptComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Detector)
	{
		Detector->OnFocusChanged.RemoveDynamic(this, &UInteractionPromptComponent::HandleFocusChanged);
	}

	if (IsValid(ScreenWidget))
	{
		ScreenWidget->RemoveFromParent();
	}
	ScreenWidget = nullptr;

	if (IsValid(PromptWidgetComponent))
	{
		PromptWidgetComponent->DestroyComponent();
	}
	PromptWidgetComponent = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UInteractionPromptComponent::HandleFocusChanged(AActor* InFocusActor, FText Prompt)
{
	FocusedActor = InFocusActor;

	// Nothing focused -> hide. Focused but the object has no text -> fall back to the
	// component default, and if that is empty too, show nothing at all.
	if (!InFocusActor)
	{
		HidePrompt();
		return;
	}

	if (Prompt.IsEmpty())
	{
		Prompt = DefaultPromptText;
	}

	if (Prompt.IsEmpty())
	{
		HidePrompt();
		return;
	}

	ShowPrompt(Prompt);
}

TSubclassOf<UInteractionPromptWidget> UInteractionPromptComponent::ResolveWidgetClass() const
{
	TSubclassOf<UInteractionPromptWidget> Class = WidgetClass;
	if (!Class && bUseBuiltInFallback)
	{
		Class = UInteractionPromptFallbackWidget::StaticClass();
	}
	return Class;
}

void UInteractionPromptComponent::EnsurePrompt()
{
	const TSubclassOf<UInteractionPromptWidget> Class = ResolveWidgetClass();
	if (!Class)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[InteractionPrompt] %s: WidgetClass 为空且 bUseBuiltInFallback 关着，提示不会显示。"),
			*GetNameSafe(GetOwner()));
		return;
	}

	if (bWorldSpacePrompt)
	{
		AActor* Owner = GetOwner();
		if (!Owner || !Owner->GetRootComponent())
		{
			return;
		}

		if (!PromptWidgetComponent)
		{
			PromptWidgetComponent = NewObject<UWidgetComponent>(Owner, TEXT("InteractionPromptWidget"));
			PromptWidgetComponent->SetupAttachment(Owner->GetRootComponent());
			PromptWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
			PromptWidgetComponent->SetDrawSize(WorldPromptDrawSize);
			PromptWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
			// Two-sided so the text is never invisible just because of the facing sign.
			PromptWidgetComponent->SetTwoSided(true);
			PromptWidgetComponent->SetTickWhenOffscreen(false);
			PromptWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PromptWidgetComponent->SetGenerateOverlapEvents(false);
			PromptWidgetComponent->RegisterComponent();
			PromptWidgetComponent->SetVisibility(false);
		}

		if (!PromptWidgetComponent->GetUserWidgetObject())
		{
			PromptWidgetComponent->SetWidgetClass(Class);
			PromptWidgetComponent->InitWidget();
		}
		return;
	}

	// -- screen space (the previous behaviour) ------------------------------
	if (IsValid(ScreenWidget))
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		// HUD is a local thing: a server-side / unpossessed owner has nothing to draw on.
		return;
	}

	ScreenWidget = CreateWidget<UUserWidget>(PlayerController, Class);
	if (!ScreenWidget)
	{
		return;
	}

	ScreenWidget->AddToViewport(ZOrder);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ScreenWidget->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetPosition(FVector2D(0.0f, ScreenOffsetY));
		CanvasSlot->SetAutoSize(true);
	}

	ScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
}

UUserWidget* UInteractionPromptComponent::GetPromptUserWidget() const
{
	if (bWorldSpacePrompt)
	{
		return PromptWidgetComponent ? PromptWidgetComponent->GetUserWidgetObject() : nullptr;
	}
	return ScreenWidget;
}

void UInteractionPromptComponent::ApplyToWidget(const FText& PromptText, bool bVisible)
{
	EnsurePrompt();

	UUserWidget* Widget = GetPromptUserWidget();
	if (!Widget)
	{
		return;
	}

	// 1) The Blueprint contract: a WBP derived from UInteractionPromptWidget implements it.
	if (UInteractionPromptWidget* Typed = Cast<UInteractionPromptWidget>(Widget))
	{
		Typed->SetPrompt(PromptText, bVisible);
	}

	// 2) The built-in popup draws itself (calling the BIE above is a harmless no-op there).
	if (UInteractionPromptFallbackWidget* Fallback = Cast<UInteractionPromptFallbackWidget>(Widget))
	{
		Fallback->ApplyPrompt(PromptText, bVisible);
	}

	// 3) Visibility differs per mode: the world widget lives on a UWidgetComponent,
	//    the screen widget is a viewport widget.
	if (bWorldSpacePrompt)
	{
		if (PromptWidgetComponent)
		{
			PromptWidgetComponent->SetVisibility(bVisible);
		}
		SetComponentTickEnabled(bVisible);
		if (bVisible)
		{
			UpdateWorldPromptTransform();
		}
	}
	else
	{
		Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UInteractionPromptComponent::UpdateWorldPromptTransform()
{
	if (!bWorldSpacePrompt || !PromptWidgetComponent)
	{
		return;
	}

	AActor* Target = FocusedActor;
	if (!Target)
	{
		PromptWidgetComponent->SetVisibility(false);
		return;
	}

	// Anchor: just above the object and pushed towards the viewer, so the prompt reads as
	// "in front of the object" instead of tracking the camera.
	FVector Origin = Target->GetActorLocation();
	FVector Extent = FVector::ZeroVector;
	Target->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent);

	FVector ViewLocation = Origin + FVector(0.0f, 0.0f, 200.0f);
	FRotator ViewRotation = FRotator::ZeroRotator;

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController()))
		{
			PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	FVector ToViewer = ViewLocation - Origin;
	ToViewer.Z = 0.0f;
	if (!ToViewer.Normalize())
	{
		ToViewer = FVector::ForwardVector;
	}

	const FVector WorldLocation =
		Origin
		+ FVector(0.0f, 0.0f, Extent.Z + WorldPromptHeightOffset)
		+ ToViewer * (WorldPromptFrontOffset + Extent.GetAbsMax() * 0.25f);

	FRotator WorldRotation = PromptWidgetComponent->GetComponentRotation();
	if (bWorldPromptFaceCamera)
	{
		WorldRotation = UKismetMathLibrary::FindLookAtRotation(WorldLocation, ViewLocation);
		WorldRotation.Yaw += WorldPromptFacingYaw;
	}

	PromptWidgetComponent->SetWorldLocationAndRotation(WorldLocation, WorldRotation);
}

void UInteractionPromptComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bPromptVisible)
	{
		UpdateWorldPromptTransform();
	}
}

void UInteractionPromptComponent::ShowPrompt(const FText& PromptText)
{
	CurrentPrompt = PromptText;
	ApplyToWidget(PromptText, /*bVisible=*/true);

	if (!bPromptVisible)
	{
		bPromptVisible = true;
		OnPromptChanged.Broadcast(PromptText, true);
	}
}

void UInteractionPromptComponent::HidePrompt()
{
	if (!bPromptVisible && !PromptWidgetComponent && !IsValid(ScreenWidget))
	{
		return;
	}

	bPromptVisible = false;
	CurrentPrompt = FText::GetEmpty();
	ApplyToWidget(FText::GetEmpty(), /*bVisible=*/false);
	OnPromptChanged.Broadcast(FText::GetEmpty(), false);
}
