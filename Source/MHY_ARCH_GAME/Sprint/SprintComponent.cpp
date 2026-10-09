// Copyright Epic Games, Inc. All Rights Reserved.

#include "SprintComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"

USprintComponent::USprintComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SprintKey = EKeys::LeftShift;
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void USprintComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		// Same reasoning as the interaction detector: a player-side component on a
		// non-Pawn owner can neither bind input nor find a controller.
		UE_LOG(LogTemp, Warning,
			TEXT("[Sprint] %s: SprintComponent 只能挂在 Pawn/Character 上，本组件已自行停用。"),
			*GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if (Movement)
	{
		AuthoredMaxWalkSpeed = Movement->MaxWalkSpeed;
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Sprint] %s: owner 上没有 CharacterMovementComponent，疾跑不会有任何效果。"),
			*GetNameSafe(GetOwner()));
	}

	// Build the fallback action up front so context registration and input binding do
	// not depend on each other's ordering.
	if (!SprintAction)
	{
		RuntimeSprintAction = NewObject<UInputAction>(this, TEXT("SprintRuntimeAction"));
		RuntimeSprintAction->ValueType = EInputActionValueType::Boolean;
	}

	TryBindInput();
	RegisterSprintContext();

	if (bLogStateChanges)
	{
		UE_LOG(LogTemp, Log, TEXT("[Sprint] %s: 就绪（%s，键 %s，原始 MaxWalkSpeed %.0f -> 疾跑 %.0f）。"),
			*GetNameSafe(GetOwner()),
			bHoldToSprint ? TEXT("按住") : TEXT("切换"),
			*SprintKey.ToString(),
			AuthoredMaxWalkSpeed,
			SprintSpeed);
	}
}

void USprintComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave the character sprinting (or at a changed speed) after teardown.
	bSprinting = false;
	bSprintRequested = false;

	if (Movement && AuthoredMaxWalkSpeed > 0.0f)
	{
		Movement->MaxWalkSpeed = AuthoredMaxWalkSpeed;
	}

	if (bContextRegistered)
	{
		if (const APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			{
				if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
				{
					if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
						LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
					{
						if (RuntimeSprintContext)
						{
							Subsystem->RemoveMappingContext(RuntimeSprintContext);
						}
					}
				}
			}
		}
		bContextRegistered = false;
	}

	Super::EndPlay(EndPlayReason);
}

void USprintComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Binding and context registration may have to wait for possession.
	if (!bInputBound)
	{
		TryBindInput();
	}
	if (!bContextRegistered)
	{
		RegisterSprintContext();
	}

	const bool bSetupDone = bInputBound && (!bRegisterSprintContext || bContextRegistered);

	// Nothing left to poll? Then stop ticking entirely.
	if (bSetupDone && !bRequireForwardInput && !bDrawOnScreenDebug)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// Conditions (crouch / direction / air) can change without a new key event.
	EvaluateSprintState();

	if (bDrawOnScreenDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(reinterpret_cast<uint64>(this), 0.0f,
			bSprinting ? FColor::Green : FColor::Cyan, GetSprintDebugString());
	}
}

// ---------------------------------------------------------------------------
// state
// ---------------------------------------------------------------------------

void USprintComponent::EvaluateSprintState()
{
	if (!bSprintEnabled)
	{
		SetSprinting(false);
		return;
	}

	bool bWanted = bSprintRequested;

	if (bIgnoreWhenCrouched)
	{
		if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
		{
			if (Character->bIsCrouched)
			{
				bWanted = false;
			}
		}
	}

	if (bWanted && !bKeepSpeedInAir && Movement && Movement->IsFalling())
	{
		bWanted = false;
	}

	if (bWanted && bRequireForwardInput && !IsMovingForward())
	{
		bWanted = false;
	}

	SetSprinting(bWanted);
}

void USprintComponent::SetSprinting(bool bNewSprinting)
{
	if (bSprinting == bNewSprinting)
	{
		return;
	}

	bSprinting = bNewSprinting;
	ApplySpeed();

	if (bLogStateChanges)
	{
		UE_LOG(LogTemp, Log, TEXT("[Sprint] %s: %s (MaxWalkSpeed -> %.0f)"),
			*GetNameSafe(GetOwner()),
			bSprinting ? TEXT("进入疾跑") : TEXT("退出疾跑"),
			Movement ? Movement->MaxWalkSpeed : -1.0f);
	}

	OnSprintChanged.Broadcast(bSprinting);
}

