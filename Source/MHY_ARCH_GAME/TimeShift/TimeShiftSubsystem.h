// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimeShiftTypes.h"
#include "TimeShiftSubsystem.generated.h"

class AActor;

/** Broadcast after the active era changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeEraChanged, ETimeEra, NewEra);

/** Broadcast when the switch cooldown (timed lock) starts or ends. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeShiftCooldownEvent);

/**
 *  UTimeShiftSubsystem - authoritative time-era state for the whole session.
 *
 *  CARRIER: single map, two era layouts.
 *  Both the Ancient and the Modern buildings are loaded in the SAME map (placed at
 *  different world positions). Pressing the switch key does NOT load a level; it
 *    1. flips the era and broadcasts OnEraChanged,
 *    2. every UTimeEraComponent gates its actor (visibility / collision / tick),
 *    3. UTimeShiftTravelComponent moves the player (and carried actors) by the offset
 *       between the two anchors that share the same AnchorId.
 *
 *  Anchors let "the same spot" exist in both eras:
 *      ATimeShiftAnchor(AnchorId="Gate", Era=Ancient)  <->  ATimeShiftAnchor(AnchorId="Gate", Era=Modern)
 *  When no matching anchor exists in the target era, the traveller simply stays put.
 *
 *  Timed lock (cooldown):
 *   - CooldownDuration is config-driven (Config/DefaultGame.ini).
 *   - While cooling down CanSwitchEra() is false and SetEra()/SwitchEra() are refused.
 *   - The countdown runs on the CORE ticker (no world dependency).
 */
