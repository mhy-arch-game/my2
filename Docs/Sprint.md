# 疾跑（USprintComponent）

> 需求：**按下 Shift 进入疾跑状态。**

## 1. 直接用

角色蓝图 `/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` 上**已经挂好** `Sprint` 组件，
**左右 Shift 都认**（默认键 `LeftShift` + 自动同时映射 `RightShift`）。

- **按住** Shift → `MaxWalkSpeed` 从角色原本的值变成 **650**；
- **松开** → 还回角色原本的速度。

不需要改蓝图、不需要改 IMC、不需要建输入资产。**打开 PIE 就能跑。**

## 2. 这次做了什么

| 产物 | 说明 |
|---|---|
| `Source/MHY_ARCH_GAME/Sprint/SprintComponent.h/.cpp` | 疾跑组件 |
| `BP_FirstPersonCharacter` 上的 `Sprint` 组件 | 已用脚本挂好并跨进程核验 |
| `/Game/Input/Actions/IA_Sprint` | 顺带建好的正式输入动作资产（**可选**，见 §3） |

## 3. 工作原理

```
Shift 按下
   │  组件自建的运行时 mapping context（Shift -> SprintAction）
   v
EnhancedInputComponent 绑定（Started / Triggered / Completed）
   │
   v
USprintComponent：合成"是否疾跑"（还要过 蹲下/方向/滞空 等条件）
   │
   v
UCharacterMovementComponent::MaxWalkSpeed = SprintSpeed
```

### 为什么要在运行时自建 mapping context？

本项目的 **E 键曾经失效过**，根因是 `IMC_Interaction` 注册在关卡根本不用的
PlayerController 上（详见 `Docs/InteractionPickingFix.md`）。疾跑沿用同一个**已被验证**的做法
（`UInteractionDetectorComponent::RegisterInteractContext`）：

- 组件自己 `NewObject` 一个 `UInputMappingContext` 并 `AddMappingContext` 到本地玩家；
- 所以**不需要动 `IMC_Default`**（脚本改 IMC 很不安全，之前确认过）；
- 等你把 Shift 正式做进项目自己的 IMC 之后，把 **`bRegisterSprintContext` 关掉**即可。

### 输入动作：可以不建资产

`SprintAction` 留空时，组件会**运行时自建一个 Boolean 动作**，所以零配置可用。
想用正式资产就把 `/Game/Input/Actions/IA_Sprint` 拖进 `SprintAction` 字段。

### 速度的取与还原

`BeginPlay` 记下角色原本的 `MaxWalkSpeed` 作为"正常速度"，不疾跑时写回去。
这是本项目一贯的模式（同 `GravityZone`），**不会覆盖别的系统对速度的调整**。

## 4. 设置在哪（Details 里）

所有属性都在**同一个扁平分类 `Sprint`** 下 → Details 搜索框输入 **`sprint`** 即可。

| Details 里显示为 | C++ 名 | 默认 | 说明 |
|---|---|---|---|
| **Sprint Speed** | `SprintSpeed` | 650 | 疾跑时的 `MaxWalkSpeed` |
| **Normal Speed Override** | `NormalSpeedOverride` | 0 | 非疾跑速度；**0 = 用角色原本的值**（推荐） |
| **Sprint Action** | `SprintAction` | 空 | 输入动作；空 = 运行时自建 |
| **Sprint Key** | `SprintKey` | `LeftShift` | 触发键 |
| **Include Right Shift** | `bIncludeRightShift` | ☑ | 同时认右 Shift |
| **Hold To Sprint** | `bHoldToSprint` | ☑ | 按住疾跑；取消勾选 = 按一下切换 |
| **Register Sprint Context** | `bRegisterSprintContext` | ☑ | 自建运行时 mapping context |
| **Sprint Context Priority** | `SprintContextPriority` | 0 | 上面那个 context 的优先级 |
| **Require Forward Input** | `bRequireForwardInput` | ☐ | 只在朝前跑时疾跑（掉头退出） |
| **Min Forward Dot** | `MinForwardDot` | 0.25 | 上面的判定阈值（加速度·前向 的点积下限） |
| **Ignore When Crouched** | `bIgnoreWhenCrouched` | ☑ | 蹲下不允许疾跑 |
| **Keep Speed In Air** | `bKeepSpeedInAir` | ☑ | 滞空保持疾跑速度 |
| **Sprint Enabled** | `bSprintEnabled` | ☑ | 总开关（过场/剧情限制） |
| **Log State Changes** | `bLogStateChanges` | ☑ | 状态变化打日志 |
| **Draw On Screen Debug** | `bDrawOnScreenDebug` | ☐ | 屏幕上一行实时状态 |

## 5. 蓝图里怎么用

| 节点 / 事件 | 用途 |
|---|---|
| **`On Sprint Changed (bSprinting)`** | 状态变化时触发 → 接镜头 FOV、角色动画速度、切换脚步音效 |
| **`Is Sprinting`** | 当前是否疾跑 |
| **`Is Sprint Requested`** | 键是否按住（切换模式下表示"逻辑上想疾跑"） |
| **`Set Sprinting (bool)`** | 直接设置状态（走路过场、剧情强制走动） |
| **`Set Sprint Enabled (bool)`** | 开关整个功能 |
| **`Get Sprint Debug String`** | 一行状态文本，给 `Print String` 用 |
| **`Get Authored Walk Speed`** | 角色原本的速度（做 UI/数值参考） |

