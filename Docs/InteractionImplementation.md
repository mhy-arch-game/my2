# 交互实现说明 —— 角色端 / 门端 / 灯端

> 工程：**MHY_PROJ_v0_1** ｜ C++ 模块：**MHY_ARCH_GAME** ｜ 引擎：UE 5.7.4
> 代码位置：`Source/MHY_ARCH_GAME/StructureInteraction/`
> 本文只讲**这三端怎么实现、怎么搭、怎么排错**。

---

## 0. 总览：一套框架，三种角色

```
                    ┌─────────────────── 角色端（玩家侧）───────────────────┐
  键盘 E ──▶ IMC ──▶│ IA_Interact ──▶ UInteractionDetectorComponent           │
                    │   ├─ 周期性搜索候选（球形 / 射线）                     │
                    │   ├─ 评分选焦点（对准优先，距离次之）                   │
                    │   ├─ 焦点描边加粗                                      │
                    │   └─ TryInteract() ──┐                                 │
                    └───────────────────────┼─────────────────────────────────┘
                                            │
              IInteractableInterface（接口） │ 或 UInteractableComponent（组件）
                                            ▼
                    ┌────────────── 物体侧 ──────────────┐
                    │ 门端：Interactable + 内建开关        │ → 变换插值开合
                    │ 灯端：Interactable + 灯光开关        │ → SetVisibility/Intensity
                    └─────────────────────────────────────┘
```

**核心设计**：探测器**只认接口或组件**，不认识任何具体类型。门和灯用的是**同一个组件类**，只是勾选了不同的可选功能 —— 这就是"不同物体不同交互逻辑"。

| 端 | 用什么 | 关键开关 |
|---|---|---|
| **角色端** | `UInteractionDetectorComponent`（挂 Pawn） | `Interact Action` |
| **门端** | `UInteractableComponent`（挂门 Actor） | `bUse Built-in Toggle` |
| **灯端** | 同一个 `UInteractableComponent` | `bToggle Lights` |

---

## 1. 角色端（玩家侧）

### 1.1 挂什么

在角色蓝图（本工程实际运行的是 `BP_FirstPersonCharacter`）上：

**Components → + Add → `Interaction Detector`**（`UInteractionDetectorComponent`）

### 1.2 参数全表

**拾取（Category `Interaction`）**

| 参数 | 默认 | 说明 |
|---|---|---|
| `Pick Mode` | `Sphere Overlap` | `Sphere Overlap` = **靠近**即可；`Line Trace` = 需要**对准** |
| `Interaction Radius` | 250 | 球形模式半径（cm） |
| `Trace Distance` | 400 | 射线模式距离（cm） |
| `bRequire Facing` | ✔ | 是否要求"朝向大致对准" |
| `Min Facing Cosine` | 0.0 | 对准严格度：`0` = 前方半球，`0.5` ≈ 60°，`0.9` ≈ 25° |
| `Update Interval` | 0.1 | 搜索间隔（秒），焦点最多延迟这么久 |
| `Probe Object Types` | WorldStatic / WorldDynamic / Pawn | 球形模式考虑的对象类型 |
| `bDraw Debug` | ✗ | 画调试线（排错期建议开） |

> **瞄准用的是相机视点**（`AController::GetPlayerViewPoint`），不是角色胶囊的 forward —— 胶囊 forward 永远不俯仰，抬头低头都瞄不准。

**输入（Category `Interaction|Input`）**

| 参数 | 默认 | 说明 |
|---|---|---|
| `Interact Action` | 空 | **必须设为 `IA_Interact`**，否则不会自绑定 |
| `bRegister Interact Context` | ✔ | 探测器**自己注册一个运行时输入上下文**，把 `Interact Key` 映射到 `Interact Action` —— 即使 Content 侧没有映射也能按 E 生效 |
| `Interact Key` | **E** | 上面那个上下文用的键 |
| `Interact Context Priority` | 0 | 上下文优先级 |

**焦点描边（Category `Interaction|Outline`）**

| 参数 | 默认 | 说明 |
|---|---|---|
| `bApply Focus Outline` | ✔ | 总开关 |
| `bUse Stencil Outline` | ✔ | 写 CustomDepth / 模板值 |
| `Focused Outline Stencil` | 2 | 聚焦档（"粗"），供描边材质分支 |
| `bOutline Visible Primitives Only` | ✔ | 跳过隐藏碰撞代理，避免多余描边 |
| `Outline Parameter Collection` | 空 | 可选 MPC，直接把厚度推给材质 |
| `Outline Thickness Parameter` | `OutlineThickness` | MPC 标量名 |
| `Focused / Resting Outline Thickness` | 4.0 / 1.5 | 聚焦 / 静息厚度 |

