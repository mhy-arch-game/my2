# InteractionLogic —— 交互部分的人机活动逻辑说明

本文档描述 `StructureInteraction` 模块中**交互功能的人机活动逻辑**：玩家能做什么、系统如何感知与判定、
双方的时序与状态如何流转、以及异常与边界如何处理。

对应代码：
- `Source/MHY_ARCH_GAME/StructureInteraction/InteractionDetectorComponent.{h,cpp}`
- `Source/MHY_ARCH_GAME/StructureInteraction/Interfaces/InteractableInterface.h`
- `Source/MHY_ARCH_GAME/StructureInteraction/InteractiveStructure.{h,cpp}`
- `Source/MHY_ARCH_GAME/StructureInteraction/StructureVisualComponent.{h,cpp}`

---

## 1. 参与者与职责

| 参与者 | 类型 | 职责 | 不负责 |
|---|---|---|---|
| **玩家（人类）** | 人 | 移动靠近/瞄准目标；按下交互键；从视觉/听觉反馈判断结果 | — |
| **交互探测器** | `UInteractionDetectorComponent`（挂玩家） | 周期性搜索候选、选出最佳、管理焦点、转发交互 | 不知道具体是门还是平台 |
| **可交互对象** | 实现 `IInteractableInterface`（如 `AInteractiveStructure`） | 自行判定可否交互、执行交互、给出提示文案、响应焦点 | 不知道玩家输入与候选选取规则 |
| **呈现层** | `UStructureVisualComponent` | 依据状态/焦点切换材质与描边 | 不参与逻辑判定 |
| **HUD（预留）** | `UInteractionPromptWidget` | 监听焦点变化显示提示文案 | 不参与逻辑判定 |

设计原则：**探测器只认接口，不认类型**；对象只认"被交互/被聚焦"，不认输入。双方通过 `IInteractableInterface` 解耦。

---

## 2. 输入与操作映射

| 玩家操作 | 输入 | 系统响应 |
|---|---|---|
| 移动/朝向目标 | Move / Look（工程既有 EnhancedInput） | 改变候选搜索结果 |
| 进入拾取范围 | — | 探测器在下一个轮询周期建立焦点 |
| 按下交互键 | `InteractAction`（可指向工程已有 `IA_Interact`），触发事件 `Started` | `TryInteract()` |
| 离开范围/移开视线 | — | 焦点解除 |
| （未配置 `InteractAction` 时） | 由角色自行绑定 | 需要角色显式调用 `TryInteract()` |

**输入绑定方式（两条路径，任选其一）**
1. **自绑定**：探测器设置了 `InteractAction` 后，会在 `BeginPlay` 尝试绑定到玩家的 Enhanced Input；
   若此时玩家的 InputComponent 尚未创建，则**每个 Tick 重试**，绑定成功置位 `bInputBound` 不再重试。
2. **手动**：不设 `InteractAction`，由 Character 的 `SetupPlayerInputComponent` 绑定并调用 `TryInteract()`。

---

## 3. 人机活动主循环（时序）

人类侧遵循 **感知 → 决策 → 操作 → 反馈**；系统侧在每个轮询周期完成"搜索 → 评分 → 焦点"。

```
玩家(人)                        探测器(系统)                      可交互对象(系统)
   │                               │                                 │
   │ 移动/朝向                      │                                 │
   │──────────────────────────────▶│ Tick 累加时间                     │
   │                               │ ≥ UpdateInterval → RefreshFocus  │
   │                               │ 1) 搜索候选(球形/射线)             │
   │                               │ 2) 逐个评分(接口+可交互+朝向+距离)  │
   │                               │ 3) 选出最佳 → SetFocusedActor     │
   │                               │──── old.OnFocusEnd() ───────────▶│ 关闭描边
   │                               │──── new.OnFocusBegin() ─────────▶│ 开启描边
   │                               │ OnFocusChanged(焦点, 提示文案)     │
   │ 看到描边/提示 ◀────────────────│ (HUD 监听委托)                    │
   │ 按下交互键                     │                                 │
   │──────────────────────────────▶│ TryInteract()                    │
   │                               │ 焦点存在? CanInteract?            │
   │                               │──── OnInteract(玩家) ───────────▶│ 状态切换/播放音效
   │                               │                                  │ 块插值过渡
   │                               │ 立即 RefreshFocus() 复核焦点       │
   │ 看到开合/材质/音效 ◀────────────┼──────────────────────────────────│
```

