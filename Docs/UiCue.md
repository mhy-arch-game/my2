# UiCue —— UI 活动导演模块（全局 · 模块自驱 · DataAsset 驱动）

> 需求：一个 UI 模块，其接口能接收一系列事件（关卡运行时间、到达某个位置），
> 在对应事件触发时在**摄像机视角**触发 UI 事件（例如显示对应句子并播放配音）。

本模块按已确认的设计落地：**模块自驱**、触发源目前只做**盒体触发体积**（触发后永久失效），
字幕是**屏幕字幕**，长文本**自动断句按次序播放**，多条 cue 走**事件队列**（播完隔 N 秒再播下一条），
全局**唯一一份 DataAsset** 配置，**不做并发**，时间轴触发与其它触发源**本期不新增**。

## 1. 结构

```
关卡里的 AUiCueTriggerVolume（盒体，填 TriggerId）
        │ 玩家进入盒子（几何包含判定）
        v
UUiCueSubsystem（GameInstanceSubsystem，核心 ticker 自驱）
        │ 命中 -> 永久失效 + cue 入队
        │ 队列：一条播完 -> 等 QueueGapSeconds（默认 3 秒）-> 播下一条
        │ 一条 cue 内的长文本 -> 按断句符切成小句，按次序推送
        v
IUiCuePresenter（实现它的 Actor）  或  内置屏幕字幕 + 自己播配音
```

## 2. 三步接线

1. **摆触发体积**：Place Actors 搜 `UI Cue Trigger Volume` → 拉成你要的范围 → 填 `TriggerId`（如 `intro_01`）；
2. **填 cue 表**：打开 `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet` → `Cues` 加一条：
   - `CueId`：唯一名（调试与队列用）
   - `TriggerId`：与体积一致
   - `Text`：长文本（可多句，会按 `SegmentDelimiters` 切开）
   - `Voice`：配音（**一条 cue = 一段长文本 + 一段长音频**；音频时长决定字幕总流程）
   - `bOnce`：默认 ✔（触发一次后永久失效）
3. **（可选）换自己的字幕样式**：做一个 Actor 实现 **`UiCuePresenter`** 接口，实现三个事件；
   一旦有实现者，模块就不再使用内置字幕。

## 2.5 两种触发方式

| `FUiCue::Trigger` | `TriggerId` 填什么 | 触发时机 |
|---|---|---|
| **`BoxVolume`（盒体体积）** | 关卡里 `UI Cue Trigger Volume` 的 `TriggerId` | 玩家进入盒子 → 立刻触发，体积**永久失效** |
| **`InteractFirstToggle`（可交互物首次切换状态）** | **可交互物在关卡里的名字 / 标签**（如 `men1`、`jiguandeng1`） | 复用 interact 模块：该物体**第一次**开 / 关状态变化时触发一次，之后不再触发 |

**`InteractFirstToggle` 的两个前提**（都是 interact 模块本来的要求）：

1. 物体要真的**有状态**：`Interactable` 组件上 `bUseBuiltInToggle` / `bToggleLights` / `bTrackOpenState` **至少开一个**，
   否则 `SetOpen()` 会直接返回、状态不变、事件也不会发（见 `Docs/InteractionImplementation.md` 9.1）；
2. "第一次切换"**不限于玩家按 E**：任何让状态变化的来源（玩家交互、联动、蓝图 `SetOpen`）都算。

---

## 3. DataAsset 参数（都在详情面板里改，不写死在代码里）

| 字段 | 默认 | 说明 |
|---|---|---|
| `QueueGapSeconds` | **3.0** | 上一条 cue 播完后，等多少秒再播队列里的下一条 |
| `DefaultSegmentSeconds` | 2.5 | 每条小句的默认显示时长（cue 上可单独覆盖） |
| `SegmentDelimiters` | `。！？!?.;；换行` | 断句符；标点**保留**在小句末尾 |
| `bUseBuiltInSubtitle` | ✔ | 没有表现层时用内置屏幕字幕 |
| `SubtitleBottomOffset` | **30**（已下调到屏幕底端；0 = 紧贴底边）| 内置字幕离屏幕底边的像素距离 |
| `bModulePlaysVoice` | ✔ | 配音由模块播放（不切断）。若表现层自己播配音就关掉，避免播两遍 |

