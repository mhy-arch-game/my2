# 跨时空传送（UTimeEraPortalComponent）

> 需求：「指定对应的对象（object）在交互时把主控角色传送至对立时空对应的位置，对应时空也存在
> 一个相应的对象」。整个模块**不新增接口**——沿用既有的 `IInteractableInterface` / `UInteractableComponent`
> 交互管线，把新行为作为**组件**挂到任意对象上。

## 0. 两种成对方式（`TargetMode`）

| 模式 | 落点从哪来 | 需要配什么 |
|---|---|---|
| **Counterpart Object（指定对应物）** | 另一个 actor 的位置 | 指认 `CounterpartActor`，或两边填同一个 `CounterpartId` |
| **Vertical Offset（仅 Z 不同）** | **自己位置 + (0,0,±VerticalOffset)**，不需要任何参照物 | 只填一个 `VerticalOffset`，**两半填同一个值** |

### 0.1 Vertical Offset：两个时空只是"上下错开"

`VerticalOffset` 的定义是 **今 − 古**（现代那一半比古代高多少；现代更低就填负数）。
组件按**自己所属时空自动取符号**：

- 本对象在 **古** → 落点 = 自己位置 + (0,0,**+**VerticalOffset)
- 本对象在 **今** → 落点 = 自己位置 + (0,0,**−**VerticalOffset)

⇒ **两半填同一个值**。这正是"仅 Z 不同"能自动配对的原因：**不需要任何引用**，
也杜绝了"一半填正一半填反"这种低级错误。

### 0.2 接线（β方案：手工挂组件）

以"古代地面的一块踏板 / 现代同位置半空的同一块踏板"为例：

1. **放两个对象**，X/Y 完全相同，只有 Z 相差 Δ：
   - 古：`(1000, 500, 100)`
   - 今：`(1000, 500, 1100)` → **Δ = +1000**
2. **两半各自**：
   - **Add Component → Time Era**，`Era` 分别设成 `Ancient` / `Modern`
     （这是"它属于哪个时空"的依据，Portal 的 `bAutoDetectEra` 默认会读它）
   - **Add Component → Time Era Portal**
   - **Add Component → Interactable**（交互入口；不挂的话 Portal 会自动建一个）
3. **两半的 Portal 上都设成同一套**：
   - `Target Mode` = **Vertical Offset (仅 Z 不同)**
   - `Vertical Offset` = **1000**（同一个值，不要一正一负）
   - `Interaction Prompt` 填提示文案（会推给 Interactable）
4. **Compile + 重启编辑器**。靠近任一踏板按 **E** → 切到另一个时空并落到另一半的位置；
   再按 E 又能回去。
5. 只想传送、不要对象本身开合，勾 `bSuppressBuiltInToggle`。

> 为什么两半都能"按 E 回去"：`VerticalOffset` 的符号是按**自己**的时空取的，
> 所以古今两半各自算出的落点正好互为对方的家。

### 0.3 什么时候用哪个

| 场景 | 用哪个 |
|---|---|
| 两个时空里只有**高度**不同（同一块平台 / 梯子 / 柱子） | **Vertical Offset**（零引用，复制后不用再连线） |
| 两个时空里位置**完全不同**（古代在山脚、现代在别处） | **Counterpart Object** |

### 0.4 调试

**成功传送会打一行日志**（括号里依次是 目标时空 / 成对方式 / 最终落点）：

```
[TimeEraPortal] Pedestal_Ancient -> <仅 Z 不同的另一半> (今, VerticalOffset, 1000/500/1100)
```

| 现象 | 原因 | 解法 |
|---|---|---|
| 按 E 没反应，日志说"时空切换仍在冷却" | `UTimeShiftSubsystem` 的冷却锁（默认 2 秒）还没过 | 等一会儿；或调小 `CooldownDuration`（见 `Config/DefaultGame.ini`） |
| 传过去了但**跑到了地下/半空** | `VerticalOffset` 符号或数值不对 | 核对两半的 `TimeEraComponent::Era` 是一古一今（符号就是从它来的），且 `VerticalOffset` **两半同值**；看日志里的落点 Z 对不对 |
| 落点被"拉"到地面 | `bPlaceOnGround` 默认开，会从落点向下打射线贴地 | 如果垂直位移后应该停在半空，关掉它 |
| 传过去后另一半**没出现** | 那个对象的 `TimeEraComponent` 缺失或 `Era` 设错 | 两半都要挂 `Time Era` 并设对`Era` |
| 想用 `ResolveCounterpart()` 判断，结果恒为 null | Vertical Offset 模式**没有"对应对象"**这个概念 | 用 `CanUsePortal()` 判断可用性；`OnPortalUsed` 的第三个参数在该模式下是 null |

