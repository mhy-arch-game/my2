# ProximitySequence —— 首次靠近播放关卡序列（机关墙：播完显形 + 挡路）

> 需求：`jiguanqiang1` / `jiguanqiang2` 功能基本相同，各自在**首次到达墙体一定距离内**触发动画
> （动画资产分别是 `LS_jiguanqiang1` / `LS_jiguanqiang2`）；**动画播完后墙体向前移动挡住道路，
> 并开启碰撞与显形**。实现基于既有功能代码（延续 `UProximityBarrierComponent` 的"距离 + 一次性"思路）。

## 0. 本关卡实测到的现状（写文档时的真实数据）

| 项 | 实测值 |
|---|---|
| `LS_jiguanqiang1` | 0–150 帧 @30fps = **5.00 秒**；绑定 actor **`jiguanqiang1`**（StaticMeshActor）|
| 轨道内容 | 一条 `MovieScene3DTransformTrack`：`Location.X` **−1500 → −1800（向前 300cm）**，Y/Z、旋转、缩放不变 |
| `LS_jiguanqiang2` | 0–150 帧 @30fps = 5.00 秒；绑定 **`jiguanqiang2`**；位移同样是 X −1500 → −1800 |
| 关卡里的 `LS_jiguanqiang1/2` Actor | 是**空壳**（`Sequence = None`、不自动播放），本方案不使用，可删 |
| `jiguanqiang1` | StaticMesh + `Interactable`(bEnabled=false) + `ProximitySequence`（**原来的 Receiver 已按需求移除**）|
| `jiguanqiang2` | StaticMesh + `ProximitySequence`（**无任何联动组件**）|
| 当前外观 | 两堵墙此刻都是**可见 + 有碰撞**（`BlockAllDynamic`）|
| 组件挂载 | `jiguanqiang1` / `jiguanqiang2` 各有一个 `ProximitySequence` 实例组件（已保存并跨进程复验）|
| 隐形阻挡墙 | `yinxingqiang1` / `yinxingqiang2`（复用墙体网格 + `InvisibleWall` 配置 + `bHiddenInGame`），位于动画终点 −1800 |

### 联动关系（谁让墙消失）

```
玩家按 E 点亮机关灯            灯的链接组件              广播频道            接收方（墙）
jiguandeng3  (bToggleLights) -> Open  -> "light_group3"  ->  jiguanqiang1  -> SetActorHidden（隐藏 + 关碰撞）
jiguandeng2                  -> Open  -> "light_group2"  ->  jiguanqiang4  -> SetActorHidden
jiguandeng1                  -> Open  -> "door_light_group1" -> jiguanqiang0 -> SetActorHidden
men20                        -> Open  -> "door_group1"   ->  jiguanqiang3  -> SetActorHidden
```

也就是**「灯亮 → 对应那堵墙消失」**（`SetActorHidden` 会连带关掉碰撞）。

> ⚠️ 这两面墙**不参与上述联动**（需求确认）：`jiguanqiang1` 原先挂在频道 `light_group3` 上的
> `InteractionOperationReceiver` **已被删除**，`jiguanqiang2` 从来就没有。所以 `jiguandeng3` 现在照样广播
> `light_group3`，但**没有任何接收方**（空广播，无害）。两面墙**唯一**的交互就是"主控角色靠近 → 播动画"。

## 1. 组件字段

`UProximitySequenceComponent`（`Source/MHY_ARCH_GAME/ProximitySequence/`）