### 1.3 输入的两条路径

**路径 A（推荐）：`Interact Action` 自绑定**

1. `Interact Action` = `IA_Interact`
2. `bRegister Interact Context` 保持 ✔
3. 运行时探测器在 `BeginPlay`（并每帧重试）做两件事：
   - 把 `IA_Interact` 的 `Started` 事件绑到自己的 `HandleInteractInput()`
   - 用自己的 `Interact Key` 建一个运行时 `UInputMappingContext` 注册进本地玩家子系统
   - 成功时打日志：`[Interaction] <角色> registered runtime interact context (E -> IA_Interact)`

   这正是为了根治"映射存在、但加在了关卡根本不用的 PlayerController 上"这个坑（本工程踩过）。
   等你自己的 IMC / PlayerController 正常映射后，把 `bRegister Interact Context` 关掉即可，不会重复。

**路径 B：自己绑**

把 `Interact Action` 留空，在角色的 `SetupPlayerInputComponent` 里绑定按键并调用探测器的 **`TryInteract()`**（BlueprintCallable）。

### 1.4 运行时行为

- **聚焦**：搜索候选 → 过滤（必须是接口或组件、`CanInteract` 为真、满足朝向）→ 评分 `朝向点积 × 1000 − 距离` → 最高分者为焦点
- **进入焦点**：调用目标的 `OnFocusBegin`，并给它的**可见图元**开 CustomDepth + 模板值 = 2（加粗）
- **离开焦点**：**逐图元还原**到聚焦前的状态（含原模板值），再调用目标的 `OnFocusEnd`
  > 顺序是刻意设计的：**先还原描边、再让物体执行自己的 `OnFocusEnd`**，否则会和物体自带高亮互相留下"残留描边"。
- **按交互键**：`TryInteract()` → `CanInteract` → `OnInteract` → 立刻重新搜一次焦点
- **焦点保持（让"再按一次"可靠的关键）**：搜索候选时，只要**当前焦点仍是合法目标**
  （可交互 + 在范围内 + 满足朝向），即使拾取射线/球形**没有再命中它**，也会把它作为候选保留。
  > 没有这一条，门一转动就会离开准星 → 立刻丢焦点 → **第二次按键永远关不上**，
  > 表现就是"只能开、不能关"。本条已默认生效，无需配置。

### 1.5 可调用的 API

| 接口 | 用途 |
|---|---|
| `TryInteract()` | 主动触发交互（手动绑定时用） |
| `GetFocusedActor()` | 当前焦点 Actor |
| `GetCurrentPrompt()` | 当前提示文案（HUD 用） |
| `IsInteractableTarget(Actor)` | 某 Actor 是否算交互目标 |
| `OnFocusChanged(FocusActor, Prompt)` | 委托，焦点变化时广播（接 HUD） |

### 1.6 HUD 接线（可选）

探测器 `OnFocusChanged` → `Create Widget`(`WBP_InteractionPrompt`) → `SetPrompt(Prompt)`；`FocusActor` 为空时移除 Widget。

---

## 2. 门端

### 2.1 挂什么

在门的蓝图（本工程是 `Content/bclass_source/active_door`）上：

**Components → + Add → `Interactable`**（`UInteractableComponent`）

### 2.2 基础参数（Category `Interaction`）

| 参数 | 说明 |
|---|---|
| `bEnabled` | 关掉则不能被交互 |
| `Interaction Prompt` | HUD 文案，例如 `开门` |

### 2.3 内建开关（Category `Interaction|Built-in Toggle`）

| 参数 | 默认 | 说明 |
|---|---|---|
| `bUse Built-in Toggle` | ✗ | **门的核心开关**，勾上 |
| `Toggle Components` | 空 | 要驱动的组件（对象引用）—— 见 2.5，通常选不到 |
| **`Toggle Component Names`** | 空 | **按组件名指定（推荐）**，填 Components 面板里的确切名字 |
| `Closed Relative Transform` | 单位 | 关门姿态（全 0） |
| `Open Relative Transform` | 单位 | 开门姿态，例如 **Rotation Z = 90** |
| `Toggle Duration` | 0.6 | 过渡秒数（0 = 瞬开） |
| `bStart Open` | ✗ | 初始是否开着 |
| `bDisable Collision When Open` | ✔ | 开门后关碰撞（可穿过） |

### 2.4 运行时行为

