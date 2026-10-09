// Copyright Epic Games, Inc. All Rights Reserved.

#include "OverlapPassageComponent.h"

#include "Components/BoxComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

UOverlapPassageComponent::UOverlapPassageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Typical scene object types the "other" body may use.
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery1); // WorldStatic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery2); // WorldDynamic
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery3); // Pawn
	ProbeObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery4); // PhysicsBody
}

void UOverlapPassageComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	HostPrimitive = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;

	if (!HostPrimitive)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OverlapPassage] %s has no primitive root component; proxy grid cannot be built."),
			*GetNameSafe(Owner));
		return;
	}

	// Remember the host's original collision so we can restore it on EndPlay.
	SavedHostProfileName = HostPrimitive->GetCollisionProfileName();
	SavedHostCollisionEnabled = HostPrimitive->GetCollisionEnabled();

	if (bDisableHostCollision)
	{
		SetHostCollisionEnabled(false);
	}

	// Cache the meshes whose appearance we may change while overlapping.
	ResolveTargetMeshes();
	CaptureMeshStates();

	BuildProxyGrid();
	RefreshPassage();
}

void UOverlapPassageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreAppearance();
	DestroyProxyGrid();

	if (HostPrimitive && bDisableHostCollision)
	{
		SetHostCollisionEnabled(true);
	}

	Super::EndPlay(EndPlayReason);
}

void UOverlapPassageComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ProxyBoxes.Num() == 0)
	{
		return;
	}

	TimeSinceRefresh += DeltaTime;
	if (UpdateInterval <= 0.0f || TimeSinceRefresh >= UpdateInterval)
	{
		TimeSinceRefresh = 0.0f;
		RefreshPassage();
	}
}

void UOverlapPassageComponent::RefreshPassage()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	bool bAnyPassable = false;

	if (!OtherActor)
	{
		// No counterpart assigned -> the host stays fully solid.
		SetAllCellsSolid();
	}
	else
	{
		bool bSkip = false;

		// Cheap AABB pre-check: if the two bodies are nowhere near each other, nothing to carve.
		if (bOnlyWhenAABBOverlap)
		{
			const FBox HostBox = Owner->GetComponentsBoundingBox(true);
			const FBox OtherBox = OtherActor->GetComponentsBoundingBox(true);
			bSkip = !HostBox.Intersect(OtherBox);
		}

		if (bSkip)
		{
			SetAllCellsSolid();
		}
		else
		{
			// Carve exactly the cells occupied by the other object.
			for (TObjectPtr<UBoxComponent>& Cell : ProxyBoxes)
			{
				if (!Cell)
				{
					continue;
				}

				const bool bPassable = CellOverlapsOther(Cell);
				if (bPassable)
				{
					bAnyPassable = true;
				}
				Cell->SetCollisionEnabled(bPassable
					? ECollisionEnabled::NoCollision
					: ECollisionEnabled::QueryAndPhysics);
			}
		}
	}

	// Visual state follows the overlap state (alternate material / transparency).
	bOverlapping = bAnyPassable;
	if (bOverlapping)
	{
		ApplyOverlapAppearance();
	}
	else
	{
		RestoreAppearance();
	}
}

void UOverlapPassageComponent::RebuildProxyGrid()
{
	BuildProxyGrid();
	RefreshPassage();
}

void UOverlapPassageComponent::BuildProxyGrid()
{
	DestroyProxyGrid();

	AActor* Owner = GetOwner();
	if (!Owner || !HostPrimitive)
	{
		return;
	}

	// Grid lives in the host primitive's local space so it follows the object when it moves.
	const FBox LocalBox = HostPrimitive->GetLocalBounds().GetBox();
	const FVector Size = LocalBox.GetSize();
	if (Size.GetMin() <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OverlapPassage] %s host bounds are degenerate; proxy grid skipped."),
			*GetNameSafe(Owner));
		return;
	}

	const int32 DivX = FMath::Max(1, Divisions.X);
	const int32 DivY = FMath::Max(1, Divisions.Y);
	const int32 DivZ = FMath::Max(1, Divisions.Z);

	// Safety cap so a bad Divisions value cannot spawn thousands of colliders.
	constexpr int32 MaxCells = 4096;
	if (DivX * DivY * DivZ > MaxCells)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[OverlapPassage] %s Divisions (%d,%d,%d) exceeds %d cells; clamping."),
			*GetNameSafe(Owner), DivX, DivY, DivZ, MaxCells);
	}

	const FVector CellSize(
		Size.X / static_cast<float>(DivX),
		Size.Y / static_cast<float>(DivY),
		Size.Z / static_cast<float>(DivZ));
	const FVector HalfExtent = CellSize * 0.5f;

	int32 Created = 0;
	for (int32 Z = 0; Z < DivZ && Created < MaxCells; ++Z)
	{
		for (int32 Y = 0; Y < DivY && Created < MaxCells; ++Y)
		{
			for (int32 X = 0; X < DivX && Created < MaxCells; ++X)
			{
				const FVector LocalCenter = LocalBox.Min + FVector(
					(X + 0.5f) * CellSize.X,
					(Y + 0.5f) * CellSize.Y,
					(Z + 0.5f) * CellSize.Z);

				UBoxComponent* Cell = NewObject<UBoxComponent>(Owner);
				Cell->SetBoxExtent(HalfExtent);
				Cell->SetMobility(HostPrimitive->Mobility);
				Cell->SetCollisionProfileName(TEXT("BlockAllDynamic"));
				Cell->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Cell->SetGenerateOverlapEvents(false);
				Cell->SetHiddenInGame(true);
				Cell->RegisterComponent();
				Cell->AttachToComponent(HostPrimitive, FAttachmentTransformRules::KeepRelativeTransform);
				Cell->SetRelativeLocation(LocalCenter);

				ProxyBoxes.Add(Cell);
				++Created;
			}
		}
	}
}