其余公共机制（交互绑定、先切时空再落点、每次使用后的冷却锁、拒绝原因广播）与
"指定对应物"模式完全一致，见下面各节。

## 0.5 配对与正确传送：到底要调哪些字段

**配对（找到另一半）** 和 **传送（落到哪里）** 是两件事，分属不同字段。先看全景，再按模式照抄最小配置。

### 0.5.1 五组字段全景

| 组 | 字段 | 影响什么 | 调错的典型症状 |
|---|---|---|---|
| **A. 我属于哪个时空** | `OwnerEra`、`bAutoDetectEra`（+ 对象上的 `TimeEraComponent::Era`） | ① 另一半在哪个时空 ② Vertical Offset 的正负号 | 配对反了；该往上却往下 |
| **B. 另一半怎么找** | `TargetMode`、`CounterpartActor`、`CounterpartId`、`VerticalOffset` | 落点的**基准位置** | 提示"找不到对应的对象"；落点跑到别处 |
| **C. 落到哪、怎么落** | `TeleportOffset`、`bMatchCounterpartYaw`、`bPlaceOnGround`、`GroundTraceDistance`、`GroundClearance` | 在基准位置上再微调 | 卡进地板 / 悬空 / 朝向不对 |
| **D. 交互入口** | `bAutoUseInteractableOnOwner`、`InteractionPrompt`、`bSuppressBuiltInToggle` | 能不能按 E、提示文案、是否顺带开合 | 没提示 / 按 E 没反应 / 门既开合又传送 |
| **E. 其它** | `bSwitchEra`、`bRequireCounterpartInOtherEra`、`PortalCooldown`、`bDisableInteractableWhileLocked` | 传送时切不切时空、冷却 | 传过去目标物体不可见 / 连按没反应 |

### 0.5.2 A 组：`OwnerEra` 的取值规则（本轮刚修）

**原来这里有个坑**：`OwnerEra` 带着 `EditCondition="!bAutoDetectEra"`，所以默认情况下它在 Details 里是**灰的、改不了**；
而 `bAutoDetectEra` 开着却找不到 `TimeEraComponent` 时，代码恰恰要**回退到 `OwnerEra`** ——
于是这个字段"既要被用、又改不了"，直接卡死配对。

**现在已去掉限制，`OwnerEra` 始终可编辑。** 实际取值规则：

| `bAutoDetectEra` | 对象上有 `TimeEraComponent`？ | 实际使用的时空 |
|---|---|---|
| ☑（默认） | 有 | **`TimeEraComponent::Era`**（`OwnerEra` 被忽略） |
| ☑（默认） | 没有 | **回退到 `OwnerEra`**（BeginPlay 打 Warning） |
| ☐ | 任意 | **`OwnerEra`** |

**不用猜——BeginPlay 会直接把这行打出来：**

```
[TimeEraPortal] Pedestal_Ancient: 所属时空 = 古 Ancient（来源：TimeEraComponent）；配对方式 = Vertical Offset；目标时空 = 今 Modern。
```

来源字段可能是：`TimeEraComponent` / `OwnerEra（回退：没找到 TimeEraComponent）` / `OwnerEra（bAutoDetectEra 已关）`。

两种配错会**额外 Warning**：

- `bAutoDetectEra` 开着但没有 `TimeEraComponent` → 提示你补组件，或确认 `OwnerEra` 是对的；
- `OwnerEra` 与 `TimeEraComponent::Era` **不一致** → 提示实际以组件为准，并叫你把两者改成一致或关掉 `bAutoDetectEra`。

### 0.5.3 按模式照抄最小配置

**模式一：Vertical Offset（两个时空仅 Z 不同）**

| 字段 | 古那一半 | 今那一半 |
|---|---|---|
| `TimeEraComponent::Era` | `Ancient` | `Modern` |
| `bAutoDetectEra` | ☑ | ☑ |
| `TargetMode` | Vertical Offset | Vertical Offset |
| **`VerticalOffset`** | **1000** | **1000（同值！不要一正一负）** |
| `InteractionPrompt` | 任意文案 | 任意文案 |
| `bPlaceOnGround` | 视需求（要停在半空就关掉） | 同左 |

