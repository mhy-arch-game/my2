# GravityZone —— 方形减重力区域

> 代码：`Source/MHY_ARCH_GAME/GravityZone/GravityZoneComponent.{h,cpp}`
> 类名：**`UGravityZoneComponent`**（`UBoxComponent` 子类）

---

## 1. 是什么

**组件本身就是那个方形区域**：把它挂到任意 Actor 上、把 Box 拉成你要的范围，
任何**胶囊与之重叠**的角色，其 `UCharacterMovementComponent::GravityScale` 会被乘以
`GravityScaleInside`。离开时**精确还原**该角色原本的重力系数。

- **不是**靠碰撞阻挡：纯 QueryOnly + `ECC_Pawn` Overlap，游戏内隐藏，**永远不挡路**
- **不假设**默认值：进入时缓存该角色**自己**的 `GravityScale`（不是硬编码写回 1.0）
- 角色在区域内**生成 / 关卡开始时就在区域内**也能生效（`BeginPlay` 会补扫一次）

---

## 2. 参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `GravityScaleInside` | 0.3 | 区域内的重力倍率。`1` = 不变，`0.3` = 飘，`0` = 完全失重 |
| `bAffectPawnsOnly` | ✔ | 只影响 Pawn（角色）。关掉则影响任何带 `CharacterMovementComponent` 的 Actor |
| `bScaleJumpVelocity` | ✗ | 同时把 `JumpZVelocity` 乘以 `sqrt(倍率)`，让跳跃高度基本不变但滞空更久（经典的月球重力手感）。默认关：跳跃会明显变高 |
| 盒体尺寸 | 400×400×200 | 在组件 Details 的 `Shape → Box Extent` 调，或视口里拖 |

**可读查询 / 事件**

| 接口 | 用途 |
|---|---|
| `IsActorInside(Actor)` | 某 Actor 当前是否在区域内 |
| `GetTrackedActorCount()` | 当前区域内有几个被影响的 Actor |
| `OnActorEntered(Actor, AppliedGravityScale)` | 进入时广播（可播特效 / 音效） |
| `OnActorExited(Actor, AppliedGravityScale)` | 离开时广播（携带还原后的值） |

---

## 3. 怎么搭（编辑器）

1. **Place Actors → 空 Actor**（或 `Basic → Actor`），拖进关卡
2. 选中它 → **+ Add → `Gravity Zone`**
3. 把该组件拖到 `DefaultSceneRoot` 上方替换它，让它成为 **Root**（摆放更方便）
4. 调 `Shape → Box Extent` 到你要的方正范围
5. 设 `Gravity Scale Inside`（例如 `0.3`）
6. 若希望跳跃高度不变、只是滞空更久，勾 `bScale Jump Velocity`
7. Play → 角色走进区域应明显变飘，走出应立刻恢复原手感

> 想直接挂到角色身上做自身周围的重力场也可以 —— 组件跟随 Actor，效果一样。

---

## 4. 已知限制

| 项 | 说明 |
|---|---|
| **多个重力区重叠不叠加** | 每个区各自缓存 / 还原。重叠时后进入的生效，退出还原为原始值。**请让区域互不重叠** |
| 只改角色重力 | 只作用于 `UCharacterMovementComponent`（角色）。物理刚体的 `bEnableGravity` / 质量不受影响 |
| 无渐变 | 进出是瞬时的。想要平滑过渡可在 `OnActorEntered` / `OnActorExited` 里自己插值 |
| 运行时改 `GravityScaleInside` | 不会自动应用到已在区域内的角色；需先离开再进入 |
| 角色在区域内被销毁 | 移动组件随之消失，无需还原；重生后重新进入会重新缓存 |
| 组件被销毁 | `EndPlay` 会把仍在区域内的角色逐个还原，不会让角色一直飘着 |

---

## 5. 相关文档

| 文档 | 内容 |
|---|---|
| `Docs/InteractionImplementation.md` | 交互实现（角色端 / 门端 / 灯端） |
| `Docs/EditorImplementationGuide.md` | 编辑器内总体搭建指南 |
---

## 6. ★ 踩坑记录：构造函数里必须用 `InitBoxExtent`

本组件第一次实现时在构造函数里写了 `SetBoxExtent(...)`，导致编辑器**直接 Fatal**：

```
Fatal error: NewObject with empty name can't be used to create default subobjects
  (inside of UObject derived class constructor)
UGravityZoneComponent::UGravityZoneComponent()  ... GravityZoneComponent.cpp:19
```

### 原因

`UBoxComponent::SetBoxExtent()` 内部会调用 `UpdateBodySetup()` → `CreateShapeBodySetupIfNeeded<>()`
→ 用**空名字** `NewObject<UBodySetup>(...)`。而「UObject 构造函数内用空名 NewObject」是引擎显式禁止的。

### 正确写法

```cpp
// 构造函数里 —— 安全（inline，只赋值 BoxExtent）
InitBoxExtent(FVector(400.0f, 400.0f, 200.0f));

// 构造函数里 —— 会 Fatal，禁止
SetBoxExtent(FVector(400.0f, 400.0f, 200.0f));
```

引擎自己也是这么做的（`PortalComponent` / `ReflectionCaptureComponent` / `SceneCaptureComponent` / `LevelBounds` 的构造函数都用 `InitBoxExtent`）。

### 适用规则

| 场景 | 用什么 |
|---|---|
| 构造函数内 | **`InitBoxExtent`**（`SetSphereRadius` / `SetCapsuleSize` 同理，构造期都要用 `Init*` 版本） |
| 运行时（BeginPlay / 蓝图） | `SetBoxExtent` 可以，安全 |

### 顺带修复的迁移代码（同类隐患）

排查时发现迁移来的 3 个 Actor **只要拖进关卡就会崩编辑器**，已一并修掉：

| 文件 | 行 | 修改 |
|---|---|---|
| `Climb/ClimbSpot.cpp` | 17 | `TriggerBox->SetBoxExtent` → `InitBoxExtent` |
| `LightReveal/RevealLightVolume.cpp` | 15 | `LightZone->SetBoxExtent` → `InitBoxExtent` |
| `LightReveal/RevealPlatform.cpp` | 16 / 26 | `SolidBox` / `DetectorBox` → `InitBoxExtent` |

> `OverlapPassage/OverlapPassageComponent.cpp:212` 的 `SetBoxExtent` 在 **运行时** `BuildProxyGrid()` 里，
> 属于安全用法，**未改动**。
