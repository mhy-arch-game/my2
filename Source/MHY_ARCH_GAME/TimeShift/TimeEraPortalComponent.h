// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimeShiftTypes.h"
#include "TimeEraPortalComponent.generated.h"

class AActor;
class UInteractableComponent;
class UTimeEraComponent;
class UTimeShiftSubsystem;
class UTimeEraPortalComponent;

/** Fired after the traveller was successfully moved to the counterpart in the other era. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTimeEraPortalUsed, AActor*, Traveler, AActor*, Source, AActor*, Counterpart);

/** Fired when the portal refused to teleport, so UI/logic can explain why. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTimeEraPortalRefused, UTimeEraPortalComponent*, Portal, FText, Reason);

/**
 *  UTimeEraPortalComponent - era-linked teleport, driven by the SHARED interact interface.
 *
 *  指认一个"对应物"，交互时把主控角色送到对立时空里那个对应物的位置。
 *
 *  Wiring (no new interface, no new actor class):
 *    - Attach this component to the object the player interacts with.
 *    - It binds itself to the owner's UInteractableComponent (and creates one when
 *      missing), so the existing UInteractionDetectorComponent flow - focus, prompt,
 *      interact key - drives it with zero extra plumbing.
 *    - Designate the counterpart either directly (CounterpartActor) or by a shared
 *      CounterpartId that is resolved through the era-anchor registry of
 *      UTimeShiftSubsystem (the counterpart lives in the OPPOSITE era).
 *
 *  Interaction result:
 *    1. the era is switched to the counterpart's era (unless bSwitchEra is off), and
 *    2. the traveller is then explicitly placed at the counterpart, overriding the
 *       generic layout mapping of UTimeShiftTravelComponent (we run after SetEra).
 *
 *  Refusal cases (no teleport, OnPortalRefused fires): no counterpart, counterpart
 *  not in the opposite era, era switch refused (switch in flight / timed lock),
 *  or this portal still locked.
 */
/**
 *  成对方式：怎么找到"另一半"。
 */
UENUM(BlueprintType)
enum class ETimeEraPortalTargetMode : uint8
{
	/**
	 * 两个时空里各放一个对象，互相指认：显式引用 → CounterpartId 锚点 → 最近的锚点。
	 */
	CounterpartObject	UMETA(DisplayName="Counterpart Object (指定对应物)"),

	/**
	 * 同一个对象在两个时空里**只差 Z 坐标**（例如同一块踏板，古代在地面、现代在半空）：
	 * 落点 = 自己位置 + (0,0,±VerticalOffset)，**不需要任何对应物引用**。
	 * 符号按自己所属时空自动取，所以两半填**同一个** VerticalOffset 即可。
	 */
	VerticalOffset		UMETA(DisplayName="Vertical Offset (仅 Z 不同)")
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class UTimeEraPortalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTimeEraPortalComponent();