- 交互 → `SetOpen(!IsOpen())`
- 开门：把 `ToggleComponents` 从 Closed 插值到 Open（`Toggle Duration` 内，Tick 驱动）
- **碰撞跟随"逻辑状态"而不是插值过程**：开门瞬间就关碰撞 —— 避免门转到一半把人卡住
- 关门：插值回 Closed，并把**原始碰撞状态精确还原**（`BeginPlay` 时缓存过，不是硬编码写回）
- 广播 `OnToggle Changed(Interactable, bIsOpen)`
- **再按一次 = 回到初始形态**：状态就是 `bIsOpen`，每次交互取反，没有单独的"关门"逻辑

### 2.5 ⚠️ 为什么用"名字"而不是对象引用

蓝图 **Class Defaults 里对象属性的下拉框只列资产（Content Browser 的 .uasset）**，而门板是同一个蓝图内部的**组件（subobject）**，不是资产 —— 所以 `Toggle Components` 经常点不出来。

三种可行办法（按省事排序）：

1. **`Toggle Component Names`**（推荐）：点 `+` 直接手打组件名，如 `DoorMesh`
2. **Construction Script 里 Set**：把组件从 Components 面板拖进图表 → `Make Array` → 连到 `Interactable` 的 `Set Toggle Components`
3. 碰运气：有些版本下拉框会列出本蓝图组件，能选就直接选

> 解析规则：**优先用对象引用；引用为空时按名字匹配**（在 `BeginPlay` 解析）。
> 解析不到会打 Warning：`[Interaction] <Actor>: bUseBuiltInToggle is on but no Toggle Components resolved...`

---

### 2.6 ★ 如何调整旋转轴（门轴 / 原点）

> **两种做法，推荐先用第 1 种（纯参数，不动层级）。**
>
> 注意原理：组件插值时旋转永远绕**被驱动组件自己的原点**。所以"门轴在哪"本质上等于
> "被驱动组件的原点在哪" —— 方法 1 用**数学等效变换**绕开这个限制，方法 2/3 则是**真的移动原点**。
>
> ⚠️ 下文还有一条**重要前提**：在 UObject 构造函数里设置 Box 尺寸必须用 `InitBoxExtent`
> （见 `Docs/GravityZone.md` §6），否则编辑器直接 Fatal。

#### ★ 方法 1（推荐）：代码级"轴承旋转"—— 直接填轴 + 枢轴，不改层级

内建开关新增了一组参数，**绕任意轴、穿过任意枢轴点旋转**，不需要加 Hinge 组件、不需要改网格 pivot：

| 参数 | 默认 | 说明 |
|---|---|---|
| `bUse Axis Rotation` | ✗ | 打开后**忽略** `Open Relative Transform`，改用下三个参数算开合姿态 |
| `Rotation Axis` | (0,0,1) | 旋转轴，**组件本地空间**。竖直门轴用 Z |
| `Open Angle Degrees` | 90 | 带符号角度。反方向就填 `-90` |
| `Rotation Pivot` | (0,0,0) | **轴穿过的点，组件本地空间** —— 这就是门轴。例如门板原点在中心、宽 100，填 `(-50,0,0)` 就绕 -X 那条边转 |

内部就是一次标准等效变换：

```
Open = ClosedRelativeTransform * ( T(P) * R(Axis, Angle) * T(-P) )
```

即"把关门姿态绕穿过 P 的轴转 Angle"。填 `(0,0,0)` 就退化成绕组件自身原点转。

> 好处：轴与枢轴都是**可调参数**，试错成本极低；也不用动组件层级和网格资产。

#### 方法 2：手动加"门轴"组件（不动资产的层级方案）

**先理解一件事**：`UInteractableComponent` 只做一件事 —— 把 `Toggle Components` 里的组件，
在 `Closed Relative Transform` 与 `Open Relative Transform` 之间插值。
**它不关心你想绕哪根轴转**，旋转中心就是那个组件自身的原点（pivot）。
所以"门轴在哪"等价于"被驱动组件的原点在哪"。

#### ✅ 推荐做法：加一个"门轴"组件，把网格挂到它下面（不改任何资产）

关键点：`Toggle Components` 接受**任意 `USceneComponent`**，不一定是网格。

1. 打开门蓝图 → **+ Add → `Scene Component`**，重命名为 **`Hinge`**
2. 把 `Hinge` **拖到组件树最顶上，成为 Root**（拖到 `DefaultSceneRoot` 上方把它替换掉）
3. 把门板 `Static Mesh Component` **拖到 `Hinge` 之下**，成为子组件
4. 把 `Hinge` 摆到**门轴的实际位置**（门框合页那条边）
5. 调整门板网格的 `Relative Location`，让它看起来位置正确
   —— 此时门板相对 `Hinge` 会有一个偏移，这正表示"门轴在 `Hinge` 的原点"