| 字段 | 默认 | 说明 |
|---|---|---|
| `Sequence` | 空 | 要播放的关卡序列：`/Game/LS_jiguanqiang1`、`/Game/LS_jiguanqiang2` |
| `SequenceActor` | 空 | 可选：直接驱动关卡里现成的 `LevelSequenceActor`；留空则新建播放器（推荐）|
| `TriggerDistance` | 600 cm | 目标到**本体包围盒最近点**的距离 ≤ 它就算"到达"。**数值由关卡实测调整** |
| `bOnce` | ✔ | 只触发一次（首次到达）|
| `bApplyInitialStateOnBeginPlay` | ✗ | **默认关**：墙原本就是"可见 + 有碰撞"，组件不去改初始状态；打开则在 BeginPlay 先设成不可见 + 关碰撞 |
| `bBlockOnFinish` | ✔ | 序列播完后**显形 + 开碰撞**，坐实"挡住道路" |
| `bLoop` | ✗ | 循环播放（循环时不会结束，也就不会进入阻挡态）|
| `bSlowPlayerNearby` | ✔ | 靠近时限速（见下节"慢速区"）|
| `SlowZoneDistance` | 900 cm | 慢速区半径（到墙体包围盒最近点）；≤0 = 直接用 `TriggerDistance` |
| `SlowSpeed` | **250 cm/s** | **区域内减速到的目标速度（暴露出来供实测）**：每帧把玩家水平速度钳到不超过它 |
| `bDebugLogSlowZone` | ✗ | 调试：限速时每帧打一行日志（会刷屏）|
| `PlayRate` | 1.0 | 播放速率 |
| `DebugLogRange` | 0 | >0 时接近过程中打日志，方便调触发线 |

接口：`TriggerNow()`、`ResetTrigger()`、`IsTriggered()`、`IsBlocking()`、`GetDistanceToTarget()`、
`GetProximitySequenceDebugString()`；事件：`OnTriggered(TriggeredBy, Distance)`、`OnSequenceFinished`。

## 2. 距离怎么算

**目标位置 → 本体包围盒最近点**的距离（`FBox::GetClosestPointTo`）：墙再大也是"贴到墙面 N 厘米"的字面意思，
上下方向同样算。目标取**本地控制器的 Pawn**（玩家）。注意：本体不可见时包围盒依然存在，所以"先不可见、
靠近才出现"的顺序不会影响触发。

## 2.5 慢速区（防"卡进墙里"）

墙要靠 5 秒动画才移到位；玩家若全速冲过来，很可能动画结束时**正好站在墙的落点上**，一开碰撞就被卡住。
所以在墙周围放一圈**慢速区**：

- 进入 `SlowZoneDistance`（默认 900cm，比触发距离 600cm 稍大，等于**提前减速**）就开始限速；
- 实现方式：**每帧把玩家的水平速度钳到不超过 `SlowSpeed`（当前 250 cm/s）**（只改速度、不写任何持久状态），
  所以离开区域自动恢复，也**不会和疾跑抢 `MaxWalkSpeed`**——疾跑在区域内同样被钳到 250；
- 墙**变成实体之后**（`bBlocked`）就不再限速，避免贴着墙走路一直慢；
- 目标速度和半径都在 Details 里暴露（角色平时 `MaxWalkSpeed` 约 500，250 ≈ 半速），**具体数值由你实测决定**。

## 3. 完整时序

```
BeginPlay   bApplyInitialStateOnBeginPlay（默认关）?  SetActorHiddenInGame(true) + SetActorEnableCollision(false)
            默认关时墙保持关卡里的原样：可见 + 有碰撞
每帧 tick    距离 = 玩家 -> 墙体包围盒最近点        （> TriggerDistance 就继续等）
            距离 <= TriggerDistance  ?  PlaySequence()   [bOnce 记一次，之后停 tick]
PlaySequence  ULevelSequencePlayer::CreateLevelSequencePlayer(...)
              Settings.FinishCompletionStateOverride = ForceKeepState   <-- 关键：别把动画的位置还原回去
              绑定 OnFinished -> HandleSequenceFinished()
OnFinished   bBlockOnFinish ? SetActorHiddenInGame(false) + SetActorEnableCollision(true)   => 挡路
```

**位移由动画资产负责**（`LS_*` 里的 300cm 关键帧）；C++ 只负责"显形 + 开碰撞 + 保持动画终态"。
`ForceKeepState` 这一步很重要：不设置的话，序列可能按 section 的完成模式把墙还原到播放前的位置，
"向前移动"就白播了。

