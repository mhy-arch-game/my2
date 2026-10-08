// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "OverlapPassageComponent.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Saved visual state of one host mesh, used to restore it when the overlap ends. */
USTRUCT()
struct FOverlapPassageMeshState
{
	GENERATED_BODY()

	/** The host mesh this state belongs to. */
	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> Mesh = nullptr;

	/** Materials the mesh had before the overlap appearance was applied. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	/** Dynamic instances created for the opacity mode (empty in material-swap mode). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> OverlapMIDs;
};

/**
 *  UOverlapPassageComponent - "where two objects overlap, that part becomes passable".
 *
 *  Route B (approved): subdivided proxy-collider approximation.
 *
 *  Attach this component to one object (the HOST). It takes over the host's collision
 *  with a grid of invisible proxy box colliders covering the host's local bounds.
 *  Every refresh it tests which grid cells are occupied by the OTHER object; those
 *  cells get their collision DISABLED, so a character can pass through exactly the
 *  overlapping region. Cells that are NOT overlapped stay solid.
 *
 *  On top of that, while the host overlaps the other object it can visually switch to
 *  an alternate material or become transparent (typically used on the movable body):
 *   - OverlapMaterial : swap the host's materials to a different (e.g. translucent) one.
 *   - bDriveOpacityParameter : drive an Opacity scalar on a dynamic instance instead.
 *
 *  Typical setup: place one component on each of the two objects, each pointing its
 *  OtherActor at the other, so the intersection is passable for both bodies.
 *
 *  Limitations (by design, route B):
 *   - Approximation: passable region is quantised to the grid cells (see Divisions).
 *   - Cell probes are axis-aligned boxes (rotation of the host is not accounted for
 *     in the per-cell test).
 *   - The host's original collision is disabled while this component is active
 *     (bDisableHostCollision); a non-colliding host cannot be carved in place.
 *   - The appearance change is applied per MESH (whole object), not per carved cell.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UOverlapPassageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOverlapPassageComponent();

	/** Recompute the carved (passable) cells immediately. */
	UFUNCTION(BlueprintCallable, Category="OverlapPassage")
	void RefreshPassage();

	/** Rebuild the proxy grid (call after changing the host's size / Divisions at runtime). */
	UFUNCTION(BlueprintCallable, Category="OverlapPassage")
	void RebuildProxyGrid();

	/** True while the host is currently overlapping the other object. */
	UFUNCTION(BlueprintPure, Category="OverlapPassage")
	bool IsOverlapping() const { return bOverlapping; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// -- configuration -----------------------------------------------------
	/** The other object we carve the intersection against. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage")
	TObjectPtr<AActor> OtherActor;

	/** Proxy grid resolution (more cells = finer passage, more colliders). */
	UPROPERTY(EditAnywhere, Category="OverlapPassage")
	FIntVector Divisions = FIntVector(4, 4, 4);

	/** Seconds between refreshes (throttles the per-cell overlap queries). */
	UPROPERTY(EditAnywhere, Category="OverlapPassage", meta=(ClampMin="0.0"))
	float UpdateInterval = 0.05f;

	/** Disable the host's own collision so the proxy grid fully governs it. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage")
	bool bDisableHostCollision = true;

	/** Skip the work while the two objects' bounds do not intersect at all. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage")
	bool bOnlyWhenAABBOverlap = true;

	/** Object types considered when probing whether a cell is occupied by the other object. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage")
	TArray<TEnumAsByte<EObjectTypeQuery>> ProbeObjectTypes;

	// -- appearance on overlap ---------------------------------------------
	/** While overlapping, change the host's look (alternate material and/or transparency). */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance")
	bool bChangeAppearanceOnOverlap = false;

	/** Material applied to the host while overlapping (e.g. a translucent one). */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance",
		meta=(EditCondition="bChangeAppearanceOnOverlap", EditConditionHides))
	TObjectPtr<UMaterialInterface> OverlapMaterial;

	/** Instead of swapping materials, drive an Opacity scalar on a dynamic instance. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance",
		meta=(EditCondition="bChangeAppearanceOnOverlap", EditConditionHides))
	bool bDriveOpacityParameter = false;

	/** Opacity value written to OpacityParameterName while overlapping. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance",
		meta=(EditCondition="bChangeAppearanceOnOverlap && bDriveOpacityParameter", ClampMin="0.0", ClampMax="1.0"))
	float OverlapOpacity = 0.35f;

	/** Scalar parameter name used for the transparency mode. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance",
		meta=(EditCondition="bChangeAppearanceOnOverlap && bDriveOpacityParameter"))
	FName OpacityParameterName = TEXT("Opacity");

	/** Explicit meshes to change; if empty, all UMeshComponents on the host are used. */
	UPROPERTY(EditAnywhere, Category="OverlapPassage|Appearance",
		meta=(EditCondition="bChangeAppearanceOnOverlap", EditConditionHides))
	TArray<TObjectPtr<UMeshComponent>> TargetMeshes;

private:
	/** Runtime proxy colliders (one per grid cell). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> ProxyBoxes;

	/** The host primitive whose bounds define the grid (usually the root component). */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> HostPrimitive;

	/** Saved visual state per host mesh, for restoring after the overlap ends. */
	UPROPERTY(Transient)
	TArray<FOverlapPassageMeshState> MeshStates;

	/** Saved host collision state, restored on EndPlay. */
	FName SavedHostProfileName = NAME_None;
	ECollisionEnabled::Type SavedHostCollisionEnabled = ECollisionEnabled::NoCollision;

	float TimeSinceRefresh = 0.0f;
	bool bOverlapping = false;
	bool bAppearanceApplied = false;

	void BuildProxyGrid();
	void DestroyProxyGrid();
	void SetHostCollisionEnabled(bool bEnabled);
	void SetAllCellsSolid();
	bool CellOverlapsOther(const UBoxComponent* CellBox) const;

	void ResolveTargetMeshes();
	void CaptureMeshStates();
	void ApplyOverlapAppearance();
	void RestoreAppearance();
};