**模式二：Counterpart Object（位置完全不同）**

| 字段 | 甲 | 乙 |
|---|---|---|
| `TimeEraComponent::Era` | `Ancient` | `Modern` |
| `bAutoDetectEra` | ☑ | ☑ |
| `TargetMode` | Counterpart Object | Counterpart Object |
| 配对（二选一） | ① `CounterpartActor` 互相指认；② 两边填**同一个** `CounterpartId` 且保持 `bRegisterAsAnchor` 开着 | 同左 |
| `bRequireCounterpartInOtherEra` | ☑（防止把同侧物体连上） | ☑ |

### 0.5.4 症状 → 该调哪个字段

| 症状 | 先调这里 |
|---|---|
| 提示"对立时空里找不到对应的对象" | **B**：`TargetMode` 选对了吗；`CounterpartActor`/`CounterpartId` 两边一致吗 |
| 提示"指定的对应对象与本体处在同一时空" | **A**：两半的 `Era` 是不是一古一今 |
| 传送方向反了（该往上却往下） | **A**：`VerticalOffset` 是否**两半同值**；`Era` 是否正确 |
| 传过去卡进地板 / 悬空 | **C**：`bPlaceOnGround`、`GroundTraceDistance`、`GroundClearance`；对照日志里的最终落点 |
| 传过去朝向不对 | **C**：`bMatchCounterpartYaw` |
| 没有提示 / 按 E 没反应 | **D**：`InteractionPrompt` 填了吗；对象上有可交互组件吗（`bAutoUseInteractableOnOwner` 开着会自动建）；探测器在这个距离能命中吗 |
| 对象既开合又传送 | **D**：勾 `bSuppressBuiltInToggle` |
| 传过去后**目标物体不可见** | **A**：那件物体自己有没有 `TimeEraComponent`、`Era` 设对了没 |
| 连按没反应 | **E**：`PortalCooldown`，以及时空切换自身的冷却（默认 2 秒） |
| 完全不知道当前算出来什么 | 蓝图调 **`Get Portal Debug String`**（见 0.5.5） |

### 0.5.5 两个"看结果"的入口

**① BeginPlay 一行**（每个 Portal 各一条）：

```
[TimeEraPortal] <对象>: 所属时空 = 今 Modern（来源：OwnerEra（bAutoDetectEra 已关））；配对方式 = Counterpart Object；目标时空 = 古 Ancient。
```

**② 每次成功传送一行**，括号里依次是 **目标时空 / 配对方式 / 最终落点**：

```
[TimeEraPortal] Pedestal_Ancient -> <仅 Z 不同的另一半> (今, VerticalOffset, 1000/500/1100)
```

**③ 蓝图节点 `Get Portal Debug String`**（`BlueprintPure`，接 `Print String`）：

```
Portal Pedestal_Ancient | mode=VerticalOffset | ownerEra=Ancient (TimeEraComponent) | targetEra=Modern | verticalOffset=1000.0 | dest=1000,500,1100 | counterpart=none | locked=no
```

一行里就包含：**配对方式 / 本对象时空及其来源 / 目标时空 / Z 偏移 / 解析出的落点 / 对应物 / 是否冷却中**。
解析失败时 `dest` 会直接写成失败原因（例如 `<解析失败: 传送装置没有 owner。>`）。

> 注意：它会**真的跑一次落点解析**（对应物模式下可能扫场景），排查时用，别每帧调。

## 1. 新增文件

| 文件 | 作用 |
|---|---|
| `Source/MHY_ARCH_GAME/TimeShift/TimeEraPortalComponent.h` | 声明 |
| `Source/MHY_ARCH_GAME/TimeShift/TimeEraPortalComponent.cpp` | 实现 |

已随模块编译通过（`UnrealEditor-MHY_ARCH_GAME.dll`）。

## 2. 设计：只接入一个接口

交互管线本来就有两条等价入口（见 `InteractableInterface.h`）：
类上实现接口，或挂 `UInteractableComponent`。本组件选后者，并且**自动接线**：

```
BeginPlay:
  找 owner 的 UInteractableComponent
     └ 没有且 bAutoUseInteractableOnOwner → NewObject 建一个并 RegisterComponent
  绑定 OnInteractRequested → HandleInteractRequested → TryUsePortal(Interactor)
```

