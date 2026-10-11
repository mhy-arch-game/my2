// Copyright Epic Games, Inc. All Rights Reserved.

#include "EndOfTheGameActor.h"

#include "GameFlowLibrary.h"
#include "InteractableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AEndOfTheGameActor::AEndOfTheGameActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// ★ 这里故意不创建 Interactable / StaticMesh 组件：
	//   蓝图子类 end_of_the_game 自己有一个叫 Interactable 的组件，基类再建一个同名的，
	//   蓝图编译就会报 Tried to create a property Interactable in scope ... already exists。
	//   组件由蓝图 / 实例提供；运行时若发现缺了，EnsureInteractionSetup() 会补。
	FallbackInteractPrompt = FText::FromString(TEXT("触碰"));

	// 自动补方块用的网格：在构造函数里取一次形成硬引用（打包时才会收进包里）。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		FallbackBodyMesh = CubeMesh.Object;
	}

	// 永续自转：URotatingMovementComponent 在 tick 里按 RotationRate 一直转；
	// UMovementComponent 的 bAutoRegisterUpdatedComponent 会自动接管本 Actor 的根组件。
	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("EndGameSpin"));
	Spin->RotationRate = SpinRate;
	Spin->bRotationInLocalSpace = bSpinInLocalSpace;
	Spin->bAutoActivate = true;
}

void AEndOfTheGameActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureInteractionSetup();

	for (UInteractableComponent* Interactable : Interactables)
	{
		if (IsValid(Interactable))
		{
			Interactable->OnInteractRequested.AddDynamic(this, &AEndOfTheGameActor::HandleInteractRequested);
		}
	}

	ApplySpinSettings();
}

void AEndOfTheGameActor::EnsureInteractionSetup()
{
	Interactables.Reset();

	// 1) 先用蓝图 / 实例上已经有的可交互组件（理论上只有一个，多了也全绑上）。
	TArray<UInteractableComponent*> Found;
	GetComponents<UInteractableComponent>(Found);
	for (UInteractableComponent* Interactable : Found)
	{
		if (!IsValid(Interactable))
		{
			continue;
		}

		// 提示词为空才填默认值；蓝图里自己写了「触碰」就尊重蓝图。
		if (Interactable->GetInteractionPrompt().IsEmpty() && !FallbackInteractPrompt.IsEmpty())
		{
			Interactable->InteractionPrompt = FallbackInteractPrompt;
		}
		Interactables.Add(Interactable);
	}

	// 2) 一个都没有：补一个，否则这个物件完全没法交互。
	if (Interactables.Num() == 0)
	{
		if (UInteractableComponent* Created = NewObject<UInteractableComponent>(this, TEXT("EndGameInteractable")))
		{
			Created->InteractionPrompt = FallbackInteractPrompt;
			Created->RegisterComponent();
			Interactables.Add(Created);

			UE_LOG(LogTemp, Log, TEXT("[EndGame] %s 上没有可交互组件，已自动补一个（提示词：%s）。"),
				*GetActorNameOrLabel(), *FallbackInteractPrompt.ToString());
		}
	}

	// 3) 没有任何「开着碰撞的图元」时补一个方块：否则交互射线打不中这个物件。
	bool bHasCollidablePrimitive = false;
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (IsValid(Primitive) && Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			bHasCollidablePrimitive = true;
			break;
		}
	}

	if (!bHasCollidablePrimitive)
	{
		if (UStaticMeshComponent* Created = NewObject<UStaticMeshComponent>(this, TEXT("EndGameBody")))
		{
			Created->SetMobility(EComponentMobility::Movable);
			Created->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Created->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Created->SetRelativeScale3D(FVector(0.45f));
			if (FallbackBodyMesh)
			{
				Created->SetStaticMesh(FallbackBodyMesh);
			}

			if (USceneComponent* Root = GetRootComponent())
			{
				Created->SetupAttachment(Root);
				Created->RegisterComponent();
			}
			else
			{
				SetRootComponent(Created);
				Created->RegisterComponent();
			}

			UE_LOG(LogTemp, Log, TEXT("[EndGame] %s 上没有带碰撞的图元，已自动补一个方块。"), *GetActorNameOrLabel());
		}
	}
}

UInteractableComponent* AEndOfTheGameActor::GetInteractableComponent() const
{
	return Interactables.Num() > 0 ? Interactables[0].Get() : nullptr;
}

UStaticMeshComponent* AEndOfTheGameActor::GetBodyMesh() const
{
	return FindComponentByClass<UStaticMeshComponent>();
}

void AEndOfTheGameActor::ApplySpinSettings()
{
	if (!Spin)
	{
		return;
	}

	Spin->RotationRate = SpinRate;
	Spin->bRotationInLocalSpace = bSpinInLocalSpace;
	Spin->SetComponentTickEnabled(bAutoSpin);
	Spin->SetActive(bAutoSpin);
}

void AEndOfTheGameActor::SetSpinning(bool bEnabled)
{
	bAutoSpin = bEnabled;
	ApplySpinSettings();
}

void AEndOfTheGameActor::SetSpinRate(FRotator NewRate)
{
	SpinRate = NewRate;
	ApplySpinSettings();
}

bool AEndOfTheGameActor::IsSpinning() const
{
	return Spin && Spin->IsActive() && !Spin->RotationRate.IsNearlyZero();
}

void AEndOfTheGameActor::HandleInteractRequested(AActor* Interactor, AActor* InteractableActor)
{
	UE_LOG(LogTemp, Log, TEXT("[EndGame] 物件 %s 被 %s 交互，开始结束流程。"),
		*GetActorNameOrLabel(), Interactor ? *Interactor->GetName() : TEXT("?"));

	TriggerEndGame();
}

void AEndOfTheGameActor::TriggerEndGame()
{
	if (bTriggered)
	{
		return;
	}
	bTriggered = true;

	// 流程一旦开始就不再接受交互。
	for (UInteractableComponent* Interactable : Interactables)
	{
		if (IsValid(Interactable))
		{
			Interactable->SetEnabled(false);
		}
	}

	UGameFlowLibrary::EndGameWithFade(this, FadeOutSeconds, BlackHoldSeconds);
}