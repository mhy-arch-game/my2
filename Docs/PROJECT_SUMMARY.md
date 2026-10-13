# MHY_ARCH_GAME 项目技术总结文档

> 范围：UE 5.7.4 工程 `MHY_PROJ_v0_1`（C++ 模块 `MHY_ARCH_GAME`），git `develop` 分支 57 个提交（当前 HEAD `4978918`）。
> 本文统一归纳：**① 提交历史 / 功能开发路径 ② 技术栈 ③ 工程（工业级）设计知识点**。
> 生成时间：2026-10-11（以仓库实际提交与 `Docs/` 现有文档为准）。

---

## 1. 项目概览

| 项 | 内容 |
|---|---|
| 引擎 | Unreal Engine **5.7.4**（`++UE5+Release-5.7-CL-51494982`；二进制引擎位于 `E:\UE5_engine\UE_5.7`）|
| 目标平台 | Windows x64，DirectX 12 / SM6（Lumen + 部分 Nanite）|
| 工程 / 模块 | 工程 `MHY_PROJ_v0_1`；C++ 模块 `MHY_ARCH_GAME`（Game 模块，配大量 Blueprint）|
| 类型 | 第一人称交互解谜 / 机关建筑：门、机关灯、重力区、传送器、时间纪元、UI 语音导览 |
| 起步地图 / GameMode | `/Game/FirstPerson/firstvision`；`BP_MHY_ARCH_GAMEGameMode` |
| 仓库 | git（`develop`），远程 `github.com/mhy-arch-game/my2`，Git LFS 归档过大包 |

---

## 2. 功能开发路径历史（提交 → 里程碑）

开发按时间线可归纳为六个阶段（提交号为缩写，可在仓库里 `git show` 核对）：

### 阶段 1 · 工程化奠基（2026-10-08）

| 提交 | 内容 |
|---|---|
| `0bcb24c` | `chore: baseline snapshot` 基线快照 |
| `3369f66` | 统一命名、配置起步地图、记录性能问题文档 |
| `b75f309` | 新增 C++ 游戏模块与基础交互框架 |
| `8bdb50a` | 把迁移来的 `MHY_ARCH_GAME` 模块作为核心 |
| `b253240` | 为迁移模块批量生成 Blueprint 并接线 |
| `241761e` | 清理 Epic 模板变体，新增编辑器实施指南 |
| `e676af7` | 蓝图可授权的交互契约 + 可挂载组件 |

### 阶段 2 · 核心机关（2026-10-09）

| 提交 | 内容 |
|---|---|
| `7366d05` | `<feat> renew project structure` 结构重整 |
| `94ec96f` | `<feat> door control, lamp control, gravity control` 门控 / 灯控 / 重力 |

### 阶段 3 · 功能扩展与多人联调（2026-10-10）

| 提交 | 内容 |
|---|---|
| `3db1cb0` | 视频内容实现 |
| `64118ca` | `<feat> reload sound system` 声音系统 |
| `0690cbf` / `65be8f0` | 模式错误修复 / P0 修复 |
| `425db11` | `<big fix able merge into main>` 大修并入主线 |
| `246b0ae` | 更新 `firstvision.umap` |

> 该阶段有大量协作提交（`Leslie*`、`fix`、多次 `Merge branch develop`），是多人并行改同一张地图、频繁解冲突的时期。

### 阶段 4 · 发布化（2026-10-11 上午）

| 提交 | 内容 |
|---|---|
| `dce57a0` | `<fix> 针对发布版本 v0.2 进行修正` |
| `925df4c` | `<fix> 调整部分可视但不可碰撞的结构` |
| `0b953c6` | `<feat> debug and constructure`（对应 v0.2 发布包）|

### 阶段 5 · 打包与资产工程（2026-10-11 下午）