要点：
- **感知是轮询制**（默认 `UpdateInterval = 0.1s`），不是每帧，故焦点建立最多有约 0.1s 延迟。
- **交互是即时制**（按键当帧响应），但依赖"当前焦点"这一轮询结果。
- 交互后**立刻复核焦点**：若交互导致对象变为不可交互（如切到 `Locked`），焦点会在同一次调用内解除。

---

## 4. 焦点生命周期（人机共有的"当前目标"）

```
        无焦点
          │  搜索到合格候选
          ▼
      ┌────────┐   候选变化 / 变为不可交互 / 离开范围
      │ 有焦点 │ ─────────────────────────────────────▶ 无焦点
      └────────┘        (旧对象 OnFocusEnd → 广播)
          │
          │ 按下交互键且 CanInteract == true
          ▼
     执行 OnInteract（焦点通常保持不变）
```

| 事件 | 系统动作 | 人类可感知结果 |
|---|---|---|
| 建立焦点 | `new.OnFocusBegin(玩家)`；广播 `OnFocusChanged(new, prompt)` | 目标描边（若已做后处理）、HUD 显示提示（若已做） |
| 焦点切换 | 旧对象 `OnFocusEnd` → 新对象 `OnFocusBegin` → 广播 | 描边/提示转移到新目标 |
| 焦点解除 | 对象 `OnFocusEnd`；广播 `OnFocusChanged(null, 空文案)` | 描边与提示消失 |
| 焦点不变 | 不重复触发任何回调 | 无闪烁 |

> 由于"同对象不重复触发"，只要目标一直合格，焦点会**稳定保持**，不会每 0.1s 闪一次。

---

## 5. 候选判定与评分规则（系统侧"选择逻辑"）

**第一步：收集候选**
- 模式 `SphereOverlap`：以玩家位置为球心、`InteractionRadius`（默认 250）做球形重叠，
  只看 `ProbeObjectTypes`（默认 WorldStatic / WorldDynamic / Pawn）中的对象。
- 模式 `LineTrace`：从玩家位置沿**正前方** `TraceDistance`（默认 400）做 `Visibility` 射线，
  命中的 Actor 即候选（天然带遮挡语义，隔墙不会被选中）。

**第二步：过滤与评分**（对每个候选）
1. 排除自身；
2. 必须实现 `IInteractableInterface`，否则淘汰；
3. 必须 `CanInteract(玩家) == true`，否则淘汰；
4. 若 `bRequireFacing == true`，与玩家前向的点积 ≤ 0（在身后）则淘汰；
5. 评分：**`Score = 朝向点积 × 1000 − 距离`** → 朝向优先，距离次之。

**第三步：取最高分者为焦点**；全部淘汰则焦点为空。

> 两种模式的差异：球形模式**无遮挡判定**（可能选中隔墙目标），射线模式**有遮挡**但只找正前方第一个命中。
> 可按关卡需求切换。

---

## 6. 机器侧状态机（交互产生的世界变化）

以 `AInteractiveStructure` 为例，交互把人机活动落到结构状态上：

```
             OnInteract（玩家按键）
   ┌──────────┐  ─────────────────▶  ┌──────────┐
   │  Closed  │                      │   Open   │
   └──────────┘  ◀─────────────────  └──────────┘
             OnInteract（再次按键）

   Locked  /  Disabled  ── CanInteract() == false ──▶ 拒绝交互（无状态变化）
```

| 状态 | `CanInteract` | 交互结果 | 块碰撞 | 材质（呈现层） |
|---|---|---|---|---|
| `Closed` | 是 | 切到 `Open` | 实体 | `NormalMaterial` |
| `Open` | 是 | 切到 `Closed` | 若 `bDisableCollisionWhenOpen` 则关闭碰撞 | `ActiveMaterial` |
| `Locked` | 否 | 无变化被拒绝 | 不变 | `DisabledMaterial` |
| `Disabled` | 否 | 无变化被拒绝 | 不变 | `DisabledMaterial` |

**状态切换时的动作序列**（`SetState` → `ApplyState(false)`，随后广播）：
1. 对每个块应用碰撞规则 `ApplyCollisionForState(新状态)`；
2. 呈现层 `ApplyStateMaterials(新状态)`；
3. 置 `bTransitioning = true`，进入插值；
4. `SetState` 在 `ApplyState` 之后广播 `OnStateChanged(新状态)`。