所以设计者只需要在对象上加 **一个** `TimeEraPortal` 组件，聚焦/描边/提示/E 键全部由
既有的 `UInteractionDetectorComponent` 负责，不需要任何蓝图连线。

## 3. 在编辑器里接线（β方案：手工挂组件）

1. 打开对象所在的蓝图（例如 `Content/bclass_source/active_door`），或直接选中关卡里的实例。
2. **Add Component → TimeEraPortal**。
3. 确保该对象能表达自己属于哪个时空：
   - 对象上已有 `TimeEraComponent` → `bAutoDetectEra`（默认开）自动取它的 `Era`；
   - 没有的话，关掉 `bAutoDetectEra` 并手工设 `OwnerEra`。
4. 指定对应物（二选一，优先级从上到下）：
   - `CounterpartActor`：直接引用**对立时空**里的那个对象（最直观）；
   - `CounterpartId`：两边的对象填**同一个名字**，运行时由时空锚点注册表解析
     （`bRegisterAsAnchor` 默认开，会把本体以该 id 注册进 `UTimeShiftSubsystem`）。
5. 对立时空里那个对象**同样要挂** `TimeEraPortal`（各自指认对方），这样一个门两边都能走。
6. 若只想传送、不要对象本身的开合动画，勾 `bSuppressBuiltInToggle`。
7. 交叉编译后必须重启编辑器，组件才会出现在 Add Component 列表里。

## 4. 关键属性

| 属性 | 默认 | 说明 |
|---|---|---|
| `CounterpartActor` | 空 | 对立时空的对应对象（最高优先级） |
| `CounterpartId` | None | 共享 id；本体注册为时空锚点，对应物按 id 在对立时空解析 |
| `bRegisterAsAnchor` | true | 把本体注册进子系统，使**通用**时空切换（Alt 切时空）在这些点也能配对 |
| `bAutoDetectEra` / `OwnerEra` | true / Ancient | 本体所属时空 |
| `bSwitchEra` | true | 使用时同时把激活时空切到对应物所在时空 |
| `bRequireCounterpartInOtherEra` | true | 对应物与本体同时空的则拒绝（防配错） |
| `TeleportOffset` | 0 | 落点偏移，**在对应物的局部空间**里解释 |
| `bMatchCounterpartYaw` | true | 落地后朝向与对应物一致（同时同步 Controller 的 ControlRotation） |
| `bPlaceOnGround` | true | 从对应物向下打射线贴地（用胶囊半高/包围盒半高） |
| `GroundTraceDistance` / `GroundClearance` | 1000 / 2 | 射线半长 / 离地间隙 |
| `bAutoUseInteractableOnOwner` | true | 自动绑定/创建 `InteractableComponent` |
| `InteractionPrompt` | 空 | 非空时覆盖到交互组件上 |
| `bSuppressBuiltInToggle` | false | 关掉内置开合，只做传送 |
| `PortalCooldown` | 0.5s | 本装置的重复使用锁定 |
| `bDisableInteractableWhileLocked` | true | 锁定期内关掉交互组件，不显示可交互提示 |

事件：`OnPortalUsed(Traveler, Source, Counterpart)`、`OnPortalRefused(Portal, Reason)`。

> **落点微调（本项目当前值）**：`modern_swift_actor` 模板的 `TeleportOffset = (5, 0, 0)`，
> 即落点沿**装置局部 X** 偏 5 cm。`TeleportOffset` 是按"装置自身的旋转"换算的
> （Vertical Offset 模式用的是本装置的旋转，而每对两半的 yaw 相同，所以等价于目标装置的旋转）。
> 各对的实际世界方向：
>
> | 对 | 装置 yaw | 局部 +X 对应世界方向 |
> |---|---|---|
> | 1/2、5/6 | 0 | **世界 +X**（+5 cm） |
> | 3/4 | −90 | **世界 −Y**（−5 cm） |
> | 7/8、9/10 | +90 | **世界 +Y**（+5 cm） |
>
> 若要"**世界 X 统一 +5 cm**"，需要把偏移改成世界空间叠加（1 个开关或 1 行代码），当前**没有**这么做。
> 偏移在贴地之前叠加且只影响 X/Y —— 贴地只改 Z，所以这 5 cm 不会被吃掉。

## 5. 时序（这一段很重要）