6. 在 `Interactable` 的 **`Toggle Component Names`** 里填 **`Hinge`**（**不是**门板网格）
7. `Open Relative Transform` = **Rotation Z = 90**

> 为什么建议把 `Hinge` 做成 Root：Actor 的位置 = Root 的位置，关卡里摆放时看到的就是门轴位置，符合直觉。

#### 备选 1：改网格自身的 pivot（会写进资产）

1. 双击门板网格 → 打开 **Static Mesh Editor**
2. 工具栏 **`Pivot` 下拉 → `Edit Pivot`**（或 `Set Pivot Offset`）
3. 把 pivot 移到门轴边缘 → **Apply** → 保存
4. 回蓝图，`Toggle Component Names` 填门板网格名

⚠️ 这会**修改共享资产** —— 所有引用这块网格的地方都会受影响。只有该网格专用于这扇门时才合适。

#### 备选 2：不动层级，直接把等价变换算出来

把"绕门轴转 90°"折算成"绕网格自身原点旋转 + 平移"，把 Location 与 Rotation 一起填进
`Open Relative Transform`。能用，但难维护难调，不推荐。

#### ❌ 常见误解：Actor 的 `Pivot Offset` 对运行时无效

关卡里选中 Actor 时 Details 中的 **`Pivot Offset` 只影响编辑器 gizmo 的显示**，
**不影响运行时旋转**。在那里改不会让门绕轴转 —— 别在这上面浪费时间。

#### 关于轴的选择与取值

- `Open Relative Transform` 的旋转是**相对**于该组件的父级
- 竖直门轴 → 用 **Z（Yaw）**，例如 `(0, 0, 90)`
- 翻板 / 翻盖（绕水平轴翻）→ 用 **Y（Pitch）** 或 **X（Roll）**
- 两个 Transform 都是**绝对相对姿态**，不是增量：
  `Closed Relative Transform` 填关门时该组件相对父级的姿态（通常全 0），
  `Open Relative Transform` 填开门时的姿态

#### 自查：门转错地方怎么判断

| 现象 | 原因 |
|---|---|
| 绕门板**中心**转 | 被驱动组件的原点在门板中心 → 用推荐做法加 `Hinge` |
| 整体**平移**而不是转 | `Open Relative Transform` 的 Location 填错了（应保持 0，只改 Rotation） |
| 转的方向相反 | Z 填 `-90` |
| 门**飞走 / 位置乱跳** | `Closed Relative Transform` 没填成关门姿态（通常应为全 0），插值起点错了 |

---

## 3. 灯端

灯**复用同一个 `UInteractableComponent`**，共用**同一个状态** `IsOpen()`，约定：**灯亮 = `IsOpen()` 为 true**。

### 3.1 参数（Category `Interaction|Light Switch`）

| 参数 | 默认 | 说明 |
|---|---|---|
| `bToggle Lights` | ✗ | **灯的核心开关**，勾上 |
| `Light Components` | 空 | 要开关的灯（对象引用），同样可能选不到 |
| **`Light Component Names`** | 空 | **按组件名指定（推荐）**，如 `PointLight` |
| `Are Lights On` | — | 只读查询：当前是否亮着 |

### 3.2 运行时行为

- 交互 → `SetOpen(!IsOpen())` → `ApplyLightState()`
- 亮：`Light->SetVisibility(true)` + `SetIntensity(原始亮度)`
- 灭：`Light->SetVisibility(false)` + `SetIntensity(0)`
- **原始亮度在 `BeginPlay` 缓存**，切回来精确还原 —— 不会把美术调好的亮度弄丢
- 灯是**瞬时**切换，**不参与** `Toggle Duration` 的插值（那是门的）
- 解析不到灯组件同样会打 Warning

### 3.3 初始状态怎么定

`bStart Open` = 初始状态是否为"开"：

| 想要的效果 | `bStart Open` |
|---|---|
| **按 E 关灯**（灯一开始亮） | ✔ |
| **按 E 开灯**（灯一开始灭） | ✗ |

---

## 4. 门 + 灯 组合

同一个 `Interactable` 组件可以**同时**驱动门板和灯 —— 两个开关都勾上，一次交互既开门又点亮门内的灯，共享同一个 `IsOpen()` 状态。

---

## 5. 物体侧的三种实现路径（怎么选）