| 提交 | 内容 |
|---|---|
| `939b280` / `d6282c7` | 记录 `0b953c6` 与 `e6c3ded` 的 Shipping 出包归档路径与体积 |
| `e6c3ded` | `<fix> 打包时强制收 UiCue 表与配音（DirectoriesToAlwaysCook）` |
| `2adfd20` | `<docs> 人物模型替换：兼容性分析与风险清单` |
| `92f06cb` / `ffd9867` | `<build> Git LFS 归档含 UiCue 的 Shipping 包（29 文件 / 895.7 MB，已推送）` |

### 阶段 6 · 发行版修复 + 新功能（2026-10-11 晚，本次会话主体）

| 提交 | 内容 |
|---|---|
| `61568a7` | `<fix> 对齐需求调整发布版` —— 一次性纳入本次会话全部成果（见下），随后 `4978918` 合并到 `develop`（65 文件 / +4422 行）|

该提交包含的功能与修复：

1. **UiCue 发行版修复**：触发 id 由"编辑器标签"改为打包安全的「对象名 / `UiCue:xxx` Actor Tag」，关卡 8 个物件补标签；新增 `-UiCueSelfTest` 发行版自检开关；文档 `Docs/UiCue.md` §11、`Docs/Packaging.md`。
2. **结束游戏系统（GameFlow）**：`UGameFlowLibrary::EndGameWithFade`（对外接口）、`UEndGameSubsystem`（渐黑 1.2s → 全黑 0.6s → 退出进程）、`AEndOfTheGameActor`（行为基类，组件归蓝图，交互提示词「触碰」）。
3. **永续自转**：`URotatingMovementComponent`（组件名 `EndGameSpin`），默认绕 Z 轴 45°/s 永续播放。
4. **蓝图类 `end_of_the_game`**：复用工程里已有空蓝图（父类从 `Actor` 改为 `AEndOfTheGameActor`），关卡实例保留在原位 `(-20040,-665,-2890)`。
5. **关卡 / 贴图**：`firstvision.umap` 更新；补 `concrete_wall_009_diff_4k` / `_disp_4k` / `_rough_4k` 及各自 `_Mat`。
6. **脚本与文档**：全套 Python 探针 / 修复 / 验证脚本（`Scripts/*`）与 `Docs/EndGame.md`。

---

## 3. 技术栈

| 层次 | 内容 |
|---|---|
| 语言 / 构建 | C++（UBT / UHT，`BuildSettingsVersion.V6` + `EngineIncludeOrderVersion.Unreal5_7`）；Blueprint；VS2022 MSVC 14.44；Windows SDK |
| 构建 / 打包 | `UnrealBuildTool`（Build.bat）、`RunUAT`（`BuildCookRun -build -cook -allmaps -stage -pak -archive`）；IoStore 容器 `.ucas` / `.utoc` / `.pak`；DDC 用 `InstalledNoZenLocalFallback`（本机 Zen 起不来）|
| 运行时框架 | `UGameInstanceSubsystem`（全局一次：UiCue / EndGame / BackgroundMusic）、`UWorldSubsystem`、`UActorComponent`；`FTSTicker` 核心 ticker 自驱 |
| 输入 | Enhanced Input（`UInputAction` / `UInputMappingContext`，检测器带运行时映射兜底 `bRegisterInteractContext`）|
| UI | UMG（`UUserWidget`）+ Slate（`SImage` / `SBorder` / `FCoreStyle`），屏幕字幕 / 交互提示 / 全屏黑屏 |
| 资产 / 数据 | `UDataAsset`（`UUiCueSet`）；`TSoftObjectPtr` 软引用；`AssetRegistry` 运行时扫描；`ConstructorHelpers::FObjectFinder` 硬引用（保证 cook）|
| 音频 | `USoundWave`；常驻 `UAudioComponent`（`bAutoDestroy=false`）+ `OnAudioFinished` 逐段连播 |
| 渲染 / 材质 | Nanite（部分网格关闭以支持 overlay）、Custom Depth / Stencil（`r.CustomDepth=3`）、Overlay Material（Fresnel）、Lumen、DX12 SM6 |
| 发布 / 部署 | Shipping（`USE_LOGGING_IN_SHIPPING=0`，`UE_LOG` 全裁掉）；发行包去掉 `.pdb`；跨机需 VC++ 2015-2022 运行库 |
| 版本管理 | git + **Git LFS**（`.gitattributes` 路径 filter；`E:\Git` 缺 `git-remote-https.exe`，用 GitHub Desktop 自带 git 2.53 推送）|
| 自动化 / 无头验证 | Editor Python（`-run=PythonScript` 命令集）、headless `-game`、`init_unreal.py` 注入、`SubobjectDataSubsystem` / `EditorActorSubsystem` / `AssetRegistry` |