```
TryUsePortal(Traveler):
  1. 解析对应物（显式引用 → 锚点注册表 → 全局搜同 id 的兄弟组件 → 最近的锚点）
  2. 【先】Subsystem->SetEra(对应物所在时空)
        → 广播 OnEraChanged：所有 UTimeEraComponent 门控显隐/碰撞，
          玩家身上的 UTimeShiftTravelComponent 会按 layout 映射搬一次家
  3. 【后】显式把角色 SetActorLocation 到对应物（覆盖第 2 步的结果）
        + 停止 CharacterMovement + 可选朝向 + 贴地
```

**必须先切时空再落点**：反过来的话第 2 步的通用映射会把我们刚算好的落点冲掉。

## 6. 拒绝情况（不传送，广播 OnPortalRefused）

- 找不到对应物：`对立时空里找不到对应的对象…`
- 对应物与本体同时空：`指定的对应对象与本体处在同一时空…`
- 时空切换冷却中（`UTimeShiftSubsystem::CanSwitchEra()` 为假）：`时空切换仍在冷却…`
- 本装置自身冷却中：`传送装置刚刚使用过，还在冷却。`

拒绝时**不改动任何状态**（时空、位置都不动）。

## 7. 与既有 TimeShift 的关系

- 没有新增 subsystem API；复用 `RegisterAnchor` / `FindAnchor` / `FindNearestAnchor`。
- `CounterpartId` 的语义与 `ATimeShiftAnchor::AnchorId` 一致，两套可以共存。
- 传送装置注册为锚点后，**通用**时空切换也能在这些点正确配对（副作用即特性）。

## 8. 过场动画 / UI 提示接口

传送的**状态变更与表现解耦**：Portal 只负责"什么时候开始 / 什么时候结束"，
动画、黑幕、UI 提示、音效由外部通过**蓝图事件**或**实现接口**自己接。

### 8.1 三段时间轴

1. **触发**：按 E → 校验通过 → 落点、目标时空、对应物**在此时就已算好**
2. **开始（`OnTransitionBegin`）**：广播过场上下文；若 `TransitionDelay > 0`，
   **真正的位移被推迟**，把时间留给过场表现（淡出 / 镜头 / UI）
3. **结束（`OnTransitionEnd`）**：延时到点后执行实际传送
   （切时空 → 落点 → 朝向），再广播一次，携带**最终落点**

> `TransitionDelay = 0`（默认）时第 2、3 步在同一帧完成 —— 行为与加接口前**完全一致**，老内容不受影响。

### 8.2 两种接法

**(a) 蓝图事件（最简单）**

选中挂着 `Time Era Portal` 的对象 → Details 面板里：

| 事件 | 时机 |
|---|---|
| `On Transition Begin` | 过场开始（位移**之前**） |
| `On Transition End` | 传送完成（位移**之后**） |

两者都带参数 `Context`（`FTeleportTransitionContext`），直接拿去播动画 / 弹 UI。

**(b) 实现接口 `ITeleportTransitionInterface`（跨对象、可复用）**

任意蓝图类实现 `Teleport Transition Interface` 之后，Portal 会**自动**找到它并调用，
不需要连任何线：

| 函数 | 语义 |
|---|---|
| `CanReceiveTeleportTransition(Context)` → bool | 返回 false 则跳过这个接收者 |
| `OnTeleportTransitionBegin(Context)` | 过场开始 |
| `OnTeleportTransitionEnd(Context)` | 传送完成 |

查找方式为遍历关卡内实现者；`bDispatchTransitionToInterfaceListeners`（默认 ✔）可整体关掉。

> 典型用法：关卡里放一个 `BP_TeleportFade` 实现该接口负责黑场；
> 以后加新的传送装置**不用逐个个接线**，全部自动生效。

### 8.3 `FTeleportTransitionContext` 字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `Traveler` | `AActor*` | 被传送的角色 |
| `Portal` | `UTimeEraPortalComponent*` | 本次传送的装置 |
| `Counterpart` | `AActor*` | 对应的另一半（可能为空） |
| `FromLocation` | `FVector` | 传送前位置 |
| `ToLocation` | `FVector` | 落点。**开始事件里是"预计落点"，结束事件里是实际落点** |
| `FromEra` / `ToEra` | `ETimeEra` | 源 / 目标时空 |
| `Duration` | `float` | 本次过场时长（= `TransitionDelay`） |

