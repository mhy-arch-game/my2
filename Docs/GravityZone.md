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
| `bScaleJumpVelocity` | ✗ | 同时把 `JumpZVelocity` 乘以 `sqrt(倍率)`。**这是"改重力但不改跳跃高度"的总开关，两个方向都成立**：`倍率<1` = 跳一样高但滞空更久（月球重力）；`倍率>1` = 跳一样高但上升下落更快（干脆）。默认关：高度会随重力反比变化（`<1` 跳更高，`>1` 跳更低） |
| `TargetJumpHeight` | 0（关） | **绝对**目标高度（cm）。> 0 时启用并**优先于** `bScaleJumpVelocity`：不管本区重力多少，最高点都正好是这个高度。见第 4.5 节 |
| 盒体尺寸 | 400×400×200 | 在组件 Details 的 `Shape → Box Extent` 调，或视口里拖 |

**可读查询 / 事件**

| 接口 | 用途 |
|---|---|
| `IsActorInside(Actor)` | 某 Actor 当前是否在区域内 |
| `GetTrackedActorCount()` | 当前区域内有几个被影响的 Actor |
| `OnActorEntered(Actor, AppliedGravityScale)` | 进入时广播（可播特效 / 音效） |
| `OnActorExited(Actor, AppliedGravityScale)` | 离开时广播（携带还原后的值） |
| `GetJumpHeightScale()` | 高度变化倍率：`bScaleJumpVelocity` 打开时恒为 `1`（高度不变），否则为 `1/倍率` |
| `GetAirTimeScale()` | 空中时间变化倍率：开了是 `1/√倍率`，没开是 `1/倍率`（越小越干脆） |
| `ConfigurePreservingJumpHeight(倍率, true)` | 一键表达"同样跳多高、但更快/更飘"，见第 4 节 |

---

## 3. 怎么搭（编辑器）

1. **Place Actors → 空 Actor**（或 `Basic → Actor`），拖进关卡
2. 选中它 → **+ Add → `Gravity Zone`**
3. 把该组件拖到 `DefaultSceneRoot` 上方替换它，让它成为 **Root**（摆放更方便）
4. 调 `Shape → Box Extent` 到你要的方正范围
5. 设 `Gravity Scale Inside`（例如 `0.3`）
6. 若希望**跳跃高度不变**（无论变飘还是变干脆），勾 `bScale Jump Velocity`
   - 变飘：倍率填 `0.3`
   - **变干脆（需求）**：倍率填 `1.8`～`2.5`，见第 4 节
7. Play → 角色走进区域手感立刻改变，走出应精确恢复原值

> 想直接挂到角色身上做自身周围的重力场也可以 —— 组件跟随 Actor，效果一样。

---

## 4. ★ 需求实现：最高点不变，但上升 / 下落更快

**结论：不需要新代码 —— `GravityScaleInside > 1` + 勾上 `bScaleJumpVelocity` 就是需求要的效果。**
为了让这个意图在编辑器里自解释，额外加了一个一键配置函数（第 4.2 节）。

### 4.1 数学依据

竖直上抛：最高点 `h = v² / (2g)`，上升时间 `t = v / g`。
设区域内重力 `g' = k·g`，同时把起跳速度 `v' = √k·v`：

| 量 | 区域内 | 与原来相比 |
|---|---|---|
| **最高点 h** | `(√k·v)² / (2·k·g) = v²/(2g)` | **完全不变** |
| 上升时间 | `√k·v / (k·g) = t / √k` | **快 √k 倍** |
| 下落时间 | 同理 `t / √k` | 快 √k 倍 |
| 落地速度 | `√k·v` | 更快（落地更"重"） |

k 取值参考：

| k | 高度 | 空中时间 | 手感 |
|---|---|---|---|
| 1.5 | 不变 | ×0.82 | 略干脆 |
| **1.8** | 不变 | ×0.75 | **推荐起步值** |
| 2.5 | 不变 | ×0.63 | 明显沉重 |
| 4.0 | 不变 | ×0.50 | 接近"两倍速" |

