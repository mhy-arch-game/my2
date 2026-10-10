# 编辑器内实现指南（Editor Implementation Guide）

> 适用工程：**MHY_PROJ_v0_1** ｜ C++ 模块：**MHY_ARCH_GAME** ｜ 引擎：UE 5.7.4
> 前置结论：迁移只带了 C++，**Content 侧全部缺失**。本文给出在编辑器内**按顺序完成**的全部操作。
>
> **组件挂载采用手动方案（b）**：不使用 `SubobjectDataSubsystem` 脚本化改写蓝图的 SCS，
> 全部在蓝图编辑器里用 **Add Component** 完成。脚本只负责"建蓝图 + 设置 Class Defaults"。

---

## 0. 已经完成的部分（不需要你重做）

| 项 | 状态 |
|---|---|
| C++ 模块 `MHY_ARCH_GAME`（59 个文件 / 7 个玩法目录） | ✅ 编译通过，`UnrealEditor-MHY_ARCH_GAME.dll` |
| Epic 模板变体（Combat/Platforming/SideScrolling，76 文件） | ✅ 已按你的要求移除 |
| 10 个蓝图子类 | ✅ 已生成（见 §2） |
| 角色 4 个 Input Action、控制器 Mapping Context、GameMode 的 Pawn/Controller | ✅ 已接线并读回校验 |
| `GlobalDefaultGameMode` | ✅ 指向 `BP_MHY_ARCH_GAMEGameMode_C` |

> 文档中的类名映射：迁移文档里写的 `BP_ThirdPersonCharacter` / `BP_SideScrollingCharacter`
> 是**上一个工程的名字**，在本工程里统一替换为 **`BP_MHY_ARCH_GAMECharacter`**。

---

## 1. 需要在编辑器内**手动新建**的资产（二进制，脚本无法生成）

| # | 资产 | 类型 | 建议路径 | 用途 / 备注 |
|---|---|---|---|---|
| 1 | ~~**`IA_Interact`**~~ | Input Action | `/Game/Input/Actions/` | ✅ **已由脚本创建**，并已建 `IMC_Interaction`（`IA_Interact → E`）接到玩家控制器 |
| 2 | **`IA_TimeShift`** | Input Action | `/Game/Input/Actions/` | `UTimeShiftInputComponent.SwitchAction`（也可复用现有 IA） |
| 3 | **攀爬蒙太奇** | Anim Montage | `/Game/MHY_ARCH_GAME/Anims/` | 需带 Root Motion + `AnimNotifyState_MotionWarping` |
| 4 | **`M_LiquidGlowFlow`** | Material | `/Game/LiquidLight/Materials/` | LiquidLight 的流动材质（步骤见 `Docs/LiquidLight.md` §2.1） |
| 5 | 面片网格 | Static Mesh | 复用 `/Game/LevelPrototyping/Meshes/SM_Plane` | 赋给 `BP_LiquidLightSurface` 的 FluidSurface |
| 6 | **角色骨骼网格 + AnimBP** | Skeletal Mesh + AnimBP | 复用 `/Game/Characters/Mannequins/` | 角色现在**没有 Mesh**，§4.1 必须做 |
| 7 | 后处理描边材质 | Material（CustomStencil） | `/Game/MHY_ARCH_GAME/Materials/` | 焦点描边（`StructureInteraction` 的**预留项**） |
| 8 | 音效资源 | SoundWave / SoundCue | `/Game/MHY_ARCH_GAME/Audio/` | MovementAudio 槽位（**留空也能跑**，只广播事件） |
| 9 | 物理材质 + Surface 定义 | Physical Material | `/Game/MHY_ARCH_GAME/Physics/` | MovementAudio 按地面材质分套（可选） |

> 1、2 是**阻塞项**：`IA_Interact` 不存在则交互系统无法触发；`IA_TimeShift` 不存在则时空切换无输入。

---

## 2. 已生成的蓝图（`Content/MHY_ARCH_GAME/`）

| 蓝图 | 父类 | 说明 |
|---|---|---|
| `Blueprints/BP_MHY_ARCH_GAMECharacter` | `AMHY_ARCH_GAMECharacter` | 玩家角色（已接 4 个 IA） |
| `Blueprints/BP_MHY_ARCH_GAMEGameMode` | `AMHY_ARCH_GAMEGameMode` | 已接 DefaultPawnClass / PlayerControllerClass |
| `Blueprints/BP_MHY_ARCH_GAMEPlayerController` | `AMHY_ARCH_GAMEPlayerController` | 已接 IMC_Default + IMC_MouseLook |
| `Blueprints/BP_InteractiveStructure` | `AInteractiveStructure` | 可交互建筑结构 |
| `Blueprints/BP_RevealLightVolume` | `ARevealLightVolume` | 光照触发区 |
| `Blueprints/BP_RevealPlatform` | `ARevealPlatform` | 受光显形的平台 |
| `Blueprints/BP_ClimbSpot` | `AClimbSpot` | 攀爬点 |
| `Blueprints/BP_LiquidLightSurface` | `ALiquidLightSurface` | 液体光表面 |
| `Blueprints/BP_TimeShiftAnchor` | `ATimeShiftAnchor` | 时代锚点 / 布局参考 |
| `UI/WBP_InteractionPrompt` | `UInteractionPromptWidget` | HUD 交互提示 |