### 8.4 新参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `TransitionDelay` | 0.0 | 过场时长（秒）。> 0 时位移被推迟这么久；期间再按 E 会被拒绝 |
| `bDispatchTransitionToInterfaceListeners` | ✔ | 是否自动调用关卡里实现了 `ITeleportTransitionInterface` 的 Actor |

过场进行中 `IsTransitioning()` 为 true；组件 `EndPlay` 会清掉计时器，不会留下悬空回调。

### 8.5 接线示例（黑幕转场）

1. 关卡里放一个空 Actor，加一个全屏 UMG（或后处理），存成 `BP_TeleportFade`
2. Class Settings → **Interfaces → Add → Teleport Transition Interface**
3. `OnTeleportTransitionBegin`：把黑幕不透明度在 `Context.Duration` 内插值到 1
4. `OnTeleportTransitionEnd`：再插值回 0
5. 传送装置上把 `Transition Delay` 设成与淡出等长（例如 `0.5`）
6. Play → 按 E：黑幕渐入 → 0.5s 后角色出现在另一半 → 黑幕渐出

> 只想做**文字提示**（如"已传送至现代"）：不用黑幕，直接在 `OnTransitionEnd` 里
> 用 `Context.ToEra` 拼字符串推给 `WBP_InteractionPrompt` 即可。

---

## 9. 落点偏移的根因与修复（实测）

### 9.1 先证明"配对距离"本身是对的

实测关卡里 5 对装置的几何（`kongjianchuansuoqi1..10`，全部 `BlockAllDynamic`→见 9.3）：

| 对 | ΔX | ΔY | ΔZ | 水平偏差 |
|---|---|---|---|---|
| 1↔2、3↔4、5↔6、7↔8、9↔10 | **0.00** | **0.00** | **−5000.00** | **0.00 cm** |

另外：装置网格的世界坐标 = actor 原点（网格无相对偏移）；`TeleportOffset = (0,0,0)`；每对两个 actor 的 yaw 相同。
=> **`VerticalOffset = −5000` 完全正确，不需要重新标定传送距离。**

### 9.2 偏移来自"落地贴地"，不是传送距离

`bPlaceOnGround = true` 时落点会被一条地面射线**覆盖 Z**：

```
Start = 落点 + (0,0, GroundTraceDistance)     // +1000
End   = 落点 - (0,0, GroundTraceDistance)     // -1000
通道   = ECC_Visibility
忽略   = 自己 + 被传送角色          ← 原来的忽略表里【没有】目标装置
命中后 Goal.Z = 命中点.Z + 胶囊半高 + GroundClearance
```

装置网格 `BlockAllDynamic`、**挡 `ECC_Visibility`**，几何为 `localZ ∈ [0, 100]`（从 actor 原点往上一米）。
于是射线从"落点+1000"往下打时，**第一个命中的就是目标装置自己的顶面**：

```
Goal.Z = 装置原点 + 100（装置顶） + 96（胶囊半高） + 2（clearance） = 装置原点 + 198
```

而站在源装置处的相对高度是 `装置原点 + 98` ⇒ **每次传送都高整整 100 cm（正好一个装置的高度）**，
表现就是"被放到了装置顶上"而不是装置处 —— 这就是位置偏移的来源。

### 9.3 修复（两处，缺一不可）

| # | 改动 | 位置 |
|---|---|---|
| 1 | 地面射线**忽略所有带 `UTimeEraPortalComponent` 的 actor**（`VerticalOffset` 模式没有 Counterpart 可忽略，故按"所有传送装置"整体忽略）；且**只允许向下贴地**：`Goal.Z = min(命中点+半高+clearance, 落点+半高+clearance)`，避免打到天花板/上方物体时把角色抬高 | `TimeEraPortalComponent::ComputeArrivalLocation` |
| 2 | ~~装置网格改用 `IgnoreOnlyPawn`~~ **（已回退）**：见 9.4 —— 这个改动是"装置消失"的直接原因，现已改回 `BlockAllDynamic` | `/Game/bclass_source/modern_swift_actor` 的 `switcher` 网格（模板，10 个实例继承） |
| 3 | **落点水平避开装置**：沿装置自身 +X 推开「装置水平半径 + 胶囊半径 + `ArrivalClearance`」，然后才向下贴地 | `TimeEraPortalComponent::bArriveClearOfDevice` / `ArrivalClearance`（默认开 / 5cm） |