---

## 4. 架构与模块地图

| 目录 / 模块 | 职责 | 关键类 / 文档 |
|---|---|---|
| `StructureInteraction` | 交互核心：契约、可挂组件、检测器、提示 | `IInteractableInterface`、`UInteractableComponent`、`UInteractionDetectorComponent`、`UInteractionPromptComponent/Widget`；`InteractionImplementation.md` |
| `InteractionOperation` | 交互联动（一物驱动他物） | `UInteractionLinkComponent`、`UInteractionOperationReceiverComponent`；`InteractionOperationLink.md` |
| `UiDirector` | UI 活动导演（语音 + 字幕） | `UUiCueSubsystem`、`UUiCueSet`、`AUiCueTriggerVolume`；`UiCue.md` |
| `GameFlow`（新） | 结束游戏 + 永续自转 | `UGameFlowLibrary`、`UEndGameSubsystem`、`AEndOfTheGameActor`；`EndGame.md` |
| `GravityZone` | 重力区 | `UGravityZoneComponent`；`GravityZone.md` |
| `TimeShift` | 时间纪元 / 传送 | `UTimeShiftSubsystem`、`UTimeEraComponent`、`UTimeEraPortalComponent`；`TimeShift.md`、`TimeEraPortal.md` |
| `ProximitySequence` / `ProximityBarrier` | 靠近触发 / 阻挡 | `UProximitySequenceComponent`、`UProximityBarrierComponent`；`ProximitySequence.md`、`ProximityBarrier.md` |
| `BackgroundMusic` | 背景音乐 | `UBackgroundMusicSubsystem`、`UBackgroundMusicSettings`；`BackgroundMusic.md` |
| `MovementAudio` / `Sprint` / `Climb` | 移动音频 / 冲刺 / 攀爬 | `UMovementAudioComponent`、`USprintComponent`、`UClimbComponent`；对应 md |
| `LightReveal` / `LiquidLight` / `OverlapPassage` | 光揭示 / 液面光 / 重叠通道 | `ARevealPlatform`、`UOverlapPassageComponent`；对应 md |

---

## 5. 工程与设计知识点总结（"工业设计知识点"）

> 本项目的"工业设计"体现在：把一整套机关玩法做成**可复用、可授权、可打包、可验证**的软件组件。下面按主题归纳成知识点。

### 5.1 交互系统设计

- **双重契约**：`IInteractableInterface`（类级，实现者自带逻辑）+ `UInteractableComponent`（可挂组件，任意 Actor 无需改类即可交互）—— 检测器两者通吃，类型无关。
- **玩家侧单一检测器** `UInteractionDetectorComponent`：SphereOverlap / LineTrace 两种拾取、面向阈值（`bRequireFacing` + `MinFacingCosine`）、聚焦粘滞（`FocusStickinessBonus`）、穿透遮挡（`bPierceOccluders`）、overlap 兜底。
- **提示词随状态切换**：`InteractionPrompt`（关）/ `InteractionPromptOpen`（开）；HUD 表现与逻辑解耦（`UInteractionPromptComponent` 驱动 `UInteractionPromptWidget`）。
- **聚焦视觉二选一**：stencil 描边 vs overlay 覆盖材质；按图元备份再还原；**overlay 只对非 Nanite 网格生效**（引擎没有 Nanite 的 overlay 路径）。

### 5.2 UI 事件导演（UiCue）

