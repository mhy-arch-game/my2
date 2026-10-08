#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionComponent.generated.h"

class UInteractableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionFocusChanged, AActor*, NewFocus, AActor*, PreviousFocus);

/**
 * Interactor side. Put this on the player character (no reparenting needed),
 * then call TryInteract() from an Enhanced Input action.
 *
 * Every TraceInterval seconds it traces from the view point, resolves the
 * focused actor and broadcasts OnFocusChanged when that actor changes.
 */
UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class MHY_PROJ_V0_1_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	/** Interact with the currently focused actor. Bind this to IA_Interact. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool TryInteract();

	/** Force an immediate focus refresh and return the new focus. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* RefreshFocus();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool HasFocus() const { return FocusedActor.IsValid(); }

	/** Prompt of the focused actor, empty when nothing is focused. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetFocusedPrompt();

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionFocusChanged OnFocusChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Seconds between two focus traces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.02", UIMin = "0.02"))
	float TraceInterval = 0.1f;

	/** Trace length in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.0"))
	float TraceDistance = 300.f;

	/** Collision channel used for the focus trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bDrawDebug = false;

	/** When true only interface implementers or UInteractableComponent owners count as targets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bRequireInteractable = true;

private:
	void UpdateFocus();
	bool TraceFromViewpoint(FHitResult& OutHit) const;

	TWeakObjectPtr<AActor> FocusedActor;
	FTimerHandle TraceTimerHandle;
};
