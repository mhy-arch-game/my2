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

### 最小验证

1. 在 `BP_MHY_ARCH_GAMECharacter` 上挂 `Interaction Detector`（§3），`Interact Action` = `IA_Interact`；
2. 把 `Pick Mode` 设 `Line Trace`、勾 `bDraw Debug`；
3. PIE 里用准星对准 `BP_InteractiveStructure` → 应有绿色调试线；按 **E** → 结构开合。

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