典型扩展（不需要改 C++）：

- **镜头 FOV 拉伸**：`On Sprint Changed` → `Set Field of View`（记得同时做插值）；
- **动画/音效**：`On Sprint Changed` → 切换 AnimBP 的 `bSprinting` / 播放疾跑脚步音；
- **体力条**：在 `On Sprint Changed` 里对体力计时，体力耗尽时调 `Set Sprint Enabled(false)`
  （建议加个协程/计时器，体力恢复后再打开）。

## 6. 调试

### 6.1 日志（默认开）

```
[Sprint] BP_FirstPersonCharacter_C_0: 就绪（按住，键 LeftShift，原始 MaxWalkSpeed 300 -> 疾跑 650）。
[Sprint] BP_FirstPersonCharacter_C_0: 已绑定疾跑输入。
[Sprint] BP_FirstPersonCharacter_C_0: 已注册运行时疾跑 context（LeftShift / RightShift）。
[Sprint] BP_FirstPersonCharacter_C_0: 进入疾跑 (MaxWalkSpeed -> 650)
[Sprint] BP_FirstPersonCharacter_C_0: 退出疾跑 (MaxWalkSpeed -> 300)
```

**看前三条就知道接线是否成立**——缺哪条就对应哪个环节没成（见 §6.3）。

### 6.2 实时状态

- 勾 **`Draw On Screen Debug`**：屏幕左上角一行实时刷新；
- 或者蓝图里 `Get Sprint Debug String` → `Print String`。

格式：

```
Sprint <owner> | sprinting=YES | requested=yes | speed=650 (authored 300, sprint 650) | key=LeftShift | bound=yes ctx=yes
```

一眼看出：**是否真的在疾跑 / 键是否按住 / 当前速度与两个基准值 / 绑定与 context 是否就绪**。

### 6.3 故障排查表

| 现象 | 原因 | 解法 |
|---|---|---|
| 按 Shift 完全没反应，**且日志缺"已绑定疾跑输入"** | 角色的 `InputComponent` 不是 `UEnhancedInputComponent`，或 owner 不是 Pawn | 本项目用的是 Enhanced Input；确认组件挂在角色（Pawn）上而不是别的 actor 上 |
| 缺"已注册运行时疾跑 context" | `bRegisterSprintContext` 被关，或玩家还没被 Possess | 打开该开关；`bRegisterSprintContext=false` 时请自己把 Shift 做进项目 IMC |
| 有"进入疾跑"日志但速度没变 | `SprintSpeed` 等于当前速度，或别的系统在覆盖 `MaxWalkSpeed` | 检查两个速度值；项目目前没有别的系统写 `MaxWalkSpeed` |
| 按了没用，但日志里 `sprinting=no requested=yes` | 条件拦住了：蹲着 / 掉头（`bRequireForwardInput`）/ 滞空（`bKeepSpeedInAir=false`） | 按需关掉对应条件 |
| 跑着跑着自己掉出疾跑 | `bRequireForwardInput` 开着且方向掉头，或滞空 | 关掉该条件，或把 `MinForwardDot` 调低 |
| 松手还在跑 | `bHoldToSprint` 被取消了（切换模式） | 这是预期行为；勾回来就是按住 |
| 想临时禁掉疾跑 | — | `Set Sprint Enabled(false)`，或在 Details 关 `Sprint Enabled` |
| 找不到设置 | 分类名与组件名不同词根的老问题 | 本组件分类就是 **`Sprint`**，Details 搜 `sprint` 一定能命中 |

## 7. 与其它系统的关系

- **`GravityZone`**：只改重力系数（`GravityScale` / `JumpZVelocity`），不碰 `MaxWalkSpeed`，互不干扰；
- **`InteractionDetector` / `InteractionLink` / `TimeEraPortal`**：都只读输入，不改速度；
- **`Climb` / `MovementAudio`**：Climb 会切换移动模式，疾跑在攀爬时不会生效（不在 Walking 模式）；
  若你希望攀爬中强制非疾跑，可以在 Climb 开始时 `Set Sprint Enabled(false)`；
- **蹲下**：默认 `bIgnoreWhenCrouched=☑`，蹲下自动退出疾跑、站起后若键还按着会自己恢复。

## 8. 现状（已核验）

| 项 | 状态 |
|---|---|
| `USprintComponent` | ✅ 已编译，反射正常（默认 `SprintSpeed=650`、键 `LeftShift`） |
| `IA_Sprint` 资产 | ✅ 已创建于 `/Game/Input/Actions/IA_Sprint` |
| 挂在 `BP_FirstPersonCharacter` 上 | ✅ 已加（父组件 `CapsuleComponent`），compile+save 成功 |
| **跨进程核验**（重新加载蓝图再读） | ✅ `has_sprint=true`，组件及其默认值都在 |
| 角色蓝图备份 | `Saved/Backup/BP_FirstPersonCharacter_before_sprint.uasset`（140.3 KB） |
| PIE 实跑 | ⏳ 未做（打开 PIE 按 Shift 即可验证；先看 §6.1 的三条就绪日志） |

### 想调整数值

在 PIE 之外的编辑器里选中角色的 `Sprint` 组件 → Details 搜 `sprint` → 改 **Sprint Speed**。
想默认更快/更慢，也可以改 C++ 默认值（`SprintSpeed = 650`）后重编译。