## 4. 接线状态（本关卡已完成并复验）

已用 `SubobjectDataSubsystem` 给两面墙各挂一个实例组件（组件名 `ProximitySequence`）并保存关卡，
再**换新进程**从磁盘复验通过：

| Actor | 组件 | Sequence | TriggerDistance | bOnce | bBlockOnFinish | bApplyInitialStateOnBeginPlay |
|---|---|---|---|---|---|---|
| `jiguanqiang1` | `ProximitySequence`（+ 哑 Interactable）| `/Game/LS_jiguanqiang1` | 600（沿用默认，待实测）| ✔ | ✔ | ✗ |
| `jiguanqiang2` | `ProximitySequence` | `/Game/LS_jiguanqiang2` | 600（沿用默认，待实测）| ✔ | ✔ | ✗ |

慢速区两墙都是 `bSlowPlayerNearby=✔ / SlowZoneDistance=900 / SlowSpeed=250 cm/s`（待实测调整）。

`jiguanqiang0/3/4` 未改动。两面墙原本就是**可见 + 有碰撞**，所以组件默认不去改初始状态。

要在别的墙上重做：选中墙 → Details → **Add Component → `Proximity Sequence`** → 填 `Sequence` → 按实测调 `TriggerDistance`。

`jiguanqiang2` 若要参与"灯亮墙消失"的联动，还需要补一个 `InteractionOperationReceiver`（频道按上表），
组件本身不负责这件事。

## 5. 与既有系统的关系

| 既有系统 | 关系 |
|---|---|
| `UProximityBarrierComponent` | 同为"距离 + 一次性"，但它管**显形 / 封路**；本组件管**播序列 + 播完坐实阻挡** |
| `UiCue` 模块 | 那是**字幕 / 配音**的事件队列；本组件不产生 UI |
| 机关灯 `InteractionLink` + 接收器 | 不冲突：那是"玩家主动开灯 → 墙消失"，本组件是"玩家靠近 → 墙出现" |
| 关卡里的空壳 `LevelSequenceActor` | 不使用（`Sequence` 为空），可删 |

## 6. 调试

- `GetProximitySequenceDebugString()` 一行状态（含 distance / triggered / blocking）；
- 日志关键字 `[ProximitySequence]`：BeginPlay / 触发 / 播完 / 开碰撞各一行，**"开碰撞"那行会直接报告
  actor 级与每个图元组件的碰撞状态**（复验"墙到底挡不挡路"最快的方式）；
- `bDebugLogSlowZone` 打开后，限速时每帧打一行；
- 想立刻看效果：蓝图调 `TriggerNow()`，或在控制台（PIE）输入 `ps.AutoTrigger 3` = 关卡开始 3 秒后自动
  触发一次（无玩家的 headless 环境也能驱动整条链路）。注意 `-ExecCmds` 里设不了它——命令执行早于游戏
  模块注册该 CVar（日志会显示 `ps.AutoTrigger = "0" LastSetBy: Constructor`）。

## 7. 注意

- 触发状态是**会话内**的（PIE 重开会重新计一次），不落盘；
- **防卡墙靠慢速区**（见 2.5 节）：靠近时先把玩家速度压下来，别让人在墙落地那一刻站在墙里。
  若实测仍会卡住，再考虑"落点有人就延后阻挡"；
- `bApplyInitialStateOnBeginPlay` **默认关**：墙在关卡里摆的就是"可见 + 有碰撞"，组件只在动画播完后把
  "显形 + 开碰撞"再坐实一次（防止中途被别的联动隐藏掉）。

## 8. 运行时实测与"动画播完没碰撞"的根因

用一条 headless 真实游戏会话验过整条链路（`-game -nullrhi -DDC-ForceMemoryCache`，无窗口、无玩家也能跑）：

