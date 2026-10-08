// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevealPlatform.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"

ARevealPlatform::ARevealPlatform()
{
	PrimaryActorTick.bCanEverTick = false;

	SolidBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SolidBox"));
	RootComponent = SolidBox;
	SolidBox->SetBoxExtent(FVector(200.0f, 200.0f, 10.0f));
	// Impassable by default: physical collision off (characters fall through / hit nothing).
	SolidBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SolidBox->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	// Detector volume overlaps pawns (no physical blocking) and extends upward so a
	// standing character's capsule remains inside it while the player rests on us.
	DetectorBox = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectorBox"));
	DetectorBox->SetupAttachment(SolidBox);
	DetectorBox->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f)); // center above the surface
	DetectorBox->SetBoxExtent(FVector(220.0f, 220.0f, 90.0f));
	DetectorBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	DetectorBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DetectorBox->SetGenerateOverlapEvents(true);
	DetectorBox->OnComponentBeginOverlap.AddDynamic(this, &ARevealPlatform::OnDetectorOverlapBegin);
	DetectorBox->OnComponentEndOverlap.AddDynamic(this, &ARevealPlatform::OnDetectorOverlapEnd);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(SolidBox);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetVisibility(false);
}

void ARevealPlatform::BeginPlay()
{
	Super::BeginPlay();

	if (VisualMesh && VisualMesh->GetNumMaterials() > 0)
	{
		if (UMaterialInterface* BaseMat = VisualMesh->GetMaterial(0))
		{
			SurfaceMID = VisualMesh->CreateDynamicMaterialInstance(0, BaseMat);
		}
	}
}

void ARevealPlatform::RevealOn()
{
	if (RevealRefCount <= 0)
	{
		RevealRefCount = 1;
		bUnrevealPending = false;
	}
	else
	{
		++RevealRefCount; // another light source is also holding us
		return;
	}

	if (bRevealed)
	{
		return;
	}

	bRevealed = true;

	// Become passable and show the surface fully lit.
	SolidBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VisualMesh->SetVisibility(true);
	ApplyAlpha(1.0f);
}

void ARevealPlatform::RevealOff()
{
	if (RevealRefCount <= 0)
	{
		return;
	}

	if (--RevealRefCount > 0)
	{
		return; // another light source still holds us lit
	}

	if (bStaysRevealed)
	{
		return; // one-way gate
	}

	TryUnreveal();
}

void ARevealPlatform::TryUnreveal()
{
	// If a character is standing on (above) us, postpone hiding until they leave.
	if (StandingPawnCount > 0)
	{
		bUnrevealPending = true;
		return;
	}

	bRevealed = false;
	bUnrevealPending = false;

	// Back to impassable: hide the surface and drop physical collision.
	VisualMesh->SetVisibility(false);
	ApplyAlpha(0.0f);
	SolidBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARevealPlatform::ApplyAlpha(float Alpha)
{
	if (SurfaceMID)
	{
		SurfaceMID->SetScalarParameterValue(OpacityParameterName, Alpha);
	}
}

void ARevealPlatform::OnDetectorOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<APawn>(OtherActor))
	{
		++StandingPawnCount;
	}
}

void ARevealPlatform::OnDetectorOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (Cast<APawn>(OtherActor))
	{
		if (StandingPawnCount > 0)
		{
			--StandingPawnCount;
		}

		// If we were waiting for the player to leave, we can safely hide now.
		if (bUnrevealPending && StandingPawnCount == 0)
		{
			TryUnreveal();
		}
	}
}
