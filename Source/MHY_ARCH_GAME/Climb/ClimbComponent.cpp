// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClimbComponent.h"

#include "ClimbSpot.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"

UClimbComponent::UClimbComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while climbing
}

void UClimbComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveCharacter();
}

void UClimbComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave the character locked if we are torn down mid-climb.
	if (bClimbing)
	{
		RestoreLocomotion();
		bClimbing = false;
	}

	if (CachedCharacter && CachedCharacter->GetMesh())
	{
		if (UAnimInstance* AnimInstance = CachedCharacter->GetMesh()->GetAnimInstance())
		{
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UClimbComponent::HandleMontageEnded);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UClimbComponent::ResolveCharacter()
{
	if (!CachedCharacter)
	{
		CachedCharacter = Cast<ACharacter>(GetOwner());
	}

	if (!CachedWarping && CachedCharacter)
	{
		CachedWarping = CachedCharacter->FindComponentByClass<UMotionWarpingComponent>();
	}
}

void UClimbComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bClimbing || !bCurveDriven || !CachedCharacter)
	{
		return;
	}

	ElapsedTime += DeltaTime;

	const float Alpha = (ClimbDuration > KINDA_SMALL_NUMBER)
		? FMath::Clamp(ElapsedTime / ClimbDuration, 0.0f, 1.0f)
		: 1.0f;

	// Vertical profile: designer curve if present, smoothstep otherwise.
	const float CurveAlpha = ClimbHeightCurve
		? FMath::Clamp(ClimbHeightCurve->GetFloatValue(Alpha), 0.0f, 1.0f)
		: FMath::SmoothStep(0.0f, 1.0f, Alpha);

	// Only Z changes: the destination keeps the start X/Y by design.
	const FVector NewLocation(
		StartLocation.X,
		StartLocation.Y,
		FMath::Lerp(StartLocation.Z, TargetLocation.Z, CurveAlpha));

	CachedCharacter->SetActorLocation(NewLocation, /*bSweep=*/false, nullptr,
		ETeleportType::TeleportPhysics);

	if (Alpha >= 1.0f)
	{
		FinishClimb(bSnapToTargetOnFinish);
	}
}

bool UClimbComponent::TryClimb(AClimbSpot* Spot)
{
	ResolveCharacter();

	if (bClimbing || !Spot || !CachedCharacter || Spot->IsConsumed())
	{
		return false;
	}

	UAnimMontage* Montage = Spot->ClimbMontageOverride ? Spot->ClimbMontageOverride : ClimbMontage;
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Climb] %s has no climb montage (neither on the component nor on the spot)."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// The character must be on the ground to start a climb.
	const UCharacterMovementComponent* Movement = CachedCharacter->GetCharacterMovement();
	if (!Movement || !Movement->IsMovingOnGround())
	{
		return false;
	}

	ActiveSpot = Spot;

	// 1) Align so the animation lines up with the ledge.
	ApplyAlignment(Spot);

	// 2) Resolve the destination: same X/Y, platform top + capsule half height.
	float StepHeight = 0.0f;
	if (!ComputeTargetLocation(Spot, StepHeight))
	{
		ActiveSpot = nullptr;
		return false;
	}

	// 3) Lock locomotion and play.
	StartLocation = CachedCharacter->GetActorLocation();
	ElapsedTime = 0.0f;

	LockLocomotion();

	// 4) Displacement source: root motion + warping if available, otherwise curve.
	bCurveDriven = false;
	if (MoveMode == EClimbMoveMode::MotionWarping)
	{
		if (CachedWarping)
		{
			CachedWarping->AddOrUpdateWarpTargetFromLocationAndRotation(
				WarpTargetName, TargetLocation, CachedCharacter->GetActorRotation());
		}
		else
		{
			// No MotionWarping component on the character -> keep the feature usable.
			UE_LOG(LogTemp, Warning,
				TEXT("[Climb] %s has no MotionWarping component; falling back to curve-driven climb."),
				*GetNameSafe(GetOwner()));
			bCurveDriven = true;
		}
	}
	else
	{
		bCurveDriven = true;
	}

	ClimbDuration = CachedCharacter->PlayAnimMontage(Montage, MontagePlayRate);

	// Root motion drives the visual; the montage end is what completes the move.
	if (UAnimInstance* AnimInstance = CachedCharacter->GetMesh() ? CachedCharacter->GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &UClimbComponent::HandleMontageEnded);
	}

	if (bCurveDriven && ClimbDuration <= KINDA_SMALL_NUMBER)
	{
		// No montage length available: use a sensible default so the move still completes.
		ClimbDuration = 1.0f;
	}

	bClimbing = true;
	SetComponentTickEnabled(bCurveDriven);

	OnClimbStarted.Broadcast();
	return true;
}