	// -- 成对方式 (how the other half is found) ----------------------------
	/** 成对方式：指定对应物，或者"两个时空仅 Z 不同"。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	ETimeEraPortalTargetMode TargetMode = ETimeEraPortalTargetMode::CounterpartObject;

	/**
	 * TargetMode = Vertical Offset 时的 Z 位移，定义为 **今 − 古**
	 * （现代那一半比古代高多少；现代更低就填负数）。
	 *
	 * 组件按自己所属时空自动取符号：
	 *   本对象在"古" → 落点 = 自己位置 + (0,0,+VerticalOffset)
	 *   本对象在"今" → 落点 = 自己位置 + (0,0,-VerticalOffset)
	 * 所以**两半填同一个值**，不会出现一半填正一半填反的错误。
	 * 这也是"两时空仅 Z 不同"能自动配对的原因：不需要任何引用。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal", meta=(EditCondition="TargetMode==ETimeEraPortalTargetMode::VerticalOffset"))
	float VerticalOffset = 1000.0f;

	// -- 指定对应物 (the designated counterpart) ---------------------------
	/**
	 * The corresponding object in the OTHER era. Highest priority: when set, it is
	 * used directly and CounterpartId is ignored.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	TObjectPtr<AActor> CounterpartActor;

	/**
	 * Shared id of the two corresponding objects (like ATimeShiftAnchor::AnchorId).
	 * The owner is registered with UTimeShiftSubsystem as an era anchor under this id,
	 * and the counterpart is looked up as the anchor of the opposite era. Use this
	 * when a direct reference cannot be authored (e.g. the objects are in different
	 * sublevels or spawned at runtime).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	FName CounterpartId = NAME_None;

	/** Register the owner as an era anchor under CounterpartId so the generic era switch also pairs it up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal", meta=(EditCondition="!CounterpartId.IsNone()"))
	bool bRegisterAsAnchor = true;

	// -- 所属时空（配对的正负号就是从这里来的）-------------------------------
	/**
	 * 自动读取 owner 上 UTimeEraComponent 的 Era。
	 *
	 *   打开（默认）：对象上有 TimeEraComponent 时以它的 Era 为准；
	 *                 **没有**时回退到下面的 OwnerEra（BeginPlay 会打 Warning 提醒）。
	 *   关闭：无论有没有 TimeEraComponent，都以 OwnerEra 为准。
	 *
	 * 不确定实际用的是哪个？看 BeginPlay 打的那行：
	 *   [TimeEraPortal] <对象>: 所属时空 = …（来源：TimeEraComponent / OwnerEra）
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	bool bAutoDetectEra = true;

	/**
	 * 本对象属于哪个时空。**始终可编辑**（不再因为 bAutoDetectEra 被灰掉）。
	 *
	 * 它决定两件事：
	 *   1. "另一半"在哪个时空（取反）；
	 *   2. Vertical Offset 模式的 Z 位移符号（古 = +VerticalOffset，今 = -VerticalOffset）。
	 * 所以这一项配错，配对方向和落点会一起错。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	ETimeEra OwnerEra = ETimeEra::Ancient;

	// -- 行为 ---------------------------------------------------------------
	/** Also flip the active era to the counterpart's era when the portal is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	bool bSwitchEra = true;

	/** Refuse the interaction unless the counterpart really belongs to the opposite era. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal")
	bool bRequireCounterpartInOtherEra = true;

	// -- 落点 (arrival placement) -------------------------------------------
	/** Offset applied to the counterpart's location, in the counterpart's local space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Placement")
	FVector TeleportOffset = FVector::ZeroVector;

	/** Face the same way as the counterpart after arriving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Placement")
	bool bMatchCounterpartYaw = true;

	/** Line-trace down from the counterpart so the traveller lands on the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Placement")
	bool bPlaceOnGround = true;

	/** Half-length of the ground trace around the counterpart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Placement", meta=(EditCondition="bPlaceOnGround", ClampMin="0.0"))
	float GroundTraceDistance = 1000.0f;

	/** Extra gap kept between the traveller's feet and the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Placement", meta=(EditCondition="bPlaceOnGround", ClampMin="0.0"))
	float GroundClearance = 2.0f;

	// -- 交互接线 (interaction wiring) --------------------------------------
	/**
	 * Bind to the owner's UInteractableComponent, creating one when the owner has
	 * none. This is what makes the portal reachable through the shared interact
	 * interface (focus / prompt / interact key) without any Blueprint graph.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Interaction")
	bool bAutoUseInteractableOnOwner = true;

	/** Prompt pushed onto the interactable (left untouched when empty). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Interaction")
	FText InteractionPrompt;

	/**
	 * Turn the interactable's built-in open/close toggle OFF, so this object ONLY
	 * teleports. Leave OFF to keep both behaviours (the object opens AND teleports).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Interaction")
	bool bSuppressBuiltInToggle = false;

	/** Minimum seconds between two uses of this portal (0 disables the lock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Interaction", meta=(ClampMin="0.0"))
	float PortalCooldown = 0.5f;

	/** Grey out the interactable (no prompt) while the portal is locked by PortalCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="TimeShift|Portal|Interaction")
	bool bDisableInteractableWhileLocked = true;

	// -- API ----------------------------------------------------------------
	/** Teleport Traveler to the counterpart. Returns false when the portal refused. */
	UFUNCTION(BlueprintCallable, Category="TimeShift|Portal")
	bool TryUsePortal(AActor* Traveler);