> 注：`BeginPlay` 里的初始 `ApplyState(true)` 是**瞬时吸附**，不广播（避免开局误触发监听方）。

**插值活动**：每个 Tick 让每块从当前相对变换向目标变换混合，
步长 `Alpha = Clamp(DeltaTime / TransitionDuration, 0, 1)`；全部到位后**精确吸附并停止 Tick**，直至下次状态切换。

---

## 7. 反馈逻辑（人机闭环的"输出"）

| 反馈通道 | 触发时机 | 当前实现 | 状态 |
|---|---|---|---|
| 焦点描边 | `OnFocusBegin / OnFocusEnd` | `SetRenderCustomDepth` + `SetCustomDepthStencilValue` | 基础实现；**后处理描边材质为预留** |
| 状态材质 | 状态切换 | `ApplyStateMaterials()` 选 `Normal/Active/Disabled` | 已实现 |
| 运动反馈 | 状态切换 | 块位置/旋转/缩放在 `TransitionDuration` 内插值 | 已实现（缓动曲线为预留） |
| 听觉反馈 | 交互成功 | `InteractSound` 在世界位置播放 | 基础实现（分层音效为预留） |
| HUD 提示 | 焦点变化 | 广播 `OnFocusChanged(焦点, 文案)` | **UI 控件为预留**，需做 WBP 并监听委托 |
| 粒子/相机 | — | 未实现 | **预留** |

反馈缺失时的体验降级：即便未做描边/HUD，玩家仍可通过"开合运动 + 材质变化 + 音效"判断交互是否生效。

---

## 8. 一次完整活动走查（示例）

1. 玩家从远处走向结构 → 探测器每 0.1s 搜索，玩家在 250 单位外 → 无候选 → 无焦点。
2. 玩家进入范围且面向结构 → 该结构 `CanInteract == true`、点积 > 0 → 成为最佳候选
   → `OnFocusBegin` → 描边亮起（若已做）→ HUD 显示"开启"（若已做）。
3. 玩家按下交互键（`Started`）→ `TryInteract()` → 焦点存在且可交互 → `OnInteract(玩家)`
   → 状态 `Closed → Open` → 块在 0.8s 内插值移开、碰撞按规则更新、材质变 `ActiveMaterial`、播放音效。
4. `TryInteract` 结束前 **立即 `RefreshFocus()`**：结构仍可交互且仍在范围内 → 焦点保持不变 → 无重复 `OnFocusBegin`。
5. 玩家再按一次 → `Open → Closed`，块插值回位，碰撞恢复，材质回 `NormalMaterial`。
6. 玩家走开 → 下一次轮询无合格候选 → `OnFocusEnd` → 描边与提示消失。

---

## 9. 边界条件与异常处理

| 情形 | 系统行为 |
|---|---|
| 范围内有多个可交互对象 | 取评分最高者（先看朝向，再看距离）；不会同时聚焦多个 |
| 目标在身后且 `bRequireFacing = true` | 被淘汰，不建立焦点 |
| 目标被销毁 | `Cast` 失败被淘汰；下一轮焦点转移或清空 |
| 焦点对象变为 `Locked/Disabled` | `CanInteract` 为假 → 下一轮 `RefreshFocus` 淘汰 → 焦点解除 |
| 连续快速按键 | 每次按下各触发一次 `OnInteract`，状态来回切换；无防抖（如需要可加冷却） |
| 未配置 `InteractAction` 且角色未手动调用 | 有焦点但不会交互 → 需二选一配置输入 |
| 探测器所在 Actor 不是 Pawn | 自绑定失败（仅记录状态），仍可手动调用 `TryInteract()` |
| 球形模式隔墙选中 | 已知差异 → 改用 `LineTrace` 模式可避免 |
| 结构没有子 `StaticMeshComponent` | 块无法应用材质/碰撞/描边（静默跳过），不报错 |
| 玩家在过渡中反复交互 | 允许；插值从当前变换继续向新目标混合，不会跳变 |

---

## 10. 参数与调优

