// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiquidLightSurface.h"

#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ALiquidLightSurface::ALiquidLightSurface()
{
	PrimaryActorTick.bCanEverTick = true;

	FluidSurface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FluidSurface"));
	RootComponent = FluidSurface;
	FluidSurface->SetMobility(EComponentMobility::Movable);
	// The surface is driven entirely by the material; physical/collision response can be
	// enabled on a Blueprint subclass if needed (this demo keeps it visual-only).
	FluidSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FluidSurface->SetCastShadow(false);
	FluidSurface->SetVisibility(true);

	// Optional light, hidden unless the user enables it via bDriveLightWithFlow.
	OptionalLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OptionalLight"));
	OptionalLight->SetupAttachment(FluidSurface);
	OptionalLight->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	OptionalLight->SetIntensity(0.0f);
	OptionalLight->SetVisibility(false);
}

void ALiquidLightSurface::BeginPlay()
{
	Super::BeginPlay();

	if (FluidSurface && FluidSurface->GetNumMaterials() > 0)
	{
		if (UMaterialInterface* BaseMat = FluidSurface->GetMaterial(0))
		{
			SurfaceMID = FluidSurface->CreateDynamicMaterialInstance(0, BaseMat);
		}
	}

	CurrentFlow = 0.0f;
	if (bStartActive)
	{
		SetFlowActive(true);
	}
	else
	{
		TargetFlow = 0.0f;
		CurrentFlow = 0.0f;
	}
	RefreshSurface();
}

void ALiquidLightSurface::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyEqual(CurrentFlow, TargetFlow, 0.001f))
	{
		CurrentFlow = FMath::FInterpConstantTo(CurrentFlow, TargetFlow, DeltaSeconds, FadeSpeed);
		RefreshSurface();
	}
}

void ALiquidLightSurface::SetFlowActive(bool bActive)
{
	bFlowActive = bActive;
	bManualStrength = false;
	TargetFlow = bActive ? MaxFlowStrength : 0.0f;
	RefreshSurface();
}

void ALiquidLightSurface::SetFlowStrength(float Strength)
{
	bFlowActive = Strength > KINDA_SMALL_NUMBER;
	bManualStrength = true;
	TargetFlow = FMath::Max(0.0f, Strength);
	RefreshSurface();
}

void ALiquidLightSurface::RefreshSurface()
{
	if (!FluidSurface)
	{
		return;
	}

	const float Flow = FMath::Clamp(CurrentFlow, 0.0f, 1.0f);

	// Update material params on the dynamic instance.
	if (SurfaceMID)
	{
		SurfaceMID->SetScalarParameterValue(FlowStrengthParameter, Flow);
	}

	// Visibility / opacity handling.
	if (bHideWhenInactive && !bFlowActive && CurrentFlow <= KINDA_SMALL_NUMBER)
	{
		FluidSurface->SetVisibility(false);
		if (SurfaceMID)
		{
			SurfaceMID->SetScalarParameterValue(OpacityParameter, 0.0f);
		}
	}
	else
	{
		FluidSurface->SetVisibility(true);
		if (SurfaceMID)
		{
			SurfaceMID->SetScalarParameterValue(OpacityParameter, FMath::Max(Flow, 0.01f));
		}
	}

	// Drive the optional light, if in use.
	if (OptionalLight)
	{
		if (bDriveLightWithFlow)
		{
			OptionalLight->SetVisibility(Flow > KINDA_SMALL_NUMBER);
			OptionalLight->SetIntensity(LightPeakIntensity * Flow);
		}
		else
		{
			OptionalLight->SetVisibility(false);
		}
	}
}