### 单条 cue（`FUiCue`）

| 字段 | 说明 |
|---|---|
| `CueId` / `TriggerId` | 唯一名 / 与体积匹配 |
| `Text` | 长文本，播放时断句 |
| `Voice` | 配音（软引用，可留空） |
| `SegmentSecondsOverride` | >0 时覆盖 `DefaultSegmentSeconds` |
| `bOnce` | 触发一次后永久失效 |

## 3.5 时间规则（配音与字幕的总流程一致）

一条 cue = **一段长文本 + 一段长音频**（1:1），因此：

| 情况 | 每条小句的显示时长 | 结果 |
|---|---|---|
| cue 填了 `Voice` | **音频时长 ÷ 小句数** | 字幕播完 ≈ 配音播完（总流程一致）|
| 没有 `Voice` | `SegmentSecondsOverride`，否则 `DefaultSegmentSeconds` | 按小句时长累加 |

- **不切断配音**：短句切换、cue 结束都不会 Stop 音频；即使音频略长于字幕也让它自然播完；
- 配音默认由**模块**播放（`bModulePlaysVoice = ✔`）；若你的表现层自己播配音，把它关掉，避免播两遍；
- 实现上取 `USoundBase::GetDuration()` 作为总时长；拿不到时长（例如过程式音源返回 0）时自动退回按小句时长累加；
- `bOnce` 的"已触发"只在本次运行内有效，**不落盘**（按你的要求）。

---

## 4. UI 活动接口 `IUiCuePresenter`（3 个事件）

| 事件 | 时机 | 参数 |
|---|---|---|
| `OnUiCueBegin` | 元事件开始 | `CueId`、配音（可空） |
| `OnUiCueSegment` | 每条小句 | `FUiCueSegment`（文字 / 第几句 / 共几句 / 时长） |
| `OnUiCueEnd` | 元事件结束 | `CueId` |

## 4.5 两个最简单的实例（照抄即可）

### 例 A：走进某个区域播一段旁白（盒体体积）

1. 关卡里 Place Actors 搜 **`UI Cue Trigger Volume`** → 拖到位置、拉成你要的范围；
2. 选中它 → Details → `Trigger Id` 填 **`intro_01`**；
3. 打开 `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet` → `Cues` 点 `+`：

| 字段 | 值 |
|---|---|
| `Cue Id` | `intro_01`（唯一即可，调试用）|
| `Trigger` | `Box Volume (盒体体积)` |
| `Trigger Id` | `intro_01`（**必须和体积上的一致**）|
| `Text` | 例如"风从破损的窗格里灌进来。"（多句会自动断句、按顺序播）|
| `Voice` | 这段旁白的音频（可留空：只出字幕）|

4. **PIE**：走进盒子 → 字幕按句播、配音播完；**这个体积之后永久失效**，再走进去不会重复播。

### 例 B：第一次打开某扇门时播一段（可交互物首次切换）

1. 先确认那扇门的名字 / 标签（例如 **`men1`** 的 `active_door`）；
2. DataAsset 里再加一条 cue：

| 字段 | 值 |
|---|---|
| `Cue Id` | `door_men1_first` |
| `Trigger` | **`Interactable First Toggle (首次切换状态)`** |
| `Trigger Id` | **`men1`**（物体在关卡里的名字或标签，**不要填体积的 id**）|
| `Text` | 例如"门轴发出一声闷响。铰链是新的。" |
| `Voice` | 对应配音（可留空）|

3. **PIE**：对准 `men1` 按 E（**第一次**状态变化）→ 触发；之后无论再开多少次都不会重复。

> `men1` 用的是内置铰链开关（`bUseBuiltInToggle = ✔`）⇒ 满足"有状态"的前提 ✓。
> 若换成"只有蓝图效果、没有开关状态"的门，先给它勾 `bTrackOpenState`（见 9.1）。

### 这两个例子的排查

