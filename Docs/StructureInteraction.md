# StructureInteraction —— 可交互建筑结构模块

关联代码目录：`Source/MHY_ARCH_GAME/StructureInteraction/`

本模块用于控制**可交互的建筑结构**（可抽象为此前的"方块"）。已按审核结论落地：
**两种拾取模式可切换 + 单 Actor 多块组件 + 枚举状态机**。

> 呈现部分（焦点描边、状态材质、音效/粒子）按要求在本文档中**标注为后续优化与功能追加的预留项**，
> 当前提供接口与基础实现，完整美术资产与表现可后续继续完善。

---

## 一、分层框架

```
┌─ 交互层 Interaction ────────────────────────────────────────────┐
│  IInteractableInterface            交互契约（5 个方法）           │
│  UInteractionDetectorComponent     挂玩家：拾取候选 / 焦点 / 转发  │
│  UInteractionPromptWidget          HUD 提示（预留，需做 WBP）     │
└───────────────▲───────────────────────────────────┬─────────────┘
                │ OnInteract / OnFocusBegin/End      │ OnFocusChanged
┌─ 结构层 Structure ─────────────────────────────────▼─────────────┐
│  AInteractiveStructure      主体，实现 IInteractableInterface      │
│   ├─ UStructureBlockComponent × N   单块：网格 / 状态变换 / 材质集 │
│   └─ UStructureVisualComponent      呈现协调（材质 / 描边）        │
│  状态机：EStructureState { Closed, Open, Locked, Disabled }        │
└──────────────────────────────────────────────────────────────────┘
```

---

## 二、类清单

| 类 | 基类 | 职责 |
|---|---|---|
| `IInteractableInterface` | `UINTERFACE` | `CanInteract / OnInteract / GetInteractionPrompt / OnFocusBegin / OnFocusEnd` |
| `UInteractionDetectorComponent` | `UActorComponent` | 挂玩家；球形或射线拾取、焦点切换、`TryInteract()`、`OnFocusChanged` 委托 |
| `UInteractionPromptWidget` | `UUserWidget` | HUD 提示（`SetPrompt` 为蓝图事件） |
| `UStructureBlockComponent` | `USceneComponent` | 单块；作为变换/状态锚点，网格取自其子 `StaticMeshComponent`；含 `ClosedTransform/OpenTransform`、材质集、碰撞规则 |
| `AInteractiveStructure` | `AActor` + `IInteractableInterface` | 结构主体；状态机、块插值、交互入口、事件委托 |
| `UStructureVisualComponent` | `UActorComponent` | 状态材质应用、焦点描边开关 |
| `EStructureState` / `EInteractionPickMode` | `UENUM` | 状态与拾取模式枚举 |

---

## 三、交互方案

- **拾取模式**（`UInteractionDetectorComponent.PickMode`，可切换）：
  - `SphereOverlap`：以玩家为中心半径 `InteractionRadius` 的球形重叠（沿用工程既有 `DoInteract` 风格）；
  - `LineTrace`：从玩家朝前 `TraceDistance` 的射线，适合精确瞄准某个方块。
- **候选筛选**：必须是实现了 `IInteractableInterface` 且 `CanInteract()` 为真；`bRequireFacing` 时忽略背后目标；
  评分 = 朝向点积优先、距离次之。
- **焦点**：候选变化时对旧对象 `OnFocusEnd`、新对象 `OnFocusBegin`（结构据此高亮），并广播 `OnFocusChanged` 给 HUD。
- **输入**：`InteractAction`（可指向工程已有 `IA_Interact`）设置后组件会尝试自绑定到玩家的 Enhanced Input；
  也可由角色自行绑定并调用 `TryInteract()`。
- **状态机**：`Closed ⇄ Open` 由交互切换；`Locked / Disabled` 拒绝交互（供其它系统设置）。
  切换时所有块在 `TransitionDuration` 内从 `ClosedTransform` 插值到 `OpenTransform`，并应用碰撞规则。

---

## 四、呈现方案（★ 预留与后续优化标注）

| 要素 | 当前状态 | 说明 |
|---|---|---|
| **状态材质切换** | ✅ 基础实现 | `UStructureVisualComponent::ApplyStateMaterials()` 按状态选择 `Normal/Active/Disabled` 材质（`HighlightMaterial` 供描边态使用） |
| **焦点描边高亮** | ◐ 基础实现，**预留完善** | 已实现 `SetRenderCustomDepth` + `SetCustomDepthStencilValue` 开关；**后处理描边材质（CustomStencil）需后续在编辑器中制作**，属预留项 |
| **音效反馈** | ◐ 基础实现，**预留扩展** | `AInteractiveStructure::InteractSound` 已在交互成功时播放；更丰富的音效分层为后续优化 |
| **粒子 / 相机反馈** | ★ **预留，未实现** | 作为后续功能追加：可挂 Niagara 系统与相机震动，需在 `.uproject` 启用 Niagara 并加模块依赖 |
| **HUD 交互提示** | ★ **预留，未实现** | `UInteractionPromptWidget` 仅提供 C++ 事件；需创建 WBP 并在关卡 HUD 中监听 `OnFocusChanged` |
| **动画过渡** | ✅ 基础实现 | 块的位置/旋转/缩放插值（`TransitionDuration`）；曲线缓动为后续优化 |