> 生成脚本是**幂等**的：`Scripts/generate_blueprints.py` + `Scripts/bp_manifest.json`。
> 想加新蓝图，往 manifest 里加一条再跑一次即可（已存在会跳过）。

---

## 3. ★ 组件挂载总表（方案 b 的核心）

**没有挂组件 = 对应系统完全不会运行。** 请逐条在编辑器里完成。

| 组件 | 挂到哪个蓝图 | 数量 | 依据 |
|---|---|---|---|
| `MotionWarpingComponent`（引擎自带） | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/Climb.md` §3.2 ① |
| `ClimbComponent` | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/Climb.md` §3.2 ② |
| `InteractionDetectorComponent` | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/StructureInteraction.md` §5.5 |
| `MovementAudioComponent` | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/MovementAudio.md` §5.2 |
| `TimeShiftTravelComponent` | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/TimeShift.md` §3.3 |
| `TimeShiftInputComponent` | `BP_MHY_ARCH_GAMECharacter` | 1 | `Docs/TimeShift.md` §3.3 |
| `TimeEraComponent` | **每一个建筑**（或建筑组根 Actor） | N | `Docs/TimeShift.md` §3.1 |
| `OverlapPassageComponent` | 需要"重叠即通行"的移动方块 Actor | N | `Docs/OverlapDemo.md` 步骤 2 |
| `StructureBlockComponent` | `BP_InteractiveStructure` **内部**（子组件） | N | `Docs/StructureInteraction.md` §5.3 |
| ❌ `StructureVisualComponent` | **不要手动加** | 0 | 由 `AInteractiveStructure` 自动创建 |

### 操作步骤（以角色为例）

1. 内容浏览器双击 `Content/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMECharacter`。
2. 左上 **Components** 面板 → **+ Add** → 搜索并添加上表前 6 个组件。
3. 建议顺序：先 `Motion Warping`（Climb 依赖它），其余顺序无关。
4. 点 **Compile** → **Save**。
5. 对 `BP_InteractiveStructure` 重复（只加 `Structure Block`）。

---

## 4. 角色蓝图 `BP_MHY_ARCH_GAMECharacter` 配置

### 4.1 骨骼网格（必须，否则角色不可见）

1. 选中 **Mesh (CharacterMesh0)** 组件。
2. `Skeletal Mesh` → 选 `/Game/Characters/Mannequins/Meshes/SKM_Manny`（或 Quinn）。
3. `Anim Class` → 选第三人称 `ABP_Manny` / `ABP_Quinn`
   （**不要**用 `/Game/FirstPerson/Anims/ABP_FP_Copy`，那是第一人称）。
4. 若朝向不对，调 `Relative Rotation` 的 Z（第三人称骨架通常需 `-90`）与 `Relative Location` 的 Z。
5. 角色自身已有 `CameraBoom` + `FollowCamera`，无需再加。

> C++ 里 `AMHY_ARCH_GAMECharacter` 只创建了 SpringArm + Camera，**有意把 Mesh 留给蓝图**。

### 4.2 交互探测器 `InteractionDetectorComponent`

| 属性 | 建议值 | 说明 |
|---|---|---|
| `Pick Mode` | `Sphere Overlap` 或 `Line Trace` | 球形=靠近即可；射线=需要瞄准 |
| `Interaction Radius` | 250 | 球形模式半径 |
| `Trace Distance` | 400 | 射线模式距离 |
| `bRequire Facing` | ✔ | 忽略身后目标 |
| `Update Interval` | 0.1 | 轮询间隔（焦点最多 0.1s 延迟） |
| `Probe Object Types` | WorldStatic / WorldDynamic / Pawn | 候选类型 |
| **`Interact Action`** | **`IA_Interact`**（需先建） | 设置后组件会**自绑定**到玩家的 Enhanced Input |

> 若不想自绑定：留空 `Interact Action`，在角色 `SetupPlayerInputComponent` 里绑定并调用 `TryInteract()`。

### 4.3 攀爬 `ClimbComponent`

| 属性 | 值 |
|---|---|
| `Climb Montage` | §1 的第 3 项蒙太奇 |
| `Move Mode` | `Motion Warping` |
| `Warp Target Name` | `ClimbTarget`（**必须与蒙太奇 Notify 里的名字一致**） |
| `Trace Up Height` | 400（需大于平台高度） |
| `Ledge Trace Channel` | `Visibility`（平台必须**阻挡**该通道） |
| `bSnap To Target On Finish` | ✔ |

### 4.4 移动音频 `MovementAudioComponent`

| 属性 | 值 |
|---|---|
| `Run Speed Threshold` | 300 |
| `bAuto Detect Jump And Land` | ✔ |
| `bAuto Footstep By Distance` | 过渡期可 ✔，正式接 AnimNotify 后 **关掉** |
| `Default Set` / `Surface Sets` | 可**留空**（只广播事件，不播放） |

### 4.5 时空切换 `TimeShiftTravelComponent` + `TimeShiftInputComponent`

| 组件 | 属性 | 值 |
|---|---|---|
| Input | `Switch Action` | `IA_TimeShift` |
| Travel | `bTeleport Owner` | ✔ |
| Travel | `Mapping Mode` | `Layout Origin`（默认，适合两套布局一一对应） |
| Travel | `Auto Collect Radius` | 800 |

---

## 4.6 交互系统：接口 + 组件，两条路径（★ 本轮已实现）

"靠近且对准一个可交互物 → 按键 → 执行该物体自己的逻辑" 已经实现，采用**双通道**设计，目标只需二选一：

| 路径 | 怎么用 | 适用 |
|---|---|---|
| **A. 实现接口** `IInteractableInterface` | 在**类**上实现（C++ 或**蓝图**都可以） | 一类对象共享同一套交互逻辑 |
| **B. 挂组件** `UInteractableComponent` | 拖到**具体实例**上，在 `On Interact Requested` 里写逻辑 | 不想改类，或每个实例逻辑都不同 |

> 关键改动：`IInteractableInterface` 已从 `NotBlueprintable` 改为 **`Blueprintable`**，5 个方法改为
> `BlueprintNativeEvent`，因此**任何蓝图都能直接实现这个接口**，不需要写 C++。
> （C++ 实现者改为覆盖 `_Implementation`，`AInteractiveStructure` 已同步。）

### 接口的 5 个方法（在"被交互物体"上实现）

| 方法 | 作用 |
|---|---|
| `CanInteract(Interactor)` | 现在能否交互；返回 false 则不会被聚焦 |
| `OnInteract(Interactor)` | 按下交互键时执行 —— **每个物体在这里写自己的逻辑** |
| `GetInteractionPrompt()` | HUD 提示文案 |
| `OnFocusBegin(Interactor)` | 成为聚焦目标（可开高亮） |
| `OnFocusEnd(Interactor)` | 失去聚焦（关高亮） |

### 探测逻辑（`UInteractionDetectorComponent`，挂在玩家身上）

- 每 `Update Interval`（默认 0.1s）搜索一次；
- `Pick Mode` = `Sphere Overlap`：以**玩家为圆心** `Interaction Radius`（默认 250）实现"靠近"；
- `Pick Mode` = `Line Trace`：从**相机视点**沿视线打 `Trace Distance`（默认 400）实现"对准"（天然带遮挡语义）；
- `bRequire Facing` + `Min Facing Cosine` 控制"对准"的严格度：`0.0`=前方半球，`0.5`≈60°，`0.9`≈25°；
- 评分 = `朝向点积 × 1000 − 距离`（对准优先，距离次之）；
- ⚠️ **瞄准已改为使用相机视点**（`AController::GetPlayerViewPoint`），不再用角色胶囊的朝向 ——
  胶囊 forward 永远不俯仰，无法表达"抬头/低头对准"。

### 输入（已接线，无需手建）

| 资产 | 状态 |
|---|---|
| `IA_Interact` | ✅ 已创建（Input Action，Boolean） |
| `IMC_Interaction` | ✅ 已创建，含 `IA_Interact → E` |
| `BP_MHY_ARCH_GAMEPlayerController.DefaultMappingContexts` | ✅ 已包含 `IMC_Interaction` |

所以**只要在探测器组件上把 `Interact Action` 设为 `IA_Interact`**，按 E 就能交互。

### 给一个新物体加交互

**路径 B（最快，零 C++）**
1. 打开该物体的蓝图 → **Add Component → `Interactable`**（`UInteractableComponent`）；
2. 设 `Interaction Prompt`（例如"开门"）；
3. 事件图表里选中该组件 → **On Interact Requested** → 接你自己的逻辑（开门 / 拾取 / 播音效…）；
4. 想要高亮：接 **On Focus Gained / On Focus Lost**。

> 同一个组件类，不同实例绑不同逻辑 —— 这正是"不同物体不同交互逻辑"。

**路径 A（一类对象共享逻辑）**
1. 蓝图 → **Class Settings → Interfaces → Add → `Interactable`**；
2. 在 **Interfaces** 分类下实现 `On Interact` 与 `Get Interaction Prompt`（`Can Interact` 默认返回 true）。

### 对准时描边加粗（★ 本轮已实现）

探测器在**聚焦变化**时给被瞄准的物体加粗描边，分两层机制：

| 机制 | 作用 | 前提 |
|---|---|---|
| **CustomDepth 模板值分层** | 聚焦时把模板值提到 `Focused Outline Stencil`（默认 **2** = "粗"档） | 描边材质按模板值分支（1=细 / 2=粗） |
| **材质参数集（可选）** | 直接把厚度标量推给描边材质，无需材质分支 | 需一个 MPC，且描边材质读同名标量 |

**探测器参数**（Category = `Interaction|Outline`）：

| 参数 | 默认 | 说明 |
|---|---|---|
| `bApply Focus Outline` | ✔ | 总开关 |
| `bUse Stencil Outline` | ✔ | 是否写 CustomDepth / 模板值 |
| `Focused Outline Stencil` | 2 | 聚焦档（"粗"） |
| `bOutline Visible Primitives Only` | ✔ | 跳过隐藏碰撞代理，避免多余描边 |
| `Outline Parameter Collection` | 空 | 可选 MPC |
| `Outline Thickness Parameter` | `OutlineThickness` | MPC 中的标量名 |
| `Focused Outline Thickness` | 4.0 | 聚焦时厚度 |
| `Resting Outline Thickness` | 1.5 | 无聚焦时厚度 |

**行为**
- 聚焦 → 该 Actor 所有**可见**图元 `SetRenderCustomDepth(true)` + 模板值 = `Focused Outline Stencil`，
  MPC 厚度设为 `Focused Outline Thickness`；
- 失焦 → **逐图元还原**到聚焦前状态（含原模板值），MPC 厚度回到 `Resting Outline Thickness`；
- 还原顺序是刻意设计的：**先还原描边，再让物体执行自己的 `On Focus End`**，避免"探测器"与
  "`UStructureVisualComponent` 自带高亮"两套系统互相留下残留描边；
- 对**实现接口**和**挂组件**两类物体都生效（探测器统一处理，物体侧不用自己写）。

**还差一步：描边后处理材质（Content 侧，项目里尚不存在）**

1. 新建 Material：`Material Domain = Post Process`，`Blendable Location = Scene Color After Tonemapping`；
2. `SceneTexture: CustomStencil` 采样本像素；
3. 以 `ScreenPosition` 为基准，按厚度偏移采样 4~8 个邻居的 CustomStencil；
4. 本像素**不等**目标模板值、但邻居**等于** → 把描边色输出到 `Emissive Color`（半透明混合）；
5. 厚度用 `ScalarParameter` 命名 `OutlineThickness`（或用 MPC 从参数集读）；模板值用分支区分 1（细）/ 2（粗）；
6. `Project Settings → Rendering → Postprocessing → Custom Depth-Stencil Pass = Enabled with Stencil`；
7. 把该材质加入关卡 Post Process Volume 的 `Post Process Materials`（或相机后处理）。

### 最小验证

1. 在 `BP_MHY_ARCH_GAMECharacter` 上挂 `Interaction Detector`（§3），`Interact Action` = `IA_Interact`；
2. 把 `Pick Mode` 设 `Line Trace`、勾 `bDraw Debug`；
3. PIE 里用准星对准 `BP_InteractiveStructure` → 应有绿色调试线；按 **E** → 结构开合。

---

## 4.7 实例：按 E 开一扇门（完整挂载步骤）

**C++ 侧已经全部就绪，不需要再写代码。** 本工程的"门"= `AInteractiveStructure`
（状态机 `Closed ⇄ Open`），按 E 会走 `ToggleState()`。

### 4.7.1 先确认依赖链

| 环节 | 资产 / 设置 | 状态 |
|---|---|---|
| 按键 E → 动作 | `IMC_Interaction`：`IA_Interact → E` | ✅ 已创建 |
| 输入上下文生效 | `BP_MHY_ARCH_GAMEPlayerController.DefaultMappingContexts` 含 `IMC_Interaction` | ✅ 已接线 |
| 交互动作资产 | `IA_Interact`（Boolean） | ✅ 已创建 |
| 探测器自绑定 | 角色上 `Interaction Detector` 的 `Interact Action = IA_Interact` | ⬜ **需要你做（见 4.7.2）** |
| 按键 → 调用 | `IA_Interact` 的 `Started` → `TryInteract()` | ✅ C++ 已实现 |
| 调用 → 开门 | `IInteractableInterface::Execute_OnInteract` → `AInteractiveStructure::OnInteract_Implementation` → `ToggleState()` | ✅ C++ 已实现 |

### 4.7.2 步骤 1：把探测器挂到角色上

1. 打开 `Content/MHY_ARCH_GAME/Blueprints/BP_MHY_ARCH_GAMECharacter`
2. **Components → + Add → 搜 `Interaction Detector`** → 添加
3. 选中它，在 Details 里设：
   - **`Interact Action` = `IA_Interact`** ← **关键；不设就不会自绑定，按 E 没反应**
   - `Pick Mode` = `Line Trace`（"对准"用这个）/ `Sphere Overlap`（靠近即可）
   - `Trace Distance` = 400；`bRequire Facing` ✔；`Min Facing Cosine` = 0.5
   - `bDraw Debug` ✔（调试期先开，能看到射线）
   - `Interaction|Outline` 段保持默认（对准时自动加粗描边）
4. **Compile → Save**

> **不想自绑定**：把 `Interact Action` 留空，改在角色的 `SetupPlayerInputComponent` 里绑定 E 并调用
> 探测器的 **`TryInteract()`**（也是 BlueprintCallable）。

### 4.7.3 步骤 2：做一扇"门"（`BP_InteractiveStructure`）

1. 打开 `Content/MHY_ARCH_GAME/Blueprints/BP_InteractiveStructure`
2. **Components → + Add → `Structure Block`**
3. 选中该 Structure Block → **+ Add → `Static Mesh`**，然后**把它拖到 Structure Block 之下**（必须变成子组件）
4. 选中这个子 `Static Mesh`：
   - `Static Mesh` = 门板网格（如 `/Game/LevelPrototyping/Interactable/Door/Meshes/SM_Door`）
   - 调 `Location` / `Scale`，让**门轴落在 Structure Block 的原点**（旋转要绕轴，不然会"漂"）
5. 选中 **Structure Block** 本身：
   - `Closed Transform` → Rotation = (0, 0, 0)（关）
   - `Open Transform` → Rotation = (0, 0, 90)（开，绕 Z 轴 90°）
   - `bDisable Collision When Open` 按需勾
   - 可选 `Normal Material` / `Active Material`
6. 选中结构根（**Class Defaults**）：
   - `Initial State` = `Closed`
   - `Transition Duration` = 0.6
   - `Interact Prompt` = `开门`
   - 可选 `Interact Sound`
7. **Compile → Save**

> ⚠️ **Structure Block 只是"变换/状态锚点"**：它自己不渲染。**没有子 Static Mesh 的块会被静默跳过**，
> 表现就是"按 E 什么都没发生"。

### 4.7.4 步骤 3：放进关卡并验证

1. 把 `BP_InteractiveStructure` 拖进 `firstvision`，放在角色能走到的地方
2. **Play**
3. 准星对准门 → 应看到绿色调试射线，门被描边且**加粗**
4. 按 **E** → 门在 `Transition Duration` 内从 Closed 转到 Open，材质切到 `Active Material`
5. 再按 **E** → 转回 Closed
6. 若按 E 没反应，按顺序排查：
   - 角色上有没有 `Interaction Detector`？`Interact Action` 设了 `IA_Interact` 吗？
   - `bDraw Debug` 打开后有没有射线？没射线 = 没聚焦到（改 `Pick Mode`/`Trace Distance`/`Min Facing Cosine`）
   - 门里有没有**带子 Static Mesh 的 Structure Block**？
   - `IMC_Interaction` 里那行映射的键是不是 E？（打开资产看一眼）

### 4.7.5 最快：用"内建开关"开门（★ 推荐，零蓝图节点）

`UInteractableComponent` 现在带一个**可选的内建开关**：开启后，交互会自动把指定组件在
`Closed Relative Transform` 与 `Open Relative Transform` 之间插值 —— **不需要连任何蓝图节点**。

给 `active_door`（或任意门蓝图）做：

1. 打开门蓝图 → **+ Add → `Interactable`**
2. 选中该组件，在 Details 里设：
   - `Interaction Prompt` = `开门`
   - ☑ **`bUse Built-in Toggle`**
   - **`Toggle Components` → +** → 选中要转起来的那个组件（门板）
   - `Closed Relative Transform`：Location / Rotation / Scale 全 0（关门姿态）
   - `Open Relative Transform`：**Rotation Z = 90**（开门姿态）
   - `Toggle Duration` = 0.6
   - ☑ `bDisable Collision When Open`（开门后可穿过）
3. Compile → Save

运行时：对准门按 **E** → 门转到 Open；再按 → 转回 Closed。
`On Toggle Changed` 可用来加音效/其他表现；`On Interact Requested` 仍然会广播，需要更复杂逻辑时照用。

> 这个内建开关是**可选的**（默认关）。关掉时就回到"完全在蓝图里写逻辑"的模式。

### 4.7.6 灯的开关（同一套交互框架）★ 本轮已实现

灯和门**共用同一个 `Interactable` 组件、同一个状态**（`IsOpen()`），约定：**灯亮 = `IsOpen()` 为 true**。

| 参数（Category `Interaction|Light Switch`） | 作用 |
|---|---|
| `bToggle Lights` | 开启灯光开关 |
| `Light Components` | 要开关的灯组件（同样可能选不到，原因见 4.7.5） |
| **`Light Component Names`** | **按组件名指定（推荐）**，例如 `PointLight` |
| `Are Lights On` | 查询当前是否亮着 |

**行为**
- `bStart Open` 决定初始状态：**勾上 = 一开始就亮**（做"按 E 关灯"）；不勾 = 一开始灭（做"按 E 开灯"）
- 交互时对每盏灯 `SetVisibility(bOn)` + `SetIntensity(0 或原始亮度)`；
  **原始亮度在 BeginPlay 缓存**，切回来精确还原（不会把美术调的亮度弄丢）
- 灯是**瞬时**切换，不参与 `Toggle Duration` 的插值
- 同一个组件可以**同时**驱动门板变换 + 灯（"按 E 开门并点亮门内的灯"）：两个开关都勾即可
- 解析不到灯组件时会在 Output Log 打 Warning

**搭建步骤（任意带灯的 Actor）**
1. 该 Actor 上确保有灯组件（`Point Light` / `Spot Light` / `Rect Light`）
2. **+ Add → `Interactable`**
3. `Interaction Prompt` = `开灯` / `关灯`
4. ☑ **`bToggle Lights`**
5. **`Light Component Names` → +** → 填灯组件在 Components 面板里的确切名字（大小写敏感）
6. `bStart Open` = 你想要的初始状态（亮 = 勾）
7. Compile → Save

> 若下拉框能列出组件，也可以直接用 `Light Components`；选不到就用名字数组（和门板同理）。

### 4.7.7 备选：完全自己写开门逻辑

想在蓝图里自己写开门（播动画 / Timeline / Niagara）：
1. 给目标 Actor（也可以是 `BP_InteractiveStructure`）**+ Add → `Interactable`**
2. 设 `Interaction Prompt` = `开门`
3. 事件图表里**选中该组件** → 右键加 **On Interact Requested** 事件 → 接你的逻辑
   （`Set Actor Rotation` / Timeline / `Play Sound` …）
4. 要高亮：接 **On Focus Gained** / **On Focus Lost**

> 同一个 `Interactable` 组件类，不同实例绑不同逻辑 —— "不同物体不同交互逻辑"。

---

## 5. 各模块详细步骤

### 5.1 StructureInteraction（可交互建筑结构）

详见 `Docs/StructureInteraction.md` §5。要点：

1. 打开 `BP_InteractiveStructure`，**Components → Add → `Structure Block`**，可加多块。
2. **每块下面再 Add → `Static Mesh`，并把它拖到该 `Structure Block` 之下**作为子组件
   （块本体只是变换/状态锚点；**没有子 Mesh 的块会被静默跳过**）。
3. 选中子 `Static Mesh` → 指定网格（如 `SM_Cube`）、设置位置与缩放。
4. 选中该 `Structure Block` → 设 `Closed Transform` / `Open Transform`。
5. 材质集：`Normal Material` / `Active Material`（可选 `Disabled` / `Highlight`）。
6. **Class Defaults**：`Initial State`=`Closed`、`Transition Duration`=0.8、`Interact Prompt`（如"开启"）、可选 `Interact Sound`。
7. 拖进关卡，放在玩家可接近处。
8. **HUD（可选，预留项）**：在角色或 HUD 蓝图里，对探测器的 `OnFocusChanged` 事件
   → `Create Widget`(`WBP_InteractionPrompt`) → 调用其 `SetPrompt`；无焦点时移除。
9. 后处理描边（预留项）：项目设置里开启 Custom Depth-Stencil，做描边材质后用
   `SetRenderCustomDepth` / `SetCustomDepthStencilValue`（C++ 已实现开关）。

### 5.2 Climb（攀爬）

1. **做蒙太奇**：选带 Root Motion 的攀爬动画 → 右键 `Create → Anim Montage`。
2. 时间轴上覆盖"位移段"→ 右键 `Add Notify State → Motion Warping`。
3. 打开该 Notify：`Warp Target Name` = `ClimbTarget`（与组件一致）。
4. 按 §3 给角色加 `Motion Warping` + `Climb` 组件，按 §4.3 配置。
5. **摆攀爬点**：`Place Actors` → 搜 `Climb Spot`（或拖 `BP_ClimbSpot`）→
   **让 Actor 箭头朝向墙面/平台**（该朝向就是攀爬面向）→ 设 `Align Offset`、`Max Climb Height`。
6. 验证：走进范围自动攀爬；结束时 X/Y 与起始一致、Z 落在平台顶面。

### 5.3 MovementAudio（移动音频）

1. 按 §3 给角色加 `Movement Audio` 组件。
2. **接 AnimNotify（正式做法）**：打开走/跑动画 → 在脚掌着地帧右键 `Add Notify → Movement Audio`，
   `Event` = `Footstep`；跳跃/落地动画同理选 `Jump` / `Land`。
3. 然后把组件的 `bAuto Footstep By Distance` **关掉**。
4. **分地面材质（可选）**：`Project Settings → Physics → Physical Surface` 定义名字（Grass/Stone/Metal）
   → 给地面材质的 Physical Material 指定对应 Surface → 在组件 `Surface Sets` 加同名条目。
5. 声音资源可以一直留空；先订阅四个事件打印日志验证触发时机。

### 5.4 TimeShift（古今时空切换）

详见 `Docs/TimeShift.md` §3 与可运行用例 `Docs/TimeShiftDemo.md`。要点：

1. 在**同一张地图**里搭两套建筑，放在不同世界坐标（互不重叠）。
2. 给每个建筑（或建筑组根）挂 `Time Era` 组件，设 `Era` = `Ancient` / `Modern`。
3. **摆布局参考锚点（每时代 1 个）**：
   - 古布局里选基准点放 `BP_TimeShiftAnchor` → `Era`=`Ancient`、勾 `bDefines Layout Origin`、`Anchor Id` 留空；
   - 今布局的同一语义位置再放一个 → `Era`=`Modern`、同样勾选。
4. 按 §3/§4.5 给角色加 `Time Shift Input` + `Time Shift Travel`。
5. **随行物品**：给要一起穿越的 Actor 打上 `TimeTraveler` 标签。
6. 验证：按切换键 → 建筑显隐互换、玩家被映射到对应位置；连按受 2 秒计时锁限制。

### 5.5 LightReveal（受光显形）

> ⚠️ 该模块**没有随迁移提供设计文档**（`Docs/` 下无 `LightReveal.md`）。以下依据 C++ 头文件给出。

1. **平台**：打开 `BP_RevealPlatform`：
   - `SolidBox`：物理支撑盒，尺寸覆盖平台面；**默认碰撞关闭**（未点亮）。
   - `DetectorBox`：重叠检测盒，**向上延伸**，用于知道有没有人站在上面（防止抽走时把人摔下去）。
   - `VisualMesh`：表面网格，指定 Static Mesh + 材质；材质若有标量参数 `Opacity`，
     点亮时透明度会被驱动（参数名可用 `Opacity Parameter Name` 改）。
   - `bStays Revealed`：勾上则点亮后永久通行（单向门）。
2. **光源区**：打开 `BP_RevealLightVolume`：
   - `LightZone`（盒体）：调整到覆盖"光区"范围。
   - **`Revealed Actors`（Instance Only）**：拖入要受本区影响的 `BP_RevealPlatform` 实例
     （这是**实例级**属性，要在**关卡里选中该实例**设置，不是 Class Defaults）。
   - `bAny Pawn Carries Light`：✔ 表示任何 Pawn 进入都算带光；取消则只认 Owner 带 `LightCarrier` 标签的 Actor。
3. 验证：进入光区 → 平台碰撞开启并显形；离开 → 若无人站立则恢复不可通行。

### 5.6 OverlapPassage（重叠即通行）

完整步骤见 `Docs/OverlapDemo.md`。要点顺序：

1. （可选）做半透明/变材质用的材质。
2. 建移动方块蓝图，挂 `Overlap Passage` 组件。
3. 两方块交叉摆放。
4. **互相指定 `OtherActor`（关键步骤，两个方向都要指定）**。
5. 配置重叠时的材质/透明切换。
6. 运行验证。

### 5.7 LiquidLight（液体光表现）

见 `Docs/LiquidLight.md`。要点：

1. 手动创建材质 `M_LiquidGlowFlow`（Translucent；双层 Panner 扰动法线 + Emissive + Fresnel；
   标量参数 `FlowStrength` / `Opacity`）。
2. 打开 `BP_LiquidLightSurface`：给 `FluidSurface` 指定 `SM_Plane`，材质 0 设为该材质。
3. 拖进关卡；可勾 `bDrive Light With Flow` 让 `OptionalLight` 随之点亮。
4. 运行时调用 `SetFlowActive(true/false)` 验证"平静 → 发光流动"的过渡。

---

## 6. 关卡布置检查表

- [ ] 关卡 `firstvision` 里有 **PlayerStart**（否则玩家出生在原点）
- [ ] 放入至少一个 `BP_InteractiveStructure`（靠近玩家出生点便于验证）
- [ ] 放入 `BP_RevealLightVolume` + 若干 `BP_RevealPlatform`，并在**实例**上互相绑定
- [ ] 放入 `BP_ClimbSpot`（朝向可攀爬的墙/平台，且上方有可站立面）
- [ ] 放入 `BP_LiquidLightSurface`
- [ ] TimeShift：两套建筑各挂 `Time Era`，每时代 1 个布局参考锚点
- [ ] OverlapPassage：两方块互指 `OtherActor`

---

## 7. 验证清单

| 验证 | 预期 |
|---|---|
| PIE 启动 | 出生在 `firstvision` 的 PlayerStart，能看到**角色模型**（做完 §4.1 后） |
| WASD / 鼠标 / 空格 | 移动、视角、跳跃正常（IA 已接线） |
| 靠近结构 | 探测器建立焦点（可用 `bDrawDebug` 或 `OnFocusChanged` 打印验证） |
| 按交互键 | 结构 Closed ⇄ Open 插值、材质切换、可选音效 |
| 走进攀爬点 | 自动攀爬，落点 X/Y 与起点一致 |
| 按切换键 | 古今建筑互换、玩家位置映射、连按被计时锁拒绝 |
| 进入光区 | 平台碰撞开启 + 显形 |
| 方块重叠 | 可通行 + 材质/透明变化 |

---

## 8. 已知缺口（需要你决定或补充）

| # | 缺口 | 影响 |
|---|---|---|
| 1 | ~~`IA_Interact` 不存在~~ | ✅ **已解决**：`Scripts/create_interact_input.py` + `create_interact_mapping.py` 建好 `IA_Interact` 与 `IMC_Interaction`（E 键） |
| 2 | `IA_TimeShift` 不存在 | 时空切换无输入（可复用现有 IA 规避） |
| 3 | `Docs/LightReveal.md` **未随迁移提供** | 我按头文件推断的 §5.5 可能与你原设计有出入 |
| 4 | 角色 Mesh / AnimBP 未指定 | 角色不可见（§4.1） |
| 5 | 攀爬蒙太奇不存在（需带 Root Motion） | Climb 无法播放动画（会回退 CurveDriven） |
| 6 | 迁移未包含任何蓝图资产，本指南的 10 个是脚本新建的 | 若你原有蓝图仍在别处，建议对比后合并 |
| 7 | `StructureInteraction` 的描边/粒子/相机/HUD 为**预留项** | 按设计文档需后续接入 |
| 8 | 未把 `GameplayStateTree` 插件关掉 | 设计文档 §7 提到"后续可能迁移到 StateTree"，故保留启用 |

---

## 9. 相关文档索引

| 文档 | 内容 |
|---|---|
| `Docs/StructureInteraction.md` | 可交互建筑结构（分层、类清单、搭建 Demo） |
| `Docs/InteractionLogic.md` | 交互的人机活动逻辑 + 解耦设计原理（13 节） |
| `Docs/Climb.md` | 攀爬（MotionWarping + 蒙太奇步骤） |
| `Docs/MovementAudio.md` | 移动音频接口与资源槽 |
| `Docs/TimeShift.md` | 古今时空切换 |
| `Docs/TimeShiftDemo.md` | 时空切换可运行用例 |
| `Docs/LightReveal.md` | ❌ 缺失（见 §8 第 3 条） |
| `Docs/LiquidLight.md` | 液体光材质做法 |
| `Docs/OverlapDemo.md` | 重叠即通行 Demo |
| `Docs/PERFORMANCE_AND_CRASH_ANALYSIS.md` | 性能与崩溃分析（P0 集显/页面文件） |
| `Scripts/generate_blueprints.py` | 蓝图生成脚本（幂等） |
| `Scripts/bp_manifest.json` | 生成清单与接线声明 |
| `Docs/MovementAudio.md` | 移动音频接口 + **★ 声音资产接入实操（跑动 / 跳跃 / 落地）**（第 8 节） |
| `Docs/TimeEraPortal.md` | 跨时空传送（配对、落点、过场接口、落点偏移根因与修复、`TeleportOffset` 微调） |
| `Docs/GravityZone.md` | 重力区（最高点不变提速、`TargetJumpHeight` 绝对高度） |
## 10. ★ 脚本批量改资产的持久化规则（血泪教训）

headless 脚本（`-run=PythonScript`）改属性能"报成功"但**实际没存进去**，是本项目最容易踩的坑。
必须把两类对象分开处理：

| 改的是什么 | 正确写法 | 错误写法的症状 |
|---|---|---|
| **Blueprint 的 SCS 组件模板**（`SubobjectDataSubsystem` 拿到的 template object） | `set_editor_property(name, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)`，再 `BlueprintEditorLibrary.compile_blueprint(bp)` + `EditorAssetLibrary.save_loaded_asset(bp)` | **不加** `notify_mode`：编译保存后值被还原（`RotationPivot` 曾退回 `(0,0,0)`） |
| **关卡里已摆放实例的组件属性** | 普通 `set_editor_property(name, value)`（**不带** `notify_mode`）+ `EditorLoadingAndSavingUtils.save_current_level()` | **加了** `notify_mode=ALWAYS`：同进程读回是新值、`save_map` 返回 True、umap mtime 也变了，但**新进程 `load_map` 读回来还是旧值**（静默丢失） |
| 关卡实例的 **Actor 自身**属性（位置 / 旋转 / 标签 / Era） | 普通 `set_editor_property` + `EditorLoadingAndSavingUtils.save_map(world, MAP)` | — |

**验证方式（强制）**：只在**新进程**里 `load_map` + 读回才算数。
同进程读回值、`save_*` 的返回值、文件 mtime —— 这三样**都不可信**（mtime 会变而内容没改）。

已验证通过的脚本模板：

- 关卡实例的组件属性：`Scripts/apply_door_hinge.py`、`Scripts/apply_gravity_instance.py`
- 蓝图 SCS 模板属性：`Scripts/apply_gravity_preset.py`、`Scripts/persist_swift.py`（phase B）

> 另一条相关坑：World Partition 关卡里"按实例覆盖"的行为**不统一** ——
> `UTimeEraPortalComponent` 的实例覆盖存不进去，`UTimeEraComponent::Era`、`UGravityZoneComponent` 的可以；
> 因此**能从模板出的配置就放模板**，实例只放"每个不一样"的值。

=> `Docs/GravityZone.md` 第 7 节还记了同类问题（构造函数里必须用 `InitBoxExtent` 而非 `SetBoxExtent`）。

### 10.1 碰撞 profile：改模板，不要改实例

| 改的是什么 | 正确写法 | 错误写法的症状 |
|---|---|---|
| 碰撞 profile（`set_collision_profile_name`） | 改**蓝图 SCS 模板**：`set_collision_profile_name` + `compile_blueprint` + `save_loaded_asset`；实例靠继承 | 对**关卡实例**调 `set_collision_profile_name`：本进程读回是 `IgnoreOnlyPawn`，`save_current_level()` 也返回 True，但**新进程读回来还是旧 profile**（未标记包 dirty，静默丢失） |

### 10.2 ★ 编辑器开着的时候，绝对不要用脚本改资产/关卡

实测：脚本在 15:42 把 `jumping_area` 写成 `TargetJumpHeight=1125 / GravityScaleInside=0.32` 并保存，
新进程复验也通过；但 `Saved/Autosaves/Game/FirstPerson/firstvision_Auto1.umap` 的 mtime 是 **15:58**，
随后一次编辑器保存把**它内存里的旧值**（1.2/550）写回磁盘 —— 脚本的改动被静默覆盖。

**开工前的检查清单**：

1. `Get-Process UnrealEditor*` 必须为空（否则构建也会因 Live Coding 失败）
2. 看一眼 `Saved/Autosaves/**` 的 mtime：若比你上次写入更新，说明编辑器正在跑
3. 每次写完都要**新进程复验**，并在用户打开编辑器之前确认结果
4. 编辑器里改过的值**以编辑器为准**，不要用脚本去"抢"同一个字段