| 现象 | 原因 |
|---|---|
| 走进盒子没反应 | `TriggerId` 与体积不一致；或 `Trigger` 选错；或该 cue 已经"永久失效"过了 |
| 交互了但没触发 | `TriggerId` 填的不是物体的名字 / 标签；或该物体**没有状态**（三个开关全关）；或它**已经**切换过一次（首次已用掉）|
| 两条 cue 撞在一起 | 按**触发顺序**排队，间隔 `QueueGapSeconds`（默认 3 秒，DataAsset 里改）|
| 想看现在在播什么 | `UUiCueSubsystem::Get(WorldContext)->GetUiCueDebugString()`，或看日志 `[UiCue]` |

### 本项目已内置一个「立刻可见」的演示

不想留就删掉它们，两处：

| 位置 | 内容 | 效果 |
|---|---|---|
| `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet` → `demo_spawn` | 盒体触发，`TriggerId = demo_spawn`，3 句演示文案，`bOnce = ✔` | 配合下面那个体积 |
| 关卡 `firstvision` → 名为 **`UiCueDemo_Volume`** 的体积 | 就放在 **PlayerStart 上**（位置 (0,0,3182)，范围 600×600×600），`Trigger Id = demo_spawn` | **一进 PIE 立刻出字幕**（出生就在盒子里），3 句 × 2.5 秒依次播 |
| `DA_UiCueSet` → `demo_men1` | 可交互物首次切换，`TriggerId = men1`（那扇门） | **第一次**开关 `men1` 时出字幕，之后不再触发 |

- 两条都是 `bOnce` ⇒ 每次 PIE 各触发**一次**；
- 演示 cue **没有配音**（现有音频只有 3 分半的背景音乐，配上会把字幕拖到 3 分半）——
  想加配音就给 cue 的 `Voice` 选一段音频，字幕总时长会自动按音频长度均分（见 3.5）；
- 改成你自己的文案 / 音频、或直接删掉这两条 cue 与那个体积，都不需要改代码。

---

## 5. 蓝图 API / 调试

| 接口 | 用途 |
|---|---|
| `UUiCueSubsystem::Get(WorldContext)` | 取子系统 |
| `TriggerCueById(CueId)` | 手动/外部触发（**触发源后续扩展时用它**） |
| `ClearQueue()` / `SkipCurrentCue()` | 清队列 / 跳过当前 |
| `IsPlaying()` / `GetQueuedCount()` | 查询 |
| `GetUiCueDebugString()` | 一行状态（正在播什么、第几句、队列长度、间隔倒计时、表现层是谁） |
| `OnCueBegan / OnCueSegmentShown / OnCueEnded` | 蓝图事件（做额外表现） |

日志关键字 `[UiCue]`：触发入队、开始 cue、找不到匹配 cue 等都会打。

## 6. 本期明确不做（按你的要求）

- **时间轴触发**（`关卡运行时间`）：枚举里留了扩展位，暂未实现；
- 其它触发源（序列播完 / 看某物 N 秒…）：暂未实现；`InteractFirstToggle` 已实现（见 2.5 与 4.5），其它情况可先用 `TriggerCueById` 手动触发；
- **并发**：同一时刻只有一条 cue（队列保证）；**配音不会被切断**，上一条音频自然播完；
- **存档**：`bOnce` 的"已触发"状态只在本次会话内有效（不写 SaveGame）。

## 7. 实现要点（避免后人踩坑）

- 盒体判定用**几何包含**（把玩家位置变换到盒子局部空间比对范围），不依赖碰撞事件，也不影响玩家移动与交互射线；
- 体积默认用 `Trigger` 碰撞预设（只判定、不阻挡）。若要它"阻挡"，把预设改成 BlockAll —— 但那样玩家进不去，触发就无法发生；
- 断句实现保留了标点（不是 `ParseIntoArray` 的丢标点行为）；
- 字幕控件与交互提示的内置弹窗同风格：**透明底 + 白字 + 深色投影 + 自动换行**。
## 10. 正式事件表（已落地，演示内容已删除）

演示用的 `demo_spawn` / `demo_men1` 两条 cue 与关卡里的 `UiCueDemo_Volume` 触发体积**都已删除**；
现在 `DA_UiCueSet` 里是下面 9 条正式元事件（配音全部来自 `Content/sound_resources`）：