- **全局唯一 + 自驱**：`UGameInstanceSubsystem` + `FTSTicker` 核心 ticker，不依赖关卡里放不放组件。
- **DataAsset 驱动**：事件表 / 间隔 / 断句符 / 字幕偏移全部外置，改资产即生效。
- **事件队列 + 不并发**：一条播完等 `QueueGapSeconds` 再播下一条；长文本按断句符切句、配音逐段连播（常驻 `UAudioComponent` + `OnAudioFinished`，不用会自毁的组件）。
- **触发 id 必须"可进包"**：`AActor::ActorLabel` 是 `WITH_EDITORONLY_DATA`，**打包后不存在**；发行版里只能靠「对象名 / Actor Tag（`UiCue:xxx`）」匹配 —— 这是"PIE 正常、包内失效"类 bug 的经典来源。
- **Shipping 无日志**：给模块加"命令行开关 → 写文件"的自检（`-UiCueSelfTest`），作为发行版的唯一现场排查手段。

### 5.3 组件命名与蓝图重载（重要教训）

- C++ 基类 `CreateDefaultSubobject` 的组件名**不能和蓝图子类已有组件同名**，否则蓝图编译报
  `Internal Compiler Error: Tried to create a property X in scope ..., but another object already exists there`。
- 通用做法：**组件归蓝图 / 实例，基类只提供行为**；确实要运行时补组件用 `NewObject + RegisterComponent`；基类组件名加前缀（如 `EndGameSpin`）。

### 5.4 状态与生命周期

- 选型：`GameInstanceSubsystem`（跨关卡、全局一次）vs `UWorldSubsystem`（每世界）vs `ActorComponent`（随 Actor）。
- 常驻音频组件（`bAutoDestroy=false`）才能让 `OnAudioFinished` 稳定回调（自毁组件在结束时先销毁，回调不可靠）。

### 5.5 发布工程（打包 / 部署）

- **硬引用自动 cook**；软引用 / `AssetRegistry` 运行时扫描 / ini 字符串路径的资产必须写 `+DirectoriesToAlwaysCook`。
- Shipping 默认 `USE_LOGGING_IN_SHIPPING=0`，`UE_LOG` 全被裁掉；要现场日志需在 Target.cs 打开（面向用户端的正式包不要）。
- 发行包去掉 `.pdb`（~211 MB）；**不要**用 `-nodebuginfo` 代替 —— 它会连带剔掉 `SlateDebug/Fonts/LastResort.*`。
- **跨机自包含**：引擎运行时 + 内容 + 配置都在包内、无本机绝对路径、中文字体（DroidSansFallback）已进包；目标机仍需 VC++ 2015-2022 x64 运行库 + DX12/SM6 显卡。
- **Git LFS 存大包**：指针/`git restore` 会覆盖真实文件（致命）；免费配额有限；本机用 GitHub Desktop 自带 git（系统 git 缺 `remote-https`）。

### 5.6 自动化与无头验证

- Editor 命令集：`UnrealEditor-Cmd.exe <uproject> -run=PythonScript -Script=... -nullrhi -DDC-ForceMemoryCache`（无物理、无渲染，只做数据/结构验证）。
- headless `-game` 真会话：`UnrealEditor.exe <uproject> -game -windowed ...`；注入 Python 用临时 `Content/Python/init_unreal.py`（`-ExecCmds=py <file>` 会跑在 Python 插件就绪之前，无效）；取世界用 `unreal.load_object(None, "/Game/<map>.<map>")`；`EditorLevelLibrary.get_game_world()` 在 `-game` 下会空指针崩溃。
- **one-file-per-actor**（`Content/__ExternalActors__` 存在）：改已有 Actor 的位置要用 destroy + 按目标 transform 重 spawn，且 `save_current_level()` + `save_dirty_packages(True, True)` 都要调 —— 只 `save_current_level()` 会"读回是新值、重载又是旧值"。
- 关卡里 `PlayerStart` 不一定等于出生点：有 `Auto Possess Player=Player0` 的 BP 角色才是真正被占据的角色。

### 5.7 人物 / 动画 / 渲染

