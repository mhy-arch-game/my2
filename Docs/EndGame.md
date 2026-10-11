# EndGame —— 结束游戏（渐黑 → 退出进程）· 永续自转

> 需求：一个「结束游戏的接口」；场景里的 **`end_of_the_game`** 蓝图类是 interact 模块的可交互对象，
> 交互提示词 **「触碰」**，交互后短暂黑屏再结束游戏进程；物件**持续旋转、永续播放**。

## 1. 对外接口

| 接口 | 位置 | 用法 |
|---|---|---|
| **`EndGameWithFade(WorldContext, FadeOutSeconds=1.2, BlackHoldSeconds=0.6)`** | `UGameFlowLibrary` | **正式接口**：蓝图搜 **End Game With Fade**（分类 `MHY|Game Flow`）拖一个节点即可；重复调用自动忽略 |
| `BeginEndGame` / `CancelEndGame` / `IsEnding` / `GetFadeAlpha` + 事件 `OnEndGameSequenceStarted` | `UEndGameSubsystem`（GameInstanceSubsystem） | 想自己接前后逻辑（停音乐、停 UiCue）时用 |
| `TriggerEndGame` / `GetInteractableComponent` / `GetBodyMesh` | `AEndOfTheGameActor` | 关卡物件的 C++ 基类 |
| `SetSpinning(bool)` / `SetSpinRate(FRotator)` / `IsSpinning()` / `GetSpinComponent()` | `AEndOfTheGameActor` | 自转开关 / 变速 |

流程：**触发 → 全屏渐黑（默认 1.2s）→ 保持全黑（默认 0.6s）→ 结束游戏进程**。

- 渐黑期间冻结玩家移动 / 视角输入，并忽略重复触发；
- 黑屏是一张 Slate `SImage` 直接压进视口（ZOrder 5000），交互提示与 UiCue 字幕都会被它盖住 —— 是"真空屏"；
- 退出用 `QuitGame`，另有 1 秒兜底：若还活着就 `FPlatformMisc::RequestExit(true)`；
- 取不到视口时（`-nullrhi` / 服务器）跳过黑屏，流程照常走完。

## 2. 场景里的物件（组件分工 —— 这是本次编译失败的教训）

| 项 | 值 |
|---|---|
| 蓝图类 | **`/Game/bclass_source/end_of_the_game`**（原本就存在；父类已从 `Actor` 改为 `AEndOfTheGameActor`）|
| 运行时类名 | `end_of_the_game_C` |
| 关卡实例 | `firstvision` 里标签 `end_of_the_game` 的那个实例（**原本就在场景里**，位置保持它原位 `(-20040, -665, -2890)`）|

| 组件 | 谁提供 | 说明 |
|---|---|---|
| `StaticMesh` | **蓝图 `end_of_the_game`** | 本体外观 + 碰撞（交互射线要打得中）|
| `Interactable` | **蓝图 `end_of_the_game`** | interact 模块的可交互组件，提示词 **触碰**（已写进关卡实例数据）|
| `EndGameSpin` | **C++ 基类** | `URotatingMovementComponent`，永续自转 |

★ **基类故意不声明 `Interactable` / `StaticMesh` 这类组件** —— 蓝图里已经有同名组件，基类再创建一个同名的，
蓝图编译会直接失败（见 §5.1）。**组件归蓝图 / 实例所有，基类只提供行为**：

1. 找到物件上的 `UInteractableComponent`（一个都没有时自动补一个；提示词为空则填 `FallbackInteractPrompt`＝「触碰」）；
2. 绑定 `OnInteractRequested` → 走结束流程；
3. 驱动 `EndGameSpin`；
4. 若物件连一个"开着碰撞的图元"都没有，自动补一个方块（`/Engine/BasicShapes/Cube`，作为硬引用已保证进包）。

可调项（物件详情）：`FadeOutSeconds`(1.2) / `BlackHoldSeconds`(0.6) / `FallbackInteractPrompt` / `SpinRate`(绕 Z 45°/s) / `SpinInLocalSpace` / `AutoSpin`。

## 3. 永续自转

| 项 | 值 |
|---|---|
| 实现 | 引擎自带的 **`URotatingMovementComponent`**（组件名 `EndGameSpin`）：tick 里按 `RotationRate` 一直转 |
| 默认 | 绕 Z 轴 **45°/s**、本地空间；`bAutoActivate=true`，一上场就转 |
| 接根组件 | `UMovementComponent` 的 `bAutoRegisterUpdatedComponent=true` 会自动接管根组件，**不需要手动接** |
| 碰撞 | 走 `MoveUpdatedComponent` 且 `bEnableCollision=false`，自转不会把玩家或别的物件挤开 |