| 参数 | 所在 | 默认 | 人机体验影响 |
|---|---|---|---|
| `UpdateInterval` | 探测器 | 0.1s | 越小焦点建立越灵敏，开销略增 |
| `InteractionRadius` | 探测器（球形） | 250 | 可交互距离 |
| `TraceDistance` | 探测器（射线） | 400 | 可瞄准距离 |
| `bRequireFacing` | 探测器 | true | 关掉则背后目标也可被选中 |
| `PickMode` | 探测器 | SphereOverlap | 是否需要遮挡语义 |
| `ProbeObjectTypes` | 探测器 | WorldStatic/Dynamic/Pawn | 决定哪些对象参与候选 |
| `TransitionDuration` | 结构 | 0.8s | 开合动作时长 |
| `InteractPrompt` | 结构 | "Interact" | HUD 文案 |
| `bDisableCollisionWhenOpen` | 块 | true | 开启后是否移除碰撞（决定能否通行） |
| `InitialState` | 结构 | Closed | 初始是否可通行 |
| `HighlightStencilValue` | 块 | 1 | 描边 stencil；0 表示不描边 |

---

## 11. 与其它模块的交互边界

- **`OverlapPassage`**：处理"两个物体重叠处可通行"的几何问题；本交互模块只负责**何时/由谁触发**状态变化。二者可组合，但互不依赖。
- **`LightReveal`**：结构块也可实现 `IRevealableInterface`，形成"受光 → 可交互/显形"的前置条件；此时 `CanInteract` 可结合受光状态返回。
- **既有 `ISideScrollingInteractable`**：并存且不冲突。新 `IInteractableInterface` 是泛化版（多了焦点与提示）；后续可选择让旧交互转调新探测器。

---

## 12. 术语表

| 术语 | 含义 |
|---|---|
| 候选（Candidate） | 本帧搜索到、可能被交互的对象 |
| 焦点（Focus） | 当前被选中的唯一交互目标 |
| 评分（Score） | 决定多个候选中谁成为焦点的数值 |
| 合格（Qualify） | 实现接口 且 `CanInteract` 为真（且满足朝向要求） |
| 轮询周期 | `UpdateInterval`，焦点更新的最小时间粒度 |
| 交集通行 | `OverlapPassage` 的几何能力，与本模块的"状态通行"不同 |

---

## 13. 解耦逻辑与设计原理详解

本节拆解交互模块**"解耦"到底解开了什么、靠什么机制实现、依据哪些设计原理、以及代价是什么**。

### 13.1 先看不解耦会怎样（问题陈述）

若不设计接口，最直接的写法是让探测器认识所有可交互类型：

```
// ❌ 高耦合写法（本模块刻意避免）
if (ADoor* Door = Cast<ADoor>(Best))            { Door->Open(); }
else if (AMovingPlatform* P = Cast<AMovingPlatform>(Best)) { P->Toggle(); }
else if (ANPC* Npc = Cast<ANPC>(Best))          { Npc->Talk(); }
else if (AInteractiveStructure* S = Cast<AInteractiveStructure>(Best)) { S->ToggleState(); }
...
```

由此产生四个连锁问题：

| 问题 | 后果 |
|---|---|
| **类型泄漏** | 探测器必须 include 每一个具体类；新增一种可交互物就要**修改探测器** |
| **无法复用** | 探测器与某玩法变体强绑，跨变体（SideScrolling / Combat / Platforming）无法共用 |
| **职责混淆** | 判定、执行、呈现、输入混在一处，改一处易伤其它 |
| **无法独立演进** | HUD、输入、对象行为互相牵制，测试与替换困难 |

**解耦的目标**就是把上面这张"探测器 → 具体类型"的星形依赖，改成"探测器 → 抽象 ← 具体类型"的依赖倒置结构。

### 13.2 解耦的三个切口（基本逻辑）

解耦不是一件事，而是沿三个维度分别切开：

```
切口一：类型解耦（谁认识谁）
   ▸ 探测器只认识 IInteractableInterface，运行时用 Cast 解析实现者
   ▸ 对象只认识"我被交互了/我被聚焦了"，不认识玩家输入

切口二：时序解耦（谁先谁后）
   ▸ 感知侧：轮询（UpdateInterval）——对象不需要主动通知探测器"我来了"
   ▸ 通知侧：委托广播（OnFocusChanged）——系统不直接驱动 HUD

切口三：职责解耦（谁干什么）
   ▸ 探测 = 找候选 | 判定 = 对象自答 CanInteract | 执行 = 对象自答 OnInteract
   ▸ 呈现 = UStructureVisualComponent | 文案 = 对象提供，HUD 只负责显示
```

三者的依赖方向都指向**抽象**：