void USprintComponent::ApplySpeed()
{
	if (!Movement)
	{
		return;
	}

	// Sprint -> SprintSpeed. Otherwise restore the speed the character had before this
	// component touched it (or an explicit override), so other systems that also adjust
	// movement speed are not permanently clobbered.
	const float Target = bSprinting
		? SprintSpeed
		: (NormalSpeedOverride > 0.0f ? NormalSpeedOverride : AuthoredMaxWalkSpeed);

	Movement->MaxWalkSpeed = Target;
}

bool USprintComponent::IsMovingForward() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Movement)
	{
		return true;
	}

	const FVector Acceleration = Movement->GetCurrentAcceleration();
	if (Acceleration.SizeSquared() < 1.0f)
	{
		// No input at all: keep whatever we already decided instead of dropping out of
		// the sprint the instant the player lets go of the stick for a frame.
		return bSprinting;
	}

	const float Dot = FVector::DotProduct(Acceleration.GetSafeNormal(), Owner->GetActorForwardVector());
	return Dot >= MinForwardDot;
}

// ---------------------------------------------------------------------------
// input
// ---------------------------------------------------------------------------

void USprintComponent::HandleSprintPressed()
{
	if (bHoldToSprint)
	{
		bSprintRequested = true;
	}
	else
	{
		// Toggle mode: this fires once per press, so flipping here is correct.
		bSprintRequested = !bSprintRequested;
	}

	EvaluateSprintState();
}

void USprintComponent::HandleSprintHeld()
{
	// Fires every frame while the key is down. In toggle mode this must be ignored,
	// otherwise the state would flip on every frame.
	if (bHoldToSprint)
	{
		bSprintRequested = true;
		EvaluateSprintState();
	}
}

void USprintComponent::HandleSprintReleased()
{
	if (bHoldToSprint)
	{
		bSprintRequested = false;
		EvaluateSprintState();
	}
}

void USprintComponent::TryBindInput()
{
	if (bInputBound)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	if (!EnhancedInput)
	{
		// InputComponent is created on possession; TickComponent retries.
		return;
	}

	UInputAction* Action = SprintAction ? SprintAction.Get() : RuntimeSprintAction.Get();
	if (!Action)
	{
		return;
	}

	EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &USprintComponent::HandleSprintPressed);
	EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &USprintComponent::HandleSprintHeld);
	EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, &USprintComponent::HandleSprintReleased);

	bInputBound = true;
	UE_LOG(LogTemp, Log, TEXT("[Sprint] %s: 已绑定疾跑输入。"), *GetNameSafe(GetOwner()));
}

void USprintComponent::RegisterSprintContext()
{
	if (bContextRegistered || !bRegisterSprintContext)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (!LocalPlayer)
	{
		// Not possessed yet; TickComponent retries.
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UInputAction* Action = SprintAction ? SprintAction.Get() : RuntimeSprintAction.Get();
	if (!Action)
	{
		return;
	}

	if (!RuntimeSprintContext)
	{
		// Owned by this component so it lives as long as we do.
		RuntimeSprintContext = NewObject<UInputMappingContext>(this, TEXT("SprintRuntimeContext"));
		RuntimeSprintContext->MapKey(Action, SprintKey);

		if (bIncludeRightShift && SprintKey == EKeys::LeftShift)
		{
			RuntimeSprintContext->MapKey(Action, EKeys::RightShift);
		}
	}

	Subsystem->AddMappingContext(RuntimeSprintContext, SprintContextPriority);
	bContextRegistered = true;

	UE_LOG(LogTemp, Log, TEXT("[Sprint] %s: 已注册运行时疾跑 context（%s%s）。"),
		*GetNameSafe(GetOwner()), *SprintKey.ToString(),
		bIncludeRightShift && SprintKey == EKeys::LeftShift ? TEXT(" / RightShift") : TEXT(""));
}

FString USprintComponent::GetSprintDebugString() const
{
	return FString::Printf(
		TEXT("Sprint %s | sprinting=%s | requested=%s | speed=%.0f (authored %.0f, sprint %.0f) | key=%s | bound=%s ctx=%s"),
		*GetNameSafe(GetOwner()),
		bSprinting ? TEXT("YES") : TEXT("no"),
		bSprintRequested ? TEXT("yes") : TEXT("no"),
		Movement ? Movement->MaxWalkSpeed : -1.0f,
		AuthoredMaxWalkSpeed,
		SprintSpeed,
		*SprintKey.ToString(),
		bInputBound ? TEXT("yes") : TEXT("no"),
		bContextRegistered ? TEXT("yes") : TEXT("no"));
}