**现在的落点规则**：配对位置 →（+ `TeleportOffset`）→ **水平推开约 54 cm 站到装置旁边** → 向下贴地站到地面。

---

### 9.4 "运行时装置消失"的根因（★ 本轮新增，实测）

修复 #1 之后落点回到**装置原点**，但装置网格的真实几何是：

```
switcher 局部包围盒 = 21.4 × 21.5 × 100.0 cm      // 又细又高的柱子，原点在它底面
角色胶囊            = 半径 34、半高 96             // 胶囊比柱子还宽
```

把胶囊中心放在装置原点（脚底在柱底平面）时：胶囊横向半径 34 > 柱子半宽 10.7，
**整根柱子被吞进角色身体里** ⇒ 第一人称的摄像机也在胶囊范围内 ⇒ 看不到装置，
交互射线同样打不到它 —— 表现就是"**传送交互物消失了**"，而编辑器从外面看一切正常。
（改成 `IgnoreOnlyPawn` 之前，装置会挡住胶囊、把玩家物理挤开，所以那时没有"看不见"的问题，
只是被挤开的那一下就是最初报的"位置偏移"。）

⇒ **落点必须"在装置旁边"，不能"在装置里"**：沿装置自身 +X 推开
`装置水平外接半径(15.2) + 胶囊半径(34) + ArrivalClearance(5) ≈ 54 cm`，再向下贴地。
于是玩家站在装置侧面的地面上，装置清晰可见、交互照常。

| 参数 | 默认 | 说明 |
|---|---|---|
| `bArriveClearOfDevice` | ✔ | 关掉 = 回到"落在装置原点"（只在装置是空心的、例如真正的圆环拱门时才合适） |
| `ArrivalClearance` | 5 cm | 胶囊表面与装置之间的额外余量 |

> 方向固定取**装置自身的 +X**（不是世界 X），这样复制到任何朝向的装置上行为一致。
> `TeleportOffset` 仍叠加在它之前，手动微调能力不变（本项目当前 `(5,0,0)`，合计水平偏移约 59 cm）。

### 9.5 第二个独立原因：时空门控（★ 本轮修复）

关卡里**只有这 10 个装置**挂了 `TimeEraComponent`（5 Ancient + 5 Modern），而该组件在 BeginPlay 就会执行：

```
bActiveInCurrentEra = bExistsInBothEras || (Era == 子系统当前时空)
SetActorHiddenInGame(!bActive)      // bGateVisibility 默认开
SetActorEnableCollision(bActive)    // bGateCollision 默认开
```

`UTimeShiftSubsystem` 的 `InitialEra = Ancient`（配置里没有覆盖）⇒ **运行时开局，5 个 Modern 装置
（2 / 4 / 6 / 8 / 10）被隐藏且关碰撞**；编辑器视口不执行这段逻辑，所以 10 个全都看得见 ——
这正是"世界视图正常、运行时少了一半"的第二个原因。

**修复**：装置模板的 `TimeEra → bExistsInBothEras = ☑`（10 个实例已同步）。

- 语义上本来就该这样：**传送装置是两个时空之间的门，必须在两边都在场**；
- **配对方向不受影响**：Portal 读的是 `Era`（仍然奇 Ancient / 偶 Modern），`bExistsInBothEras` 只影响显隐与碰撞；
- 若以后希望某个装置只在单一时空出现，把这一项关掉即可。

---

## 10. 未验证 / 待确认

- 只在编译期验证 + 逻辑自检；**运行时行为尚未在编辑器里实测**（需要先接线，见第 3 节）。
- 过场接口（第 8 节）已完成**反射级验证**（UHT 产出、结构体 8 个字段、两个事件、接口类均可加载），
  但**位移推迟的实际时序、以及接口分发**尚未在 PIE 里实测。
- 第 9 节的落点修复是**按实测几何推导**的（射线忽略表 + 装置碰撞 profile），**未在 PIE 里实测**；
  复测时请确认：传送后脚底是否正好落在目标装置基座平面（应与源装置处相对高度一致）。
- 主控角色当前用的是 `firstvision` 关卡里的 `BP_FirstPersonCharacter`；它是否挂
  `TimeShiftTravelComponent` 未确认——若没挂，第 5 节第 2 步不会发生，落点仍由第 3 步决定，
  功能正常但不会顺带做 layout 映射。