```
   UInteractionDetectorComponent ─────┐
                                      ▼
                          IInteractableInterface          ← 抽象（契约）
                                      ▲
                    ┌─────────────────┼─────────────────┐
        AInteractiveStructure   （未来）ALever    （未来）ADoor ...
                    │
                    ▼ 仅结构内部
        UStructureVisualComponent / UStructureBlockComponent
```

### 13.3 依赖倒置的落地机制

| 环节 | 机制 | 代码位置 |
|---|---|---|
| 定义抽象 | `UINTERFACE(MinimalAPI, NotBlueprintable)` + 纯虚类 | `Interfaces/InteractableInterface.h` |
| 调用方依赖抽象 | `Cast<IInteractableInterface>(Actor)` 后调虚函数 | `InteractionDetectorComponent.cpp` → `ScoreCandidate` / `TryInteract` |
| 实现方依赖抽象 | `class AInteractiveStructure : public AActor, public IInteractableInterface` 并 `override` | `InteractiveStructure.h` |
| 双向零类型依赖 | 探测器不 include 任何具体类；对象不 include 探测器 | 两个 .cpp 的 include 列表 |

关键点：**探测器唯一的"上游知识"是"存在这么一个接口"**，这是契约依赖（不可避免且必要）；
除此之外它对该对象是门、平台还是建筑一无所知。

### 13.4 契约本身的设计：五个方法各归其位

接口不是"把方法堆在一起"，而是按**谁拥有信息**来划分职责：

| 方法 | 由谁回答 | 为什么放在对象侧 |
|---|---|---|
| `CanInteract(Interactor)` | 对象 | 只有对象知道自己是 Locked、冷却中、还是缺前置条件 |
| `OnInteract(Interactor)` | 对象 | 只有对象知道交互要做什么（开合/旋转/播放） |
| `GetInteractionPrompt()` | 对象 | 文案属于对象自身语义与本地化，探测器不应硬编码 |
| `OnFocusBegin(Interactor)` | 对象（**有默认空实现**） | 焦点是通用概念，但"被聚焦时做什么"因对象而异；给默认实现使**只关心交互的对象不必实现它** |
| `OnFocusEnd(Interactor)` | 对象（**有默认空实现**） | 同上，保证焦点生命周期成对且不强制 |

> 这一划分同时体现了 **接口隔离（ISP）**：`OnFocus*` 作为可选能力提供默认实现，
> 实现者只需实现它真正关心的 3 个方法。

### 13.5 扩展点：新增一种可交互物要动什么

```
新增 ALever（拉杆）：
  class ALever : public AActor, public IInteractableInterface { ... }
  → 实现 3 个纯虚方法（可选择性重写 OnFocus*）
  → 放入关卡

需要修改的既有代码：无（探测器、HUD、结构、呈现组件都不动）
```

这就是 **开闭原则（OCP）**：对扩展开放（加新类型），对修改封闭（不改探测器）。

### 13.6 各项设计原理与代码证据对照

| 设计原理 | 在本模块的体现 | 代码证据 |
|---|---|---|
| **单一职责 SRP** | 探测 / 判定 / 执行 / 呈现 / 文案 各自独立成类 | `InteractionDetectorComponent`、`InteractiveStructure`、`StructureVisualComponent`、`StructureBlockComponent` |
| **开闭原则 OCP** | 加新可交互物无需改探测器 | 探测器仅依赖接口 |
| **里氏替换 LSP** | 任何接口实现都能被探测器同等对待 | 评分/交互逻辑只调接口方法，无类型分支 |
| **接口隔离 ISP** | 焦点钩子有默认空实现，实现者只实现关心的部分 | `InteractableInterface.h` 中 `OnFocusBegin/End` 带默认体 |
| **依赖倒置 DIP** | 高层（探测器）与低层（结构）都依赖抽象 | `Interfaces/InteractableInterface.h` |
| **组合优于继承** | 结构通过"持有块组件 + 呈现组件"组合能力，而非继承层级堆叠 | `AInteractiveStructure` 聚合 `UStructureBlockComponent` / `UStructureVisualComponent` |
| **事件驱动 / 观察者** | 焦点变化用动态多播委托广播，订阅方自决 | `FOnInteractionFocusChanged OnFocusChanged` |
| **分层架构** | 交互层 / 结构层 / 呈现层单向依赖 | 见 `Docs/StructureInteraction.md` 第一节 |
| **数据与行为分离** | 块只存数据（变换/材质），行为由结构与呈现层驱动 | `StructureBlockComponent` 无交互逻辑 |
| **失败安全（Fail-safe）** | 未挂子网格的块静默跳过、未配置输入时不报错，只降级 | `StructureBlockComponent::ResolveMesh`、探测器 `TryBindInput` |
| **可选的耦合强度** | 输入支持"自绑定"或"手动调用"两条路径，按需选择耦合级别 | `InteractAction` + `TryInteract()` |