- 人物模型替换需先定：文件格式 / 骨架、替换目标是**体模还是第一人称手臂网格**、身高是否变化、是否保留攀爬与移动音频（见 `Docs/CharacterModelReplacement.md`）。
- 永续动画用 `URotatingMovementComponent`（tick 里按 `RotationRate` 一直转，自动接管根组件，`bEnableCollision=false` 不打扰物理），比手写 Tick 更标准、也进蓝图组件面板。

### 5.8 教训速查表

| 现象 | 根因 | 修法 |
|---|---|---|
| PIE 正常、Shipping 只触发第一条 | 触发 id 用了编辑器标签（打包后不存在）| 改 对象名 / `UiCue:xxx` Actor Tag |
| 蓝图编译 `Tried to create a property X ... already exists` | 基类组件与蓝图组件同名 | 基类不声明该组件，组件归蓝图 |
| 脚本改 Actor 位置不落盘 | one-file-per-actor | destroy + 重 spawn + 双 save |
| `-game` 里 Python 拿不到世界 | 无 world context | `load_object(None, "/Game/<map>.<map>")` |
| `-game` 里调 `get_game_world` 崩溃 | GEditor 为空 | 别用编辑器专有 API |
| `-ExecCmds=py` 无效 | 早于 Python 插件就绪 | 临时 `init_unreal.py`（用完删）|
| cook 漏资产（UiCue 表 / 配音）| 只有 AssetRegistry 扫描引用 | `+DirectoriesToAlwaysCook` |
| overlay 高亮看不到 | 网格开了 Nanite | 关 Nanite（或走 stencil）|
| 发行包日志全空 | Shipping `NO_LOGGING` | 命令行开关 + 写文件自检 |

---

## 6. 已知问题与待办

- **缺失法线贴图** `/Game/texture/concrete_wall_009_nor_gl_4k`（`M_wall3` 依赖）仍缺 → cook 告警，游戏里 `M_wall3` 用默认材质（所有机器表现一致，属统一瑕疵）。
- `IA_TimeShift` 缺失（历史遗留，待确认是否还需要）。
- 人物模型替换方案未定（见 `Docs/CharacterModelReplacement.md`）。
- `Docs/Packaging.md` 有 1 处未提交（HEAD `4978918` 之后的发布记录微调）。
- 结束游戏的"交互 → 黑屏 → 退出"目前是在编辑器 `-game` 会话里验证的；若要在**发行包内**也自动验到底，可仿照 `-UiCueSelfTest` 加一个 `-EndGameSelfTest` 开关。

---

## 7. 文档索引

| 文档 | 主题 |
|---|---|
| `Packaging.md` | 打包命令 / 发布记录 / 跨机交付 / 坑（cook、pdb、LFS）|
| `UiCue.md` | UI 活动导演模块 + §11 发行版触发 id 约定 |
| `EndGame.md` | 结束游戏接口 + 永续自转 + 组件分工 + 踩坑 |
| `InteractionImplementation.md` / `InteractionLogic.md` / `InteractionPickingFix.md` / `InteractionOperationLink.md` | 交互实现 / 逻辑 / 拾取 / 联动 |
| `StructureInteraction.md` / `InteractSelectionVisual.md` / `DoorHinge.md` | 结构交互 / 选中视觉 / 门铰链 |
| `GravityZone.md` / `TimeShift.md` / `TimeEraPortal.md` / `ProximitySequence.md` / `ProximityBarrier.md` | 重力 / 时间纪元 / 传送 / 靠近 |
| `LightReveal.md` / `LiquidLight.md` / `OverlapDemo.md` | 光揭示 / 液面光 / 重叠通道 |
| `Climb.md` / `Sprint.md` / `MovementAudio.md` / `BackgroundMusic.md` | 攀爬 / 冲刺 / 移动音频 / 背景音乐 |
| `CharacterModelReplacement.md` | 人物模型替换分析 |
| `PERFORMANCE_AND_CRASH_ANALYSIS.md` | 性能与崩溃分析 |
| `EditorImplementationGuide.md` | 文档索引 / 编辑器实施指南 |