> `k < 1` 就是原本的月球重力用法。**同一个开关同时覆盖两个方向**，
> 需求里的用法只是把它用到 `k > 1`，所以物理部分零改动。

### 4.2 怎么配

**编辑器**：`Gravity Scale Inside` = `1.8`，勾 `bScale Jump Velocity`，其余不变。

**蓝图 / C++**（推荐，意图自解释）：

```cpp
GravityZone->ConfigurePreservingJumpHeight(1.8f, /*bAlsoScaleJumpVelocity=*/true);
```

### 4.3 其他可选实现路径（供参考，**当前未采用**）

| 方案 | 做法 | 评价 |
|---|---|---|
| **A. 本组件（已采用）** | 重力 ×k、起跳速度 ×√k | 只影响 Pawn；改两个浮点，零开销；**高度精确不变** |
| B. 非对称重力 | 自定义 `UCharacterMovementComponent`，上升/下落用不同倍率（下落再乘一个系数） | 能分别调"上快/下快"，做出"上慢下快"的厚重感；需子类化并覆写 `PhysFalling`，改动大 |
| C. 引擎 `APhysicsVolume` | 用 `GravityZ` / `TerminalVelocity` | 连物理刚体一起改；**无法自动放大起跳速度**，仍需本组件补跳速 |
| D. 只改角色自身 | 直接在 `BP_FirstPersonCharacter` 的 `CharacterMovement` 上调 | 全关卡生效，做不到"只在区域内变" |
| E. 纯表现 | 不改物理，只加快跳跃动画与镜头 | 不改变真实运动，不满足需求 |

> 若以后要 B 那种"上升慢、下落快"的落差感，在 `bScaleJumpVelocity` 的基础上
> 再给下落单独一个大系数即可，本组件**已有的缓存/还原机制无需改动**。

### 4.4 注意

- **必须同时勾 `bScaleJumpVelocity`**：只把重力调大而不放大起跳速度，跳跃会变**低**（`h/k`），不是需求要的效果。
- 若 `bApplyGravityWhileJumping` 被关掉，上升段公式不再成立（本组件不修改该开关）。
- 角色按住跳跃且 `ACharacter::JumpMaxHoldTime > 0` 时会额外续力，此时"高度不变"只是近似——本组件**不**改这个值。

---

### 4.5 场景 B：本区原本就是"低重力大跳"，要**保留那个高度**

`bScaleJumpVelocity` 的参考系是**正常重力（倍率 1）**，它只能保住"正常重力下的高度"。
如果区域本身把重力压得极低（本项目 `jumping_area` 就是 `GravityScaleInside = 0.08`），
开这个开关反而会把跳跃**拉回正常高度** —— 不是需求要的。

这时用 **`TargetJumpHeight`（cm）**：填"我想要跳多高"，组件按本区实际重力反算起跳速度

```
v = sqrt(2 · |GetGravityZ()| · 高度)          // 高度 = v² / (2g) 反解
```

**与角色自身的 `JumpZVelocity` 无关**，高度精确命中；`TargetJumpHeight > 0` 时优先于 `bScaleJumpVelocity`。

> 注意 UE5 的 `UCharacterMovementComponent::GetGravityZ()` **内部已经乘过 `GravityScale`**
> （`return Super::GetGravityZ() * GravityScale;`），取到的就是角色此刻真正受到的重力，代码里没有再乘一次。

#### 本项目实测值

角色 `JumpZVelocity = 420`、`GravityScale = 1.0`、`MaxWalkSpeed = 600`、`AirControl = 0.6`。
原本 `0.08` 重力下的高度 = `420² / (2 × 0.08 × 980)` = **1125 cm（11.25 m）**，上升 5.36 s、总滞空 10.71 s。

**保持 1125 cm 不变**，只调重力倍率 `k`（`k` 越大越快，高度永远不变）：