### 13.7 耦合矩阵：解耦前后对比

| 依赖方向 | 解耦前 | 解耦后 |
|---|---|---|
| 探测器 → 具体对象类型 | N 个（每加一类 +1） | **0**（只依赖接口） |
| 具体对象 → 探测器 | 可能（回调/取输入） | **0** |
| 系统逻辑 → HUD | 直接调用 | **0**（经由委托，HUD 订阅） |
| 结构逻辑 → 表现细节 | 混写 | 仅调用 `UStructureVisualComponent` 的抽象动作 |
| 交互模块 → OverlapPassage | — | **0**（互不依赖，可组合） |

### 13.8 仍然存在的耦合（有意的权衡）

解耦不是"零耦合"，而是**把耦合放在正确的位置**。以下耦合是刻意保留的：

| 保留的耦合 | 原因 | 代价 / 权衡 |
|---|---|---|
| 探测器 ↔ `IInteractableInterface` 契约 | 没有契约就没有交互，这是必要依赖 | 接口一旦改动，所有实现方需同步（故接口保持最小：5 个方法） |
| 结构 ↔ 其内部块/呈现组件 | 同一结构内**高内聚**优于拆散 | 结构内部改动需关注三者协作 |
| 接口为 `NotBlueprintable`（C++ 实现） | 简化实现、避免蓝图接口的性能与复杂度 | 纯蓝图对象**暂不能**实现该接口；未来若需要，可改为 `BlueprintNativeEvent` 并配合 `Execute_` 调用（属可预期扩展） |
| 探测器为**轮询**而非事件 | 避免要求每个对象主动注册/注销，降低对象侧负担 | 焦点建立最多延迟一个 `UpdateInterval`（0.1s） |
| 输入可"自绑定"到探测器 | 换取开箱即用 | 若玩家 InputComponent 创建较晚，需靠 Tick 重试；可用手动绑定路径规避 |

### 13.9 解耦带来的实际收益

1. **可替换**：任何实现接口的 Actor 都能被同一个探测器交互，可写测试桩（如"永远可交互的假对象"）验证探测器逻辑。
2. **可复用**：探测器可挂在 SideScrolling / Combat / Platforming 任一变体的角色上，无需改动对象。
3. **可独立演进**：HUD 未完成时交互逻辑照常工作（只是没有提示）；后处理描边未完成时交互与状态机照常工作。
4. **可并行开发**：交互、结构、呈现、HUD 可由不同人并行推进，只需遵守接口契约。
5. **可组合**：`OverlapPassage`（重叠可通行）与 `LightReveal`（受光解锁）都能与结构组合，而不产生相互依赖。

### 13.10 需要避免的反模式（本模块的"红线"）

| 反模式 | 为什么禁止 |
|---|---|
| 在探测器里 `switch/Cast` 具体类型 | 直接破坏 OCP，类型泄漏回来 |
| 让对象直接操作 HUD 控件 | 逻辑层被 UI 生命周期拖累，且无法在无 UI 环境测试 |
| 让块反向引用结构/呈现组件 | 形成环状依赖，块无法独立复用 |
| 把"可否交互"的判断写进呈现层 | 呈现层变成逻辑判据，双份真相（single source of truth 被破坏） |
| 让 `OverlapPassage` 依赖结构模块 | 两个机制应可独立使用，组合而非继承 |

### 13.11 一句话总结

> **解耦的基本逻辑** = 把"探测器认识所有对象"倒置为"探测器和对象都只认识一个最小契约"，
> 并沿**类型、时序、职责**三个切口同时切开；
> **对应的设计原理** = 依赖倒置（DIP）+ 接口隔离（ISP）+ 开闭原则（OCP）+ 单一职责（SRP）
> + 组合优于继承 + 事件驱动；
> **代价** = 契约的稳定性要求、轮询带来的轻微延迟、以及清晰但真实存在的内部高内聚耦合。

