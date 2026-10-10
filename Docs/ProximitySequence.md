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
| `SlowSpeedMultiplier` | 0.4 | **慢速倍率（暴露出来供实测）**：区域内水平速度上限 = 当时的 `MaxWalkSpeed` × 本倍率 |
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
- 实现方式：**每帧把玩家的水平速度钳到 `MaxWalkSpeed × SlowSpeedMultiplier`**（只改速度、不写任何持久状态），
  所以离开区域自动恢复，也**不会和疾跑抢 `MaxWalkSpeed`**——疾跑在区域内只是上限高一点，照样被限速；
- 墙**变成实体之后**（`bBlocked`）就不再限速，避免贴着墙走路一直慢；
- 倍率和半径都在 Details 里暴露，**具体数值由你实测决定**。

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

慢速区两墙都是 `bSlowPlayerNearby=✔ / SlowZoneDistance=900 / SlowSpeedMultiplier=0.4`（待实测调整）。

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

> 运行时验证需要在 PIE 里做（headless 命令程没有玩家控制器，距离触发跑不起来）。
> PIE 时看日志关键字 `[ProximitySequence]`：进入触发距离会打"首次进入距离触发…"，
> 序列播完会打"已显形 + 开启碰撞（阻挡道路）"。

- `GetProximitySequenceDebugString()` 一行状态（含 distance / triggered / blocking）；
- 日志关键字 `[ProximitySequence]`（初始状态、触发、播完都会各打一行）；
- 想立刻看效果：蓝图里调 `TriggerNow()`。

## 7. 注意

- 触发状态是**会话内**的（PIE 重开会重新计一次），不落盘；
- **防卡墙靠慢速区**（见 2.5 节）：靠近时先把玩家速度压下来，别让人在墙落地那一刻站在墙里。
  若实测仍会卡住，再考虑"落点有人就延后阻挡"；
- `bApplyInitialStateOnBeginPlay` **默认关**：墙在关卡里摆的就是"可见 + 有碰撞"，组件只在动画播完后把
  "显形 + 开碰撞"再坐实一次（防止中途被别的联动隐藏掉）。