void UClimbComponent::ApplyAlignment(const AClimbSpot* Spot)
{
	if (!Spot || !Spot->bAlignCharacterOnStart || !CachedCharacter)
	{
		return;
	}

	const FTransform AlignTransform = Spot->GetAlignTransform();

	FRotator NewRotation = CachedCharacter->GetActorRotation();
	if (Spot->bAlignCharacterRotation)
	{
		NewRotation = AlignTransform.Rotator();
	}

	CachedCharacter->SetActorLocationAndRotation(
		AlignTransform.GetLocation(), NewRotation, /*bSweep=*/false, nullptr,
		ETeleportType::TeleportPhysics);
}

bool UClimbComponent::ComputeTargetLocation(const AClimbSpot* Spot, float& OutStepHeight)
{
	OutStepHeight = 0.0f;

	if (!CachedCharacter)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = CachedCharacter->GetCapsuleComponent();
	if (!World || !Capsule)
	{
		return false;
	}

	const FVector Origin = CachedCharacter->GetActorLocation();

	// Probe straight down at the SAME X/Y, starting above the character.
	const FVector TraceStart(Origin.X, Origin.Y, Origin.Z + TraceUpHeight);
	const FVector TraceEnd(Origin.X, Origin.Y, Origin.Z);

	FCollisionQueryParams Params(FName(TEXT("ClimbLedgeProbe")), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(CachedCharacter);

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, LedgeTraceChannel, Params);

	// No platform above -> refuse the climb (never move the character into the void).
	if (!bHit)
	{
		return false;
	}

	const float PlatformTopZ = Hit.ImpactPoint.Z;
	const float StepHeight = PlatformTopZ - Origin.Z;

	// Must actually go up, and not exceed what this spot allows.
	if (StepHeight <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (Spot && StepHeight > Spot->MaxClimbHeight)
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Destination: identical X/Y, standing on the platform top.
	TargetLocation = FVector(Origin.X, Origin.Y, PlatformTopZ + HalfHeight);
	OutStepHeight = StepHeight;
	return true;
}

void UClimbComponent::LockLocomotion()
{
	if (!CachedCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = CachedCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		// Flying keeps root motion applying while ignoring ground locomotion.
		Movement->SetMovementMode(MOVE_Flying);
	}

	if (bLockRotationDuringClimb)
	{
		bSavedUseControllerRotationYaw = CachedCharacter->bUseControllerRotationYaw;
		CachedCharacter->bUseControllerRotationYaw = false;
	}
}

void UClimbComponent::RestoreLocomotion()
{
	if (!CachedCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = CachedCharacter->GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (bLockRotationDuringClimb)
	{
		CachedCharacter->bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
	}
}

void UClimbComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bClimbing)
	{
		return;
	}

	// Interrupted montages still finish the move when a target was resolved, so the
	// character never gets stranded halfway up the wall.
	FinishClimb(bSnapToTargetOnFinish);
}

void UClimbComponent::FinishClimb(bool bSnapToTarget)
{
	if (!bClimbing)
	{
		return;
	}

	bClimbing = false;
	SetComponentTickEnabled(false);

	if (bSnapToTarget && CachedCharacter)
	{
		// Exact landing: X/Y identical to the start, Z on the platform top.
		CachedCharacter->SetActorLocation(TargetLocation, /*bSweep=*/false, nullptr,
			ETeleportType::TeleportPhysics);
	}

	if (CachedWarping)
	{
		CachedWarping->RemoveWarpTarget(WarpTargetName);
	}

	RestoreLocomotion();

	if (ActiveSpot)
	{
		ActiveSpot->NotifyClimbed();
		ActiveSpot = nullptr;
	}

	OnClimbFinished.Broadcast();
}

void UClimbComponent::CancelClimb()
{
	if (!bClimbing)
	{
		return;
	}

	bClimbing = false;
	SetComponentTickEnabled(false);

	if (CachedWarping)
	{
		CachedWarping->RemoveWarpTarget(WarpTargetName);
	}

	RestoreLocomotion();
	ActiveSpot = nullptr;

	OnClimbFinished.Broadcast();
}