UCLASS(config=Game)
class UTimeShiftSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Convenience accessor from any world-context object (pawn, component, actor). */
	UFUNCTION(BlueprintPure, Category="TimeShift", meta=(WorldContext="WorldContextObject"))
	static UTimeShiftSubsystem* Get(const UObject* WorldContextObject);

	/** Flip to the other era (this is what the switch key calls). */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	void SwitchEra();

	/** Move to a specific era. Refused while switching or while the cooldown is active. */
	UFUNCTION(BlueprintCallable, Category="TimeShift")
	void SetEra(ETimeEra NewEra);

	/** The era currently considered active. */
	UFUNCTION(BlueprintPure, Category="TimeShift")
	ETimeEra GetEra() const { return CurrentEra; }

	/** The era that is not active. */
	UFUNCTION(BlueprintPure, Category="TimeShift")
	ETimeEra GetOtherEra() const;

	/** True only while a switch is being processed (re-entrancy guard). */
	UFUNCTION(BlueprintPure, Category="TimeShift")
	bool IsSwitching() const { return bSwitching; }

	/** Fired after the active era changed; listeners gate content and relocate actors. */
	UPROPERTY(BlueprintAssignable, Category="TimeShift")
	FOnTimeEraChanged OnEraChanged;

	// -- era anchors -------------------------------------------------------
	/** Register an anchor actor for an era (normally called by ATimeShiftAnchor). */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Anchor")
	void RegisterAnchor(AActor* Anchor, FName AnchorId, ETimeEra Era);

	/** Unregister an anchor actor. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Anchor")
	void UnregisterAnchor(AActor* Anchor);

	/** The anchor registered for AnchorId in the given era (may be null). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Anchor")
	AActor* FindAnchor(FName AnchorId, ETimeEra Era) const;

	/** The registered anchor of the given era closest to Location (may be null). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Anchor")
	AActor* FindNearestAnchor(ETimeEra Era, const FVector& Location, float MaxDistance = 0.0f) const;

	/** The AnchorId an anchor actor was registered under (NAME_None if unknown). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Anchor")
	FName GetAnchorId(AActor* Anchor) const;

	// -- era layout references ---------------------------------------------
	/**
	 * Register the actor that defines an era's whole layout reference transform.
	 * With both eras registered, positions map 1:1 between the two layouts
	 * (the "layouts correspond" case) - see ETimeShiftMappingMode::LayoutOrigin.
	 */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Anchor")
	void RegisterLayoutOrigin(AActor* LayoutRoot, ETimeEra Era);

	/** Unregister a layout reference actor. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Anchor")
	void UnregisterLayoutOrigin(AActor* LayoutRoot);

	/** The actor defining the given era's layout reference (may be null). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Anchor")
	AActor* GetLayoutOrigin(ETimeEra Era) const;

	/** Layout reference transform of an era. Returns false when none is registered. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Anchor")
	bool GetLayoutTransform(ETimeEra Era, FTransform& OutTransform) const;

	// -- timed lock (cooldown) ---------------------------------------------
	/** Whether a switch is allowed right now (not switching, not on cooldown). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Cooldown")
	bool CanSwitchEra() const;

	/** Whether the switch cooldown is currently active. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Cooldown")
	bool IsOnCooldown() const;

	/** Seconds left on the cooldown (0 when not cooling down). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Cooldown")
	float GetCooldownRemaining() const;

	/** Cooldown progress: 1 = just started, 0 = finished / inactive (for UI fills). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Cooldown")
	float GetCooldownRatio() const;

	/** Configured cooldown length in seconds. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Cooldown")
	float GetCooldownDuration() const { return CooldownDuration; }

	/** Start (or restart) the timed lock. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Cooldown")
	void StartCooldown();

	/** Cancel the timed lock immediately. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Cooldown")
	void ClearCooldown();

	/** Change the cooldown length at runtime (does not affect an active countdown). */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Cooldown")
	void SetCooldownDuration(float NewDuration);

	/** Fired when the timed lock starts. */
	UPROPERTY(BlueprintAssignable, Category="TimeShift|Cooldown")
	FOnTimeShiftCooldownEvent OnCooldownStarted;

	/** Fired when the timed lock ends (expired or cleared) - UI can re-enable the prompt. */
	UPROPERTY(BlueprintAssignable, Category="TimeShift|Cooldown")
	FOnTimeShiftCooldownEvent OnCooldownFinished;

	// -- RESERVED: cross-era linkage ---------------------------------------
	/** RESERVED: register an object implementing ITimeLinkableInterface. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Link")
	void RegisterLinkable(AActor* Linkable);

	/** RESERVED: unregister a previously registered linkable object. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Link")
	void UnregisterLinkable(AActor* Linkable);

	/** RESERVED: publish a state to every registered object sharing LinkId. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Link")
	void BroadcastLinkState(FName LinkId, const FTimeLinkState& State);

	/** RESERVED: all registered linkable actors sharing LinkId. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Link")
	TArray<AActor*> GetLinkablesById(FName LinkId) const;

protected:
	/** Era the session starts in. */
	UPROPERTY(Config, EditAnywhere, Category="TimeShift")
	ETimeEra InitialEra = ETimeEra::Ancient;

	/** Timed lock applied after each successful switch, in seconds (0 disables the lock). */
	UPROPERTY(Config, EditAnywhere, Category="TimeShift|Cooldown", meta=(ClampMin="0.0"))
	float CooldownDuration = 2.0f;

private:
	ETimeEra CurrentEra = ETimeEra::Ancient;
	bool bSwitching = false;

	/** Absolute time (FPlatformTime seconds) at which the cooldown expires. */
	double CooldownEndTime = 0.0;

	/** Core-ticker handle used to fire OnCooldownFinished when the lock expires. */
	FTSTicker::FDelegateHandle CooldownHandle;

	/** True once a cooldown has been armed but not yet finished/cleared. */
	bool bCooldownArmed = false;

	/** Anchors by id, one per era (weak refs: level teardown cleans up automatically). */
	UPROPERTY(Transient)
	TMap<FName, FTimeShiftAnchorPair> Anchors;

	/** Actor defining each era's whole-layout reference transform. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> AncientLayoutOrigin;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ModernLayoutOrigin;

	/** RESERVED: registered cross-era linkable actors. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> Linkables;

	/** Core-ticker callback that ends the cooldown; returns false to run once. */
	bool HandleCooldownElapsed(float DeltaTime);
};