	/**
	 * The counterpart actually resolved for this portal.
	 * VerticalOffset 模式下**恒为 null** —— 那种模式没有"对应对象"，落点由自己位置 + Z 位移推导。
	 */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	AActor* ResolveCounterpart() const;

	/** Era the owner lives in. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	ETimeEra GetOwnerEra() const;

	/** Era the counterpart is expected in (the opposite of the owner's era). */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	ETimeEra GetCounterpartEra() const;

	/** Whether TryUsePortal would succeed right now. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	bool CanUsePortal(AActor* Traveler) const;

	/** Whether the per-portal lock is still running. */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	bool IsLocked() const;

	/**
	 * 一行人类可读状态：配对方式 / 本对象时空与来源 / 目标时空 / Z 偏移 /
	 * 解析出的落点 / 对应物 / 是否冷却中。给蓝图 Print String 排查用。
	 * 注意：它会真的跑一次落点解析（对应物模式下可能扫场景），别每帧调。
	 */
	UFUNCTION(BlueprintPure, Category="TimeShift|Portal")
	FString GetPortalDebugString() const;

	/** Fired after a successful teleport. */
	UPROPERTY(BlueprintAssignable, Category="TimeShift|Portal")
	FOnTimeEraPortalUsed OnPortalUsed;

	/** Fired when a use was refused, with the human-readable reason. */
	UPROPERTY(BlueprintAssignable, Category="TimeShift|Portal")
	FOnTimeEraPortalRefused OnPortalRefused;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Bound to UInteractableComponent::OnInteractRequested. */
	UFUNCTION()
	void HandleInteractRequested(AActor* Interactor, AActor* Interactable);

private:
	/** The interactable this portal is wired to (owned by the same actor). */
	UPROPERTY(Transient)
	TObjectPtr<UInteractableComponent> BoundInteractable;

	/** The owner's era component, when it has one. */
	UPROPERTY(Transient)
	TObjectPtr<UTimeEraComponent> EraComponent;

	/** Whether we registered the owner as an era anchor (and under which id). */
	bool bAnchorRegistered = false;
	FName RegisteredAnchorId = NAME_None;

	/** Whether we had to switch the interactable off for the lock. */
	bool bInteractableDisabledByUs = false;

	/** FPlatformTime seconds at which the per-portal lock expires. */
	double LockEndTime = 0.0;

	/** Timer that re-enables the interactable when the lock ends. */
	FTimerHandle LockTimerHandle;

	/** Resolve the counterpart: explicit reference first, then id, then nearest anchor. */
	AActor* ResolveCounterpartInternal(FText& OutRefusalReason) const;

	/**
	 * Resolve WHERE the traveller ends up, independent of HOW the pair was found.
	 * 两种成对方式都走这里，所以 TryUsePortal / CanUsePortal 不需要知道落点是
	 * 来自另一个 actor 还是来自 Z 位移。OutCounterpart 在 VerticalOffset 模式下为 null。
	 */
	bool ResolveDestination(FVector& OutLocation, FRotator& OutRotation, ETimeEra& OutEra,
		AActor*& OutCounterpart, FText& OutRefusalReason) const;

	/** Which era an arbitrary actor belongs to (portal, era component, else unknown). */
	bool TryGetActorEra(const AActor* Actor, ETimeEra& OutEra) const;

	/** Final arrival location: BaseLocation/BaseRotation is the already-resolved destination. */
	FVector ComputeArrivalLocation(const FVector& BaseLocation, const FRotator& BaseRotation,
		const AActor* Traveler) const;

	/** Capsule / bounds half height, used to sit the traveller on the floor. */
	float GetTravelerHalfHeight(const AActor* Traveler) const;

	/** The pawn to move: pawns are used as-is, controllers are followed to their pawn. */
	AActor* ResolveTraveler(AActor* Interactor) const;

	/** Arm the per-portal lock (and grey out the interactable while it runs). */
	void ArmLock();

	/** Lock elapsed: re-enable the interactable if we disabled it. */
	void HandleLockElapsed();
};

static ETimeEra GetOppositeEra(ETimeEra Era)
{
	return Era == ETimeEra::Ancient ? ETimeEra::Modern : ETimeEra::Ancient;
}