void UOverlapPassageComponent::DestroyProxyGrid()
{
	for (TObjectPtr<UBoxComponent>& Cell : ProxyBoxes)
	{
		if (Cell)
		{
			Cell->DestroyComponent();
		}
	}
	ProxyBoxes.Reset();
}

void UOverlapPassageComponent::SetHostCollisionEnabled(bool bEnabled)
{
	if (!HostPrimitive)
	{
		return;
	}

	if (bEnabled)
	{
		if (SavedHostProfileName != NAME_None)
		{
			HostPrimitive->SetCollisionProfileName(SavedHostProfileName);
		}
		HostPrimitive->SetCollisionEnabled(SavedHostCollisionEnabled);
	}
	else
	{
		HostPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UOverlapPassageComponent::SetAllCellsSolid()
{
	for (TObjectPtr<UBoxComponent>& Cell : ProxyBoxes)
	{
		if (Cell && Cell->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
		{
			Cell->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

bool UOverlapPassageComponent::CellOverlapsOther(const UBoxComponent* CellBox) const
{
	if (!CellBox || !OtherActor)
	{
		return false;
	}

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(GetOwner());

	TArray<AActor*> Overlaps;
	UKismetSystemLibrary::BoxOverlapActors(
		this,
		CellBox->GetComponentLocation(),
		CellBox->GetScaledBoxExtent(),
		ProbeObjectTypes,
		OtherActor->GetClass(),
		IgnoredActors,
		Overlaps);

	return Overlaps.Contains(OtherActor);
}

void UOverlapPassageComponent::ResolveTargetMeshes()
{
	MeshStates.Reset();

	// Explicit list wins; otherwise use every mesh component on the host.
	if (TargetMeshes.Num() > 0)
	{
		for (const TObjectPtr<UMeshComponent>& MeshPtr : TargetMeshes)
		{
			if (UMeshComponent* Mesh = MeshPtr)
			{
				FOverlapPassageMeshState State;
				State.Mesh = Mesh;
				MeshStates.Add(State);
			}
		}
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		TArray<UMeshComponent*> Meshes;
		Owner->GetComponents<UMeshComponent>(Meshes);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh)
			{
				FOverlapPassageMeshState State;
				State.Mesh = Mesh;
				MeshStates.Add(State);
			}
		}
	}
}

void UOverlapPassageComponent::CaptureMeshStates()
{
	for (FOverlapPassageMeshState& State : MeshStates)
	{
		State.OriginalMaterials.Reset();
		State.OverlapMIDs.Reset();

		UMeshComponent* Mesh = State.Mesh;
		if (!Mesh)
		{
			continue;
		}

		const int32 NumMats = Mesh->GetNumMaterials();
		for (int32 Index = 0; Index < NumMats; ++Index)
		{
			State.OriginalMaterials.Add(Mesh->GetMaterial(Index));
		}
	}
}

void UOverlapPassageComponent::ApplyOverlapAppearance()
{
	if (!bChangeAppearanceOnOverlap || bAppearanceApplied)
	{
		return;
	}

	for (FOverlapPassageMeshState& State : MeshStates)
	{
		UMeshComponent* Mesh = State.Mesh;
		if (!Mesh)
		{
			continue;
		}

		const int32 NumMats = Mesh->GetNumMaterials();

		if (bDriveOpacityParameter)
		{
			// Build a dynamic instance per element and drive its opacity parameter.
			State.OverlapMIDs.Reset();
			for (int32 Index = 0; Index < NumMats; ++Index)
			{
				// Note: both branches must be the same type, otherwise the ternary is
				// ambiguous between TObjectPtr<UMaterialInterface> and UMaterialInterface*.
				UMaterialInterface* Base = State.OriginalMaterials.IsValidIndex(Index)
					? State.OriginalMaterials[Index].Get()
					: Mesh->GetMaterial(Index);

				UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(Index, Base);
				if (MID)
				{
					MID->SetScalarParameterValue(OpacityParameterName, OverlapOpacity);
				}
				State.OverlapMIDs.Add(MID);
			}
		}
		else if (OverlapMaterial)
		{
			// Simple whole-mesh material swap.
			for (int32 Index = 0; Index < NumMats; ++Index)
			{
				Mesh->SetMaterial(Index, OverlapMaterial);
			}
		}
	}

	bAppearanceApplied = true;
}

void UOverlapPassageComponent::RestoreAppearance()
{
	if (!bAppearanceApplied)
	{
		return;
	}

	for (FOverlapPassageMeshState& State : MeshStates)
	{
		UMeshComponent* Mesh = State.Mesh;
		if (!Mesh)
		{
			continue;
		}

		for (int32 Index = 0; Index < State.OriginalMaterials.Num(); ++Index)
		{
			Mesh->SetMaterial(Index, State.OriginalMaterials[Index]);
		}
		State.OverlapMIDs.Reset();
	}

	bAppearanceApplied = false;
}