| CueId | 触发 | TriggerId | 配音（按数组顺序连播）|
|---|---|---|---|
| `ui_opening` | `GameStart` | — | `opening` / `opening2` / `opening3` |
| `ui_kongjian1` | 首次交互 | `kongjianchuansuoqi1` | `kongjianzhuanhuanqi1` / `kongjianzhuanhuanqi1_2` |
| `ui_kongjian2` | 首次交互 | `kongjianchuansuoqi2` | `kongjianzhuanhuanqi2` |
| `ui_men1` | 首次交互 | `men1` | `men1` |
| `ui_men2` | 首次交互 | `men2` | `men2` |
| `ui_men3` | 首次交互 | `men3` | `men3` |
| `ui_men21` | 首次交互 | `men21` | `men21` / `men21_2` |
| `ui_jiguanqiang1` | 首次靠近 600cm | `jiguanqiang1` | `LS12` |
| `ui_jiguanqiang2` | 首次靠近 600cm | `jiguanqiang2` | `LS12` |

### 配音 / 字幕的时长规则（一条 cue 多段配音）

- `FUiCue::Voices` 是**有序数组**，模块按顺序**连播**：上一段 `OnAudioFinished` 才播下一段，不重叠、不切断；
- **配音段数 == 小句数** 时，每句就用它自己那段配音的时长（最准）；
  否则退回"配音总时长 / 小句数"，保证字幕总流程与配音总流程一致；
- 于是"同一个元事件里的多句短句"用**一条 cue + 多段音频**表达就够了，不必切成一堆 cue 靠队列间隔串起来。

### 触发源（现在共 4 种）

| 触发 | 说明 |
|---|---|
| `BoxVolume` | 玩家进入关卡里摆放的 `UiCueTriggerVolume`（触发后永久失效）|
| `InteractFirstToggle` | 某个可交互物**首次交互**（**按下交互键** 或 **状态变化**，谁先算谁）|
| `GameStart` | 进入游戏即触发（世界开始 + 本地玩家就绪后发一次）|
| `ProximityToActor` | 首次靠近某个 Actor 到 `ProximityDistance`（cm）内；距离 = 玩家到该 Actor 包围盒最近点 |

> `InteractFirstToggle` 之所以同时监听"按下交互键"：`kongjianchuansuoqi*` 这类物体
> `bUseBuiltInToggle / bTrackOpenState / bToggleLights` 全关，**没有内置状态**，
> 不会广播 `OnToggleChanged`，只靠状态变化的话永远触发不了。

### 运行时实测（headless `-game`）

```
[UiCue] 进入游戏 -> cue 'ui_opening' 入队（队列 1）。
[UiCue] 已绑定 41 个可交互物（首次交互触发）。
[UiCue] 播放配音 1/3：opening
[UiCue] 开始 cue 'ui_opening'（3 句，每句时长 [2.3 3.1 2.6]秒，配音 3 段，表现层=内置字幕）。
[UiCue] 播放配音 2/3：opening2        (+3.7s)
[UiCue] 播放配音 3/3：opening3        (+3.4s)
```

逐句时长 2.3 / 3.1 / 2.6 正好等于 `opening` / `opening2` / `opening3` 三段音频各自的时长 —— 规则生效。

### 新增 cue 的做法

1. 内容浏览器打开 `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet`；
2. `Cues` 里加一条：填 `CueId`（唯一）、`Trigger`、`TriggerId`（物体在关卡里的**标签**）、`Text`（多句用 `.` 断开）、
   `Voices`（同一命名带数字标号的配音按数字顺序依次添加）；
3. `ProximityDistance` 只对"首次靠近"有效，默认 600cm。
### 字幕位置（2026-10 调整）

字幕已移到**屏幕底端**：`DA_UiCueSet` 的 `SubtitleBottomOffset` = **30**（px，距屏幕底边）。
控件锚点是屏幕底部居中（anchors `(0.5, 1.0)` + alignment `(0.5, 1.0)`），所以这个值就是"离底边多少像素"，
想让字幕更贴边就调小（0 = 紧贴底边），想抬高就调大。改完直接保存 DataAsset 生效，不用重编译。

