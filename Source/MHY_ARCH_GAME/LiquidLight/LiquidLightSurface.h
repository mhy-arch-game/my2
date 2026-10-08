// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LiquidLightSurface.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

/**
 *  LiquidLightSurface - a standalone "selected light rendered as flowing liquid".
 *
 *  An independent demonstration host (not coupled to the LightReveal layer).
 *  It is a flat surface (StaticMesh + M_LiquidGlowFlow material) that, when its
 *  flow is activated, looks like a gently self-flowing glowing liquid.
 *
 *  - Material self-flow is the primary visual (see Docs/LiquidLight.md for the
 *    M_LiquidGlowFlow recipe). No Niagara is used by this class.
 *  - An optional PointLight can be attached and driven together with the flow,
 *    so the whole thing reads as "a light wrapped in liquid".
 *  - Minimal class tree (per review): only this AActor, no extra interface, so
 *    it stays a standalone reusable demo. If it later needs to join LightReveal,
 *    it can implement IRevealableInterface without changing its public API.
 */
UCLASS()
class ALiquidLightSurface : public AActor
{
	GENERATED_BODY()

public:
	ALiquidLightSurface();

	/** Turn the liquid-flow look on (flowing + fully visible) or off (calm / hidden). */
	UFUNCTION(BlueprintCallable, Category="LiquidLight")
	void SetFlowActive(bool bActive);

	/** True while the flow look is active. */
	UFUNCTION(BlueprintPure, Category="LiquidLight")
	bool IsFlowActive() const { return bFlowActive; }

	/** Directly set the flow-drive strength (0..~1). Overrides the automatic value. */
	UFUNCTION(BlueprintCallable, Category="LiquidLight")
	void SetFlowStrength(float Strength);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// -- editor tuning -----------------------------------------------------
	/** Should the surface start flowing on BeginPlay. */
	UPROPERTY(EditAnywhere, Category="LiquidLight")
	bool bStartActive = false;

	/** Hide the surface entirely when flow is inactive (vs. just calm). */
	UPROPERTY(EditAnywhere, Category="LiquidLight")
	bool bHideWhenInactive = true;

	/** Flow-drive value reached when active (passed to the FlowStrength material parameter). */
	UPROPERTY(EditAnywhere, Category="LiquidLight", meta=(ClampMin="0.0"))
	float MaxFlowStrength = 1.0f;

	/** How fast the visual ramps toward its target (units / second). */
	UPROPERTY(EditAnywhere, Category="LiquidLight", meta=(ClampMin="0.01"))
	float FadeSpeed = 2.0f;

	/** Material scalar parameter controlling flow intensity. */
	UPROPERTY(EditAnywhere, Category="LiquidLight")
	FName FlowStrengthParameter = TEXT("FlowStrength");

	/** Material scalar parameter controlling surface opacity. */
	UPROPERTY(EditAnywhere, Category="LiquidLight")
	FName OpacityParameter = TEXT("Opacity");

	/** If true, also drive the attached optional point light intensity with the flow. */
	UPROPERTY(EditAnywhere, Category="LiquidLight")
	bool bDriveLightWithFlow = false;

	/** Peak intensity of the optional light when fully active. */
	UPROPERTY(EditAnywhere, Category="LiquidLight", meta=(ClampMin="0.0"))
	float LightPeakIntensity = 8000.0f;

	// -- components --------------------------------------------------------
	/** Flat mesh that carries the flowing-liquid material (set its static mesh in editor). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FluidSurface;

	/** Optional light attached to the surface; enabled only when used. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Components, meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPointLightComponent> OptionalLight;

private:
	// -- internal state ----------------------------------------------------
	bool bFlowActive = false;
	float CurrentFlow = 0.0f;
	float TargetFlow = 0.0f;
	bool bManualStrength = false;
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMID;

	void RefreshSurface();
};