| `GravityScaleInside` | 速度倍率 | 反算起跳速度 | 上升时间 | 总滞空 | 水平射程（600 cm/s） |
|---|---|---|---|---|---|
| 0.08（原值） | 1.00× | 420 | 5.36 s | 10.71 s | 64.3 m |
| 0.16 | 1.41× | 594 | 3.79 s | 7.58 s | 45.5 m |
| **0.32（当前已应用）** | **2.00×** | **840** | **2.68 s** | **5.36 s** | **32.1 m** |
| 0.64 | 2.83× | 1188 | 1.89 s | 3.79 s | 22.7 m |
| 1.00（正常重力） | 3.54× | 1485 | 1.52 s | 3.03 s | 18.2 m |

**应用位置**：`/Game/bclass_source/jumping_area` 的 BP 模板 + 关卡 `firstvision` 里的那个 `jumping_area`
（标签 `jumping_area`，位置 `(-7850, 615, 100)`，`BoxExtent ≈ (775, 698, 3598)`）。

> ⚠ **关卡里的当前值请以编辑器 Details 为准，不要照抄文档。**
> 这两个数是**可调的设计参数**，编辑器与脚本都会改它；实测出现过
> "脚本写入 1125/0.32 并复验通过 → 编辑器保存后变回它内存里的值（1.2/550）"。
> 记忆口诀：**高度只由 `TargetJumpHeight` 决定，速度只由 `GravityScaleInside` 决定。**
>
> 若关卡里是 `TargetJumpHeight = 550` + `GravityScaleInside = 1.2`，则：
> 实际重力 `1176 cm/s²`、起跳速度 `1137 cm/s`、上升 `0.97 s`、总滞空 `1.93 s`、
> 水平射程约 `11.6 m` —— 相对最初的 `0.08` 方案是**高度 1125→550、弧线快约 5.5 倍**。

#### ⚠ 代价：水平射程同比缩短

滞空变短 ⇒ **同样跑速下水平跳距同比变短**（上表 64.3 m → 32.1 m）。
若关卡里有按旧射程设计的跨距，需要一并调整。想**同时保住射程**有两条路（**尚未实现，等你定**）：

| 方案 | 做法 | 代价 |
|---|---|---|
| 1. 区域内同步提速 | 把 `MaxWalkSpeed` 也乘 `√(k/旧k)`（0.32 时 600 → 1200） | **地面跑速也会变快**，整个区域变成"快区" |
| 2. 只在空中提速 | 组件 tick 里判断 `Movement->IsFalling()`，空中用提高后的速度上限、落地还原 | 射程不变且地面不受影响，但每帧要多维护一次速度上限 |

另一个更"重"的观感路线（方案 B，见 4.3）是**非对称重力**：上升仍然轻、下落额外加重，
做出"跳上去飘、掉下来快"的落差，这需要在自定义 `UCharacterMovementComponent` 里覆写 `PhysFalling`。

---

## 5. 已知限制

| 项 | 说明 |
|---|---|
| **多个重力区重叠不叠加** | 每个区各自缓存 / 还原。重叠时后进入的生效，退出还原为原始值。**请让区域互不重叠** |
| 只改角色重力 | 只作用于 `UCharacterMovementComponent`（角色）。物理刚体的 `bEnableGravity` / 质量不受影响 |
| 无渐变 | 进出是瞬时的。想要平滑过渡可在 `OnActorEntered` / `OnActorExited` 里自己插值 |
| 运行时改 `GravityScaleInside` | 不会自动应用到已在区域内的角色；需先离开再进入 |
| 角色在区域内被销毁 | 移动组件随之消失，无需还原；重生后重新进入会重新缓存 |
| 组件被销毁 | `EndPlay` 会把仍在区域内的角色逐个还原，不会让角色一直飘着 |

---

## 6. 相关文档

| 文档 | 内容 |
|---|---|
| `Docs/InteractionImplementation.md` | 交互实现（角色端 / 门端 / 灯端） |
| `Docs/EditorImplementationGuide.md` | 编辑器内总体搭建指南 |
---

## 7. ★ 踩坑记录：构造函数里必须用 `InitBoxExtent`

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