```
首次进入距离触发（2941.4cm ≤ 100000cm），开始播放序列      ← 第一次 tick 就点火
序列播放结束                                              ← 正好 +5.0 秒，OnFinished 有回调
已显形 + 开启碰撞（阻挡道路）。actorCollision=1，图元 1 个：
    StaticMeshComponent0=ECollisionEnabled::QueryAndPhysics/BlockAllDynamic
```

### 为什么"动画播完墙没碰撞"——SetActorEnableCollision 是空操作

`AActor::SetActorEnableCollision` 的实现（`Engine/Source/Runtime/Engine/Private/Actor.cpp:5267`）：

```cpp
void AActor::SetActorEnableCollision(bool bNewActorEnableCollision)
{
    if (bActorEnableCollision != bNewActorEnableCollision)   // ← 没变化就直接 return
    {
        bActorEnableCollision = bNewActorEnableCollision;
        for (UActorComponent* Comp : Components) Comp->OnActorEnableCollisionChanged();
    }
}
```

它**只翻 actor 级开关，不会去改组件的 `CollisionEnabled`**。而本项目的显隐操作恰恰是走 actor 级开关的
（`InteractionOperationReceiverComponent`：`SetActorHidden` → `Owner->SetActorEnableCollision(!bActive)`）。
所以只调一句 `SetActorEnableCollision(true)` 并不足以稳定地把"能挡路"坐实。

**修法** —— `ApplyBlockedState()` 现在做四件事：

1. `SetActorHiddenInGame(false)`；
2. `SetActorEnableCollision(true)`；
3. **逐个图元组件** `SetCollisionEnabled(...)`，恢复"播放前快照"（`PlaySequence()` 里的
   `SnapshotCollisionState()` 记录），快照拿到 `NoCollision` 就按 `QueryAndPhysics` 兜底；
4. 打一行日志把 actor 级 + 每个图元的碰撞状态/配置写出来，方便每次复验。

另外加了**兜底**：万一运行时创建的播放器没回调 `OnFinished`，`PollSequenceFinished()` 会在 tick 里检测
"播放中 → 已停止"再补一次（结束处理幂等，只做一次）。

## 9. 隐形阻挡墙（世界视图里摆的、永久生效）

需求：在世界视图里、**动画结束后的墙体位置**摆一面**不可见但有碰撞阻挡**的隐形墙来模拟阻挡，
**永远存在、与互动与否无关**。

已摆好并跨进程复验（标签 `yinxingqiang1` / `yinxingqiang2`）：

| Actor | 位置 | 网格 | 碰撞配置 | 碰撞开关 | 游戏内可见 | 编辑器可见 | Mobility |
|---|---|---|---|---|---|---|---|
| `yinxingqiang1` | (-1800, -3355, 296.5) | 与 `jiguanqiang1` **同网格** | `InvisibleWall` | QueryAndPhysics | ✗（`bHiddenInGame=True`）| ✔ | Static |
| `yinxingqiang2` | (-1800, 3145, 296.5) | 与 `jiguanqiang2` **同网格** | `InvisibleWall` | QueryAndPhysics | ✗ | ✔ | Static |

要点：

- **位置 = 墙体原位置 + (-300, 0, 0)**，正好是 `LS_*` 动画的终点（−1500 → −1800），墙演完到位时与隐形墙重合；
- **直接复用墙体自己的 StaticMesh**（同网格 / 同旋转 / 同缩放），碰撞形状与可见墙**完全一致**，
  不需要去估包围盒尺寸；
- 碰撞配置用引擎自带的 **`InvisibleWall`**（`BaseEngine.ini:3122`）：
  `QueryAndPhysics` + `ObjectType=WorldStatic` + **`Visibility` 通道 Ignore** ⇒ **挡 Pawn / 物体，但不挡视线与交互射线**；
- `bHiddenInGame=True` → **游戏里不可见**；编辑器里仍然可见（方便核对位置）。要连编辑器也不显示，
  把该组件的 `bVisible` 或 Actor 的 `bHidden` 关掉即可；
- 这两个 Actor 身上**没有任何本项目的组件**，不参与触发、不参与联动，**永远存在**。