## 4. 实测（真 `-game` 会话）

harness：`Scripts/runtime_endgame_test.py`（把角色送到物件旁 → 采样自转 → 对准 → 读聚焦目标与提示词 → `TryInteract()`）。

```
t=0.5 找到目标 end_of_the_game_C loc=(-20040.0,-665.0,-2890.0)
  可交互组件=InteractableComponent 提示词=[触碰] enabled=True
t=0.5 自转采样 yaw=37.3
t=1.5 自转采样 yaw=77.3
t=2.5 自转采样 yaw=122.4
t=3.5 自转采样 yaw=167.9
每秒 yaw 增量: [46.0, 45.1, 45.5] -> 持续旋转 OK
t=6.0 距目标 242cm focused=end_of_the_game_C_0 prompt=[触碰]
t=6.0 已聚焦，调用 TryInteract()
（进程自行退出，exitcode=0）
```

对应的游戏日志（`-game` 下 `UE_LOG` 有效）：

```
[EndGame] 物件 end_of_the_game 被 BP_FirstPersonCharacter_C_1 交互，开始结束流程。
[EndGame] 开始结束流程：渐黑 1.20s + 全黑保持 0.60s。
[EndGame] 结束游戏进程。              （1.8 秒后）
FPlatformMisc::RequestExit(0, UGameEngine::HandleExitCommand)
Engine exit requested
```

## 5. 踩坑记录

### 5.1 蓝图组件与基类组件**不能同名**（本次编译失败的根因）

现象：`end_of_the_game` 蓝图编译失败

```
LogBlueprint: Error: Internal Compiler Error: Tried to create a property Interactable in scope SKEL_end_of_the_game_C,
but another object already exists there.   （KismetCompilerMisc.cpp）
蓝图编译失败： end_of_the_game
```

原因：蓝图自己声明了名为 **`Interactable`** 的组件，而 C++ 基类（旧版）也 `CreateDefaultSubobject` 了一个同名组件 ——
同一个作用域里同一个属性名只能有一个，蓝图侧创建失败。

修法（已落地）：**基类不声明这些组件**，只提供行为，组件全部由蓝图 / 实例提供（需要在运行时补齐的用 `NewObject` + `RegisterComponent`）。

> 通用规则：C++ 基类里的 `CreateDefaultSubobject` 名字（`Interactable` / `StaticMesh` / `Body` …）不要用"大家都爱这么叫"的名字，
> 要么让蓝图提供，要么加前缀（本次自转组件就叫 `EndGameSpin`）。

### 5.2 本关卡是 one-file-per-actor（`Content/__ExternalActors__`）

用 Python 对**已有实例**调 `set_actor_location` 改位置：`save_current_level()` 返回 True、同进程读回也是新值，
**重新加载后仍是旧位置**。可靠做法是 destroy + 按目标 transform 重 spawn，并把 `save_current_level()` 与
`save_dirty_packages(True, True)` 都调上。

### 5.3 `-game` 会话里不要碰编辑器专有 API

`unreal.EditorLevelLibrary.get_game_world()` 会直接 `EXCEPTION_ACCESS_VIOLATION` 崩进程（`GEditor` 为空）。
取世界用 `unreal.load_object(None, "/Game/FirstPerson/firstvision.firstvision")`；玩家用
`GameplayStatics.get_player_pawn(world, 0)`，相机用 `get_player_camera_manager(world, 0)`。

### 5.4 往 `-game` 会话注入 Python

`-ExecCmds=py <file>` 会跑在 Python 插件就绪之前（实测无效）；可靠做法是临时写 `Content/Python/init_unreal.py`
（插件初始化时自动执行），**验证完必须删掉**。

### 5.5 关卡里的 `PlayerStart` 不是玩家出生点

玩家是由关卡里 `BP_FirstPersonCharacter`（`Auto Possess Player = Player0`）直接占据的 —— 调试验证时别拿 PlayerStart 当角色位置。

### 5.6 Shipping 包没有日志

`USE_LOGGING_IN_SHIPPING=0`，`UE_LOG` 全被编掉；现场验证要走「命令行开关 + 写文件」，
做法见 `Docs/UiCue.md` §11 的 `-UiCueSelfTest`。