| 路径 | 怎么做 | 适用于 | 代价 |
|---|---|---|---|
| **A. 内建开关**（门/灯就是它） | 挂 `Interactable` + 勾开关 + 填属性 | 最常见的"开/关"类物体 | 功能限于变换 / 灯 |
| **B. 挂组件 + 蓝图事件** | 挂 `Interactable` → 事件图表接 **On Interact Requested** | 需要播动画 / Timeline / Niagara / 音效 | 要画节点 |
| **C. 实现接口** | Class Settings → Interfaces → 加 `Interactable` → 实现 `On Interact` | 一类对象共享同一套逻辑 | 每个类都要实现一次 |

> `IInteractableInterface` 是 **`Blueprintable`** 的，**任何蓝图都能实现**，不需要写 C++。
> 五个方法：`CanInteract` / `OnInteract` / `GetInteractionPrompt` / `OnFocusBegin` / `OnFocusEnd`。

`UInteractableComponent` 上可绑的事件：

| 事件 | 参数 | 时机 |
|---|---|---|
| `On Interact Requested` | Interactor, Interactable | 按下交互键时（内建开关之后也广播） |
| `On Focus Gained` | Interactor, Interactable | 成为焦点 |
| `On Focus Lost` | Interactor, Interactable | 失去焦点 |
| `On Toggle Changed` | Interactable, bIsOpen | 内建开关状态变化 |

---

## 6. 排查清单（按顺序，别跳）

**按 E 完全没反应**

1. 角色上有没有 `Interaction Detector`？`Interact Action` 设成 `IA_Interact` 了吗？
2. 看 Output Log 有没有 `registered runtime interact context (E -> IA_Interact)`
   - 没有 → `Interact Action` 是空的，或 `bRegister Interact Context` 被关了，且 Content 里的 IMC 也没映射它
3. `bDraw Debug` 打开，Play 时准星对准门，有没有绿线？
   - **没绿线** = 没聚焦到 → 调 `Pick Mode` / `Trace Distance` / `Min Facing Cosine` / `Probe Object Types`；或确认门的网格**阻挡 Visibility 通道**
   - **有绿线但按 E 无事** → 往下看

**有绿线，按 E 但门不动**

4. Output Log 有没有 `no Toggle Components resolved`？
   - 有 → `Toggle Component Names` 里填的名字和门板组件**实际名字不一致（大小写敏感）**
   - 没有 → 确认 `bUse Built-in Toggle` 勾了、`Open Relative Transform` 真的不同于 Closed
5. 门板网格必须是**该 Actor 的组件**（不是场景里另一个独立 Actor）

**灯不亮 / 不灭**

6. `bToggle Lights` 勾了吗？`Light Component Names` 名字对吗？
7. 确认该 Actor 上真的有灯组件（`Point Light` / `Spot Light` / `Rect Light`）
8. 若灯自己有每帧设亮度的蓝图逻辑，会覆盖这里的设置

**线上诊断工具**：`Scripts/diagnose_interaction.py` 是可重跑的只读诊断，会把控制器 / 角色探测器 / 门的真实配置导出到 `Scripts/diag_result.json`。排查"配置到底生效没有"时先跑它，别靠猜。

---

## 7. 已知限制

| 项 | 说明 |
|---|---|
| `Toggle Component Names` 大小写敏感 | 必须与 Components 面板显示的名字完全一致 |
| 内建开关不播动画 | 只做变换插值 + 灯开关；要动画 / 音效走路径 B / C |
| 灯瞬间切换 | 不支持淡入淡出；需要渐变就用路径 B 自己做 |
| 描边需要后处理材质 | C++ 只负责写 CustomDepth / 模板值；**描边后处理材质需你手工做**（见 `Docs/EditorImplementationGuide.md` §4.6） |
| 描边作用于"可见图元" | 隐藏的碰撞代理默认跳过（`bOutline Visible Primitives Only`） |
| 一个物体一个 `Interactable` | 同一 Actor 上多份会互相覆盖状态；要用不同交互就拆 Actor |
| 运行时上下文是安全网 | 自己的 IMC 接好后建议关掉 `bRegister Interact Context`，避免重复注册 |

---

## 8. 相关文档

| 文档 | 内容 |
|---|---|
| `Docs/EditorImplementationGuide.md` | 编辑器内逐步搭建（含按键映射、描边材质做法） |
| `Docs/InteractionLogic.md` | 交互的人机活动逻辑与解耦设计原理 |
| `Docs/StructureInteraction.md` | 可交互建筑结构（方块式，另一套开合实现） |
| `Scripts/diagnose_interaction.py` | 只读诊断脚本 |