> 预留项说明：这些是**刻意留待后续优化与功能追加**的部分 —— 接口/属性已就位，接入美术资产即可生效，不影响当前交互与状态逻辑。

---

## 五、在引擎中搭建 Demo

1. **编译工程**，使 `StructureInteraction` 下的 C++ 类出现在内容浏览器（`C++ Classes/MHY_ARCH_GAME/`）。
2. **建结构蓝图**：右键 `AInteractiveStructure` → `Create Blueprint class` → 命名 `BP_InteractiveStructure`。
3. **加方块**：在蓝图 Components 面板 **Add → `Structure Block`**，添加若干块：
   - 每块下方再 **Add → `Static Mesh`**，并把它**拖到对应 `Structure Block` 之下**作为子组件
     （块本体是变换/状态锚点，子网格才是可见可碰撞的块体；未挂子网格的块会被静默跳过）；
   - 选中子网格 → 指定 `Static Mesh`（如 `SM_Cube`）、位置与缩放；
   - 选中该 `Structure Block` → 设置 `Closed Transform` / `Open Transform`（相对结构的开合位置）；
   - 设置材质集的 `Normal Material` / `Active Material`（可选 `Disabled Material`、`Highlight Material`）。
4. **配置结构**：在 `BP_InteractiveStructure` 的 Class Defaults 设：
   - `Initial State`（Closed）、`Transition Duration`（如 0.8）、`Interact Prompt`（如"开启"）；
   - 可选 `Interact Sound`。
5. **放玩家与探测器**：
   - 在你使用的玩家 Character（如 `BP_SideScrollingCharacter`）上 **Add → `Interaction Detector`**；
   - `Pick Mode` 选 `Sphere Overlap` 或 `Line Trace`，按需调 `Interaction Radius` / `Trace Distance` / `bRequire Facing`；
   - 把 `Interact Action` 设为工程已有 `IA_Interact`（自绑定）；若不想自绑定，则在角色的输入绑定里调用探测器的 `TryInteract()`。
6. **摆进关卡**：把 `BP_InteractiveStructure` 拖入关卡，放在玩家可接近处。
7. **运行验证**：
   - 靠近/瞄准结构 → 方块描边（若已做后处理）且（若已做 HUD）显示提示；
   - 按交互键 → 方块从 Closed 插值到 Open、碰撞按规则更新、材质切换；
   - 再按一次 → 回到 Closed。
8. **手动测试锁定态**：运行时用蓝图 `SetState(Locked)` 验证交互被拒绝、材质变为 Disabled。

---

## 六、与其它模块的关系

| 模块 | 关系 |
|---|---|
| `OverlapPassage` | 独立的"两物体重叠即可通行"几何机制；本模块负责**交互与状态**，两者可组合（例如结构开启后再由 OverlapPassage 处理块间重叠通行） |
| `LightReveal` | 结构块亦可实现 `IRevealableInterface`，实现"受光才可交互/显形" |
| 既有 `ISideScrollingInteractable` | 不修改、不冲突；新的 `IInteractableInterface` 是泛化版本，后续可选择让旧交互转调新探测器 |

---

## 七、已知限制与后续优化

- `UStructureBlockComponent` 为 `USceneComponent`（审核结论），网格取自其**子 `StaticMeshComponent`**；块的运动为运行时插值。
- 呈现的**后处理描边材质、粒子/相机反馈、HUD 提示**为预留项，需美术/HUD 侧接入。
- 结构状态为单枚举（本模块不处理复杂序列）；若后续需要多阶段行为，可迁移到项目已启用的 **StateTree**。
- `UStructureVisualComponent` 由 `AInteractiveStructure` 自动创建；不要在同一结构上重复添加。

---

## 八、相关文档

| 文档 | 内容 |
|---|---|
| `Docs/InteractionLogic.md` | 交互部分的**人机活动逻辑**（时序、焦点、评分、状态机、边界），以及第 13 节**解耦逻辑与设计原理详解**（依赖倒置 / 接口隔离 / 开闭原则 / 组合优于继承 / 耦合矩阵与权衡） |
| `Docs/OverlapDemo.md` | `OverlapPassage`（两物体重叠处可通行 + 变材质/透明）的操作与 demo 指南 |
| `Docs/LiquidLight.md` | 光照液体流动表现的实现说明 |
