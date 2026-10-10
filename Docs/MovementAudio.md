# MovementAudio —— 移动音频接口（跳跃 / 行走 / 奔跑）

关联代码目录：`Source/MHY_ARCH_GAME/MovementAudio/`

**定位**：这是一套**音频接口（挂点）**，不是音效包。所有声音槽都是**可留空的软引用**：
- **现在**：不填任何资源也能工作——组件照常追踪移动状态并**广播事件**；
- **后续**：把 `TSoftObjectPtr<USoundBase>` 槽位填上资源即可自动播放，**无需改代码**；
- 也可以完全不填槽，只订阅事件，把声音接到 Wwise / MetaSound / 蓝图。

---

## 〇、运动状态 → 音乐（本轮新增的核心）

原有的事件是**一次性**的（脚步 / 起跳 / 落地，属于"点"）；本轮补上**持续运动状态**（"段"）：
状态一变就换音乐，并把变化**派发**给任何实现了接口的旁观者。

### 0.1 状态枚举 `EMovementAudioState`

| 状态 | 判定 | 显示名 |
|---|---|---|
| `Idle` | 水平速度 ≤ `WalkSpeedThreshold`(10) | Idle (站立) |
| `Walk` | > 10 且 ≤ `RunSpeedThreshold`(300) | Walk (行走) |
| `Run` | > 300 且 ≤ `SprintSpeedThreshold`(500) | Run (奔跑) |
| `Sprint` | > 500 | Sprint (疾跑) |
| `Crouch` | `Character::bIsCrouched` | Crouch (蹲伏) |
| `InAir` | `Movement->IsFalling()` | In Air (滞空) |

**优先级（高 → 低）：`InAir` > `Crouch` > `Sprint` > `Run` > `Walk` > `Idle`。**
滞空一律算 InAir；蹲着一律算 Crouch（蹲着不可能疾跑）；其余按水平速度分档。

> **与疾跑组件的配合**：`SprintComponent` 把 `MaxWalkSpeed` 设成 650，而 `SprintSpeedThreshold` 默认 500，
> 所以**两者不需要互相引用**就能正确识别疾跑。若你把疾跑速度改小，请同步把 `SprintSpeedThreshold`
> 调到"奔跑阈值"与"疾跑速度"之间。

### 0.2 接口 `IMovementAudioInterface`（面向外部系统，本轮新增）

| 函数 | 用途 |
|---|---|
| `CanReceiveMovementAudio(Character)` | 是否关心该角色；默认 true，想只听特定角色就在这里判断 |
| `OnMovementAudioStateChanged(NewState, PreviousState, Character)` | 状态变化 → 实现者在这里切音乐 |

**它解决的问题**：让"谁在听"和"谁在产生运动"解耦。典型实现者是**全局音乐导演**——
它不需要认识具体角色，也不用跟角色蓝图连线，只要实现接口就能收到所有角色的运动状态。

⚠️ `PreviousState` 在**第一次**进入状态时等于 `NewState`（表示"这是初始状态"），
所以实现者不必自己等第一次变化，收到就能起播正确的音乐。

### 0.3 两种接法（任选其一，拿到的是同一个事件）

| 接法 | 怎么做 | 适合 |
|---|---|---|
| **A. 接口**（推荐，解耦） | 让旁观者蓝图实现 **Movement Audio** 接口，写 `On Movement Audio State Changed` | 音乐导演、区域环境音、UI / 演出 |
| **B. 委托** | 在角色 `MovementAudio` 组件上给 **`On Movement State Changed`** 绑定事件 | 只关心这一个角色、想快速验证 |

### 0.4 操作步骤

#### 方案 A：零蓝图（组件内置的状态音乐）

1. 选中角色（`BP_FirstPersonCharacter`）上的 **`MovementAudio`** 组件。
2. Details 搜 **`state`**（分类是 `MovementAudio|States`）。
3. 展开 **`State Music`** 数组，为每个状态各加一条：
   | `State` | `Music` | `Volume Multiplier` |
   |---|---|---|
   | Idle | 站立音乐（软引用；资源需自己勾 `Looping`） | 1.0 |
   | Run | 奔跑音乐 | 1.0 |
   | Sprint | 疾跑音乐 | 1.2 |
   | … | 其它状态按需 | |
4. 没配的那些状态**不会放音乐**（只是把当前音乐淡出），这是预期行为——和原有音效槽一样的"接口先行"。
5. 完成。**保存即可**，不需要编译、不需要蓝图连线。

#### 方案 B：接口（推荐做大一点的项目）

1. 新建一个 Actor 蓝图，例如 `BP_MusicDirector`，拖进关卡。
2. 右上 **Class Settings** → **Interfaces → Implemented Interfaces → Add** →
   选 **`Movement Audio`** → **Compile**。
3. 在 **My Blueprint → Interfaces** 下会出现两个事件：
   - **`Can Receive Movement Audio`**：默认返回 true；想只听特定角色就在这里判断；
   - **`On Movement Audio State Changed`**：按 `New State` 分支切音乐
     （用 `Switch on E Movement Audio State` 节点最直观，或直接开 `==` 比较）。
     切音乐用 `Spawn Sound 2D` + 记一个 `UAudioComponent` 变量，旧的在 `Previous State` 分支里 `Fade Out`。
4. **Compile + Save**。之后关卡里所有角色的运动状态变化都会送到这个导演。

> 两种接法**可以同时用**：组件内置音乐负责"单人基础反馈"，接口导演负责"整体音乐编排"。

### 0.5 配置一览（Details 搜 `state`）

| 属性 | 默认 | 说明 |
|---|---|---|
| `WalkSpeedThreshold` | 10 | 高于它算行走 |
| `SprintSpeedThreshold` | 500 | 高于它算疾跑 |
| `bPlayMusicPerState` | ☑ | 按状态循环播放音乐 |
| `StateMusic` | 空 | 每状态的循环音乐（`State` / `Music` / `VolumeMultiplier`） |
| `MusicFadeTime` | 0.35 | 淡入淡出时长（0 = 硬切） |
| `bDispatchToInterfaceListeners` | ☑ | 把状态变化派发给接口实现者 |
| `bLogStateChanges` | ☐ | 状态变化打一行日志（排查用） |

**播放行为**：状态变化 → 旧音乐 `FadeOut(MusicFadeTime)` → 新音乐 `SpawnSound2D` + `FadeIn(MusicFadeTime)`，
即**交叉淡化**；且是 **2D 播放**（不受位置衰减），音乐本来就该是全局的。
**音乐是否循环取决于声音资源自身**（SoundWave 的 `Looping` 勾选），本系统不管。

### 0.6 调试

**查询**：`GetMovementState()`（BlueprintPure，返回当前状态）。

**一行状态**：`GetMovementAudioDebugString()` → 接 `Print String`：

```
MovementAudio BP_FirstPersonCharacter_C_0 | state=Run (奔跑) | speed2D=420 (walk>10 run>300 sprint>500) | run=yes air=no crouch=no | music=playing fade=0.35 | iface=on
```

一行里能看出四件事：**当前状态**、**当前速度与三个阈值**、**run/air/crouch 三个原始标志**、
**是否正在放音乐 + 是否派发接口**。

**故障排查表**

| 现象 | 原因 | 解法 |
|---|---|---|
| 状态一直停在 Idle | 该组件挂在非 `ACharacter` 的 owner 上（BeginPlay 已打 Warning 并停用 Tick） | 挂到角色上 |
| 该疾跑却显示 Run | `SprintSpeedThreshold` 高于实际速度 | 把它调到"奔跑阈值"与"疾跑速度"之间（默认 500 对 650 的疾跑是够的） |
| 蹲下不切 Crouch | 角色不是通过 `Crouch` 进入的（例如只把胶囊压低了） | 用 `Crouch`/`UnCrouch` 或勾 `Character Movement → Can Crouch` |
| 音乐不切换 | `bPlayMusicPerState` 被关，或该状态的 `Music` 是空的/没加载 | 检查这两项；看调试串里 `music=none/playing` |
| 音乐没有循环 | 声音资源自身没勾 `Looping` | 在 SoundWave 资源里勾上；本系统不负责循环 |
| 音乐硬切没有过渡 | `MusicFadeTime` 为 0 | 设成 0.3 左右 |
| 接口收不到事件 | 旁观者没实现接口，或没在关卡里；或 `bDispatchToInterfaceListeners` 被关 | 用 `Can Receive Movement Audio` 返回 true 自检；打开调试串看 `iface=on` |
| 换了音乐但旧的还在响 | 淡出还没结束（0.35 秒内） | 正常；想立刻停就把 `MusicFadeTime` 设为 0 |

---

## 一、类清单

| 类 | 基类 | 职责 |
|---|---|---|
| `UMovementAudioComponent` | `UActorComponent` | 挂在角色上：追踪走/跑/空中状态、广播事件、按槽位播放音效 |
| `EMovementAudioEvent` | `UENUM` | `Footstep` / `Jump` / `Land` |
| `FMovementAudioSet` | `USTRUCT` | 一套音效：`Surface`（地面材质）+ `Footstep`/`Jump`/`Land` 软引用 |
| `UAnimNotify_MovementAudio` | `UAnimNotify` | 在动画里精确触发脚步/跳跃/落地事件 |

依赖：`Build.cs` 已加入 `PhysicsCore`（读取 `UPhysicalMaterial::SurfaceType`）与 `MHY_ARCH_GAME/MovementAudio` 包含路径。

---

## 二、接口一览（后续就在这里挂资源）

### 2.1 事件（`BlueprintAssignable`，订阅式）
| 事件 | 参数 | 触发时机 |
|---|---|---|
| `OnFootstep` | `FName SurfaceName, bool bRunning` | 脚步（AnimNotify 或距离兜底） |
| `OnJump` | `FName SurfaceName` | 起跳（进入 `MOVE_Falling` 且 Z 速度 > 0） |
| `OnLand` | `FName SurfaceName, float FallSpeed` | 落地（`FallSpeed` = 本次滞空的最大下落速度，可做轻/重落地分级） |
| `OnRunStateChanged` | `bool bRunning` | 走 ⇄ 跑切换 |

> **播放策略**：事件**先广播**，再尝试播放槽位里的资源。因此即使还没接声音，逻辑侧也能拿到完整事件流。

### 2.2 触发函数（`BlueprintCallable`）
| 函数 | 说明 |
|---|---|
| `PlayFootstep()` | 脚步：按脚下材质选音效 |
| `PlayJump()` | 起跳 |
| `PlayLand()` | 落地 |
| `PlayMovementAudioEvent(Event)` | 通用入口（AnimNotify 走这个） |

### 2.3 查询（`BlueprintPure`）
`IsRunning()` / `IsInAir()` / `GetCurrentSurfaceName()` / `GetSurfaceNameAtLocation(Location)`

---

## 三、声音资源槽（后续填）

```
DefaultSet     : FMovementAudioSet     // 默认一套（未匹配到材质时使用）
SurfaceSets    : TArray<FMovementAudioSet>   // 按地面材质分套：草地 / 石地 / 金属…
VolumeMultiplier / PitchMin / PitchMax       // 音量与随机音高
Attenuation / Concurrency                    // 衰减与并发限制
```

每个 `FMovementAudioSet`：
| 字段 | 说明 |
|---|---|
| `Surface` | 对应的 `EPhysicalSurface`（在 `Project Settings → Physics → Physical Surface` 里定义名字，如 Grass/Stone/Metal） |
| `Footstep` | 脚步音（软引用） |
| `Jump` | 起跳音（软引用） |
| `Land` | 落地音（软引用） |

**地面材质判定**：从角色位置向下打一条 `SurfaceTraceDistance` 的射线（`SurfaceTraceChannel`），取命中物体的物理材质 `SurfaceType` → 匹配 `SurfaceSets`；匹配不到用 `DefaultSet`。事件里的 `SurfaceName` 就是该材质的显示名。

---

## 四、自动检测配置

| 配置 | 说明 | 默认 |
|---|---|---|
| `bAutoDetectJumpAndLand` | 用 `ACharacter::MovementModeChangedDelegate` / `LandedDelegate` 自动判定起跳与落地 | true |
| `RunSpeedThreshold` | 水平速度超过该值算"奔跑" | 300 |
| `bAutoFootstepByDistance` | **过渡用**：没接 AnimNotify 前，按行进距离自动踩点 | **true（本项目已开）** |
| `FootstepDistance` | 自动踩点的间隔距离 | 180 |
| `SurfaceTraceDistance` | 向下探测地面材质的距离 | 150 |
| `SurfaceTraceChannel` | 探测通道（需被地面阻挡） | Visibility |

**脚步时机的正式做法**：`bAutoFootstepByDistance` 关闭，改用动画里的 `AnimNotify_MovementAudio`（时机由动画决定，最自然）。

---

## 五、引擎内使用步骤

1. **重新构建工程**（新增了类与 `PhysicsCore` 依赖）。
2. 打开角色蓝图（如 `BP_ThirdPersonCharacter`）→ **Add Component → `Movement Audio`**：
   - 先不填任何声音也能跑；
   - 按需调 `Run Speed Threshold`、`bAuto Detect Jump And Land`。
3. **（过渡验证）** 想先听到节奏：勾 `bAuto Footstep By Distance`。
4. **（正式）加 AnimNotify**：
   - 打开走/跑动画（或它的 Montage）→ 在脚掌着地帧右键 **Add Notify → `Movement Audio`**；
   - 该 Notify 的 `Event` 保持 `Footstep`（也可用于 `Jump` / `Land`）；
   - 在跳跃/落地动画对应位置同样加，事件选 `Jump` / `Land`；
   - 然后把组件上的 `bAuto Footstep By Distance` 关掉。
5. **后续加声音资源**：把 `Default Set` 或 `Surface Sets` 里的三个软引用指向具体 `SoundWave`/`SoundCue` 即可，**无需改代码**。
6. 想让不同地面不同声音：先在 `Project Settings → Physics → Physical Surface` 定义材质名（如 Grass、Stone），
   再给地面材质的 `Physical Material` 指定对应 Surface，最后在组件的 `Surface Sets` 里加同名条目。

**验证**：走动/奔跑/起跳/落地时，在蓝图里订阅四个事件打印日志（或绑一个临时音效），确认事件按预期触发。

---

## 六、边界与注意事项

| 情形 | 说明 |
|---|---|
| 角色不是 `ACharacter` | 组件打 Warning 并自动停止 Tick（只支持 ACharacter 的移动组件） |
| 槽位为空 | **不播放**，只广播事件（设计如此，便于后续接入） |
| 软引用首次播放 | 用 `LoadSynchronous()` 同步加载，首次可能有一次轻微卡顿；资源热了之后无影响。若在意可用蓝图预先 `AsyncLoad` |
| 走下台阶/掉落不算跳跃 | 只有"进入 Falling 且 Z 速度 > 0"才判为起跳 |
| 落地分级 | `OnLand` 的 `FallSpeed` 是本次滞空**峰值**下落速度，可用于轻/重落地音效 |
| 地面探测不到 | 返回 `SurfaceType_Default`，走 `DefaultSet` |
| 与 Wwise/MetaSound 集成 | 不填槽，只订阅四个事件把参数（SurfaceName / bRunning / FallSpeed）转发给中间件即可 |

---

## 七、后续可扩展项
1. 用 `MetaSound` 替代 `USoundBase` 槽（把 `SurfaceName`、速度等作为参数传入）。
2. 按**动画状态**（走/跑/落地类型）细分脚步集，而不是只按地面材质。
3. 蹲伏、落地翻滚、滑铲等更多移动事件枚举。
4. ~~距离兜底模式加入"速度→步频"映射~~ —— **已天然满足**：兜底是"每走 `FootstepDistance`(180cm) 踩一次"，
   所以步频 = 速度 ÷ 180，本来就随速度变化（走 200 → 1.1 步/秒；跑 450 → 2.5；疾跑 650 → 3.6）。
5. **脚步槽分出"走 / 跑"**（`FMovementAudioSet` 加 `FootstepRun`，为空时回落到 `Footstep`）：
   现在要区分走路与跑步的脚步，得在蓝图里对 `OnFootstep(bRunning)` 分流；加槽位后填两个资源即可。
6. **落地分出"轻 / 重"**（加 `LandHard` + `HeavyLandSpeedThreshold`，用 `FallSpeed` 判定）：
   现在要在蓝图里对 `OnLand(FallSpeed)` 分流，同上。
7. 把 `MetaSound` 参数化（把 `SurfaceName` / 速度 / `FallSpeed` 当参数传给 MetaSound 或 Wwise 事件）。

---

## 八、★ 声音资产接入实操（跑动 / 跳跃 / 落地）

> 这一节回答一个问题：**手上有了 wav / SoundCue，应该往哪里填、什么时候会响。**

### 8.1 先分清两层：一次性事件（点） vs 持续状态（段）

| 层 | 表现 | 填哪里 | 谁触发 |
|---|---|---|---|
| **一次性事件** | 脚步、起跳、落地 | `Default Set` / `Surface Sets` 里的 `Footstep` / `Jump` / `Land`（都是软引用） | 脚步：AnimNotify 或距离兜底；起跳/落地：**自动检测** |
| **持续状态** | 走路/跑步/疾跑/蹲伏/滞空时的**循环**背景音（音乐、风声、呼吸、脚步循环铺底） | `State Music` 数组（每项：`State` + `Music` + `VolumeMultiplier`） | 状态变化时自动切（`bPlayMusicPerState`） |

两者互不干扰：想"跑起来有低鸣"就填 `State Music` 的 `Run`/`Sprint` 条目；
想"每步一声"就填 `Default Set` 的 `Footstep`。

### 8.2 需要你提供的资源清单

工程里**目前只有 1 个声音资产**（`/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02`，可直接拿来冒烟测试），
脚步/跳跃/落地素材需要你导入或指定。建议目录 `/Game/MHY_ARCH_GAME/Audio/Movement/`。

| 槽位 | 建议资源 | 建议数量 | 说明 |
|---|---|---|---|
| `Footstep`（走） | 落地/摩擦类短音 | 3–5 个 | **不要只给 1 个**，反复播同一段会像机枪；用 `SoundCue` 的 Random 节点，或直接填多个备选靠音高随机（组件已内建 0.95–1.05 随机音高） |
| `Footstep`（跑） | 更重、更快的版本 | 3–5 个 | 目前要走/跑不同音，见 8.5 |
| `Jump` | 起跳/布料/衣摆 | 1–2 个 | 自动在"离开地面且 Z 速度 > 0"时触发（走下台阶不算） |
| `Land` | 落地 | 2 个（轻/重） | 轻/重分级见 8.5；`OnLand` 事件已带**本次滞空峰值下落速度** `FallSpeed` |
| `Surface Sets` | 每种地面材质一份 | 按材质数 | 分组键是 `Physical Surface`（如 Grass/Stone/Metal），不是材质本身 |
| `State Music` | 循环音（可选） | 0–6 | 每个状态一条；循环与否取决于声音资源自身的 Loop 设置 |

### 8.3 三步接上（零代码）

**第 1 步：把资源放进工程**
1. 把 `.wav` 拖进 Content Browser（目标目录 `/Game/MHY_ARCH_GAME/Audio/Movement/`）；
2. 循环用的资源在导入后双击 → 勾 `Looping`；一次性音效不要勾；
3. 想要随机/音量调制：右键 → `Sounds → Sound Cue`，在里面加 Random 节点放多个 wav（推荐）。

**第 2 步：填到组件上**
1. 打开 `BP_FirstPersonCharacter` → 选中 `MovementAudio` 组件；
2. Details 搜 `sound` → 展开 **`Default Set`**：
   - `Footstep` → 选脚步资源（或 SoundCue）；
   - `Jump` → 选起跳资源；
   - `Land` → 选落地资源；
3. 想按地面材质分：先在 `Project Settings → Physics → Physical Surface` 里定义材质名（如 `Grass`/`Stone`），
   给地面材质的 `Physical Material` 指定对应 Surface，再在 **`Surface Sets`** 里 `+` 一条：`Surface` 选该材质名、三个槽同上填。
   **匹配不到就落到 `Default Set`**，所以 `Default Set` 必须至少有一套能听的声音；
4. 想给"跑动"加循环铺底：Details 搜 `state` → **`State Music`** `+` → `State = Run/Sprint`、`Music` 选循环音、`VolumeMultiplier` 调音量。

**第 3 步：确认触发时机**

| 事件 | 现在的触发源 | 你要做的 |
|---|---|---|
| **起跳 / 落地** | 已自动（`bAuto Detect Jump And Land` 默认开） | **不用做任何事**，填了 `Jump`/`Land` 就会响 |
| **脚步（跑动）** | 本工程**没有动画序列**，所以走的是**距离兜底**：每走 180cm 触发一次（已打开 `bAuto Footstep By Distance`） | 已有动画后改用 AnimNotify（见 8.4），并关掉兜底 |

### 8.4 有动画之后：脚步交给动画（更自然）

1. 打开走/跑动画（或其 Montage）→ 在**脚掌着地的那一帧**右键 → `Add Notify → Movement Audio`；
2. 该 Notify 的 `Event` 保持 `Footstep`；跳跃/落地动画里同样加，事件选 `Jump` / `Land`；
3. 回到角色蓝图，把 `bAuto Footstep By Distance` **关掉**（否则动画与兜底会各踩一次）。

> Notify 会自动找到同角色上的 `MovementAudio` 组件并转发事件；没有填任何资源时它也是安全的（只广播事件）。

### 8.5 走 / 跑、轻落地 / 重落地怎么区分（**当前走蓝图，零代码**）

组件的 4 个事件都是 `BlueprintAssignable`，在角色蓝图里 `Bind Event` 即可：

```
On Footstep (SurfaceName, bRunning)   // bRunning=True → 播 RunFootstep，否则 WalkFootstep
On Jump     (SurfaceName)            // 起跳
On Land     (SurfaceName, FallSpeed) // FallSpeed > 700 → 播重落地，否则轻落地
On Run State Changed (bRunning)      // 走↔跑切换（要切脚步循环时用）
```

这一步**不需要改代码**：既可以在蓝图里分流，也可以完全不订阅、直接只填组件槽（那就走/跑同音、落地不分轻重）。

> 想省掉蓝图分流、直接把"跑"和"重落地"做成组件上的槽位 —— 见第 7 节扩展项 5、6（约 30 行代码，需你确认后我加）。

### 8.6 推荐的资产设置（听感相关）

| 项 | 位置 | 建议 |
|---|---|---|
| 音高随机 | 组件 `Pitch Min/Max` | 默认 `0.95–1.05` 已够；脚步可放宽到 `0.9–1.12` |
| 音量 | 组件 `Volume Multiplier` | 全局总音量；单条 `State Music` 还有自己的倍率（两者相乘） |
| 距离衰减 | 组件 `Attenuation` | **第一人称可留空**（自身音效）；有 NPC 时建一个 `Sound Attenuation` 资产填上 |
| 并发限制 | 组件 `Concurrency` | 建一个 `Sound Concurrency`（限制同一时间最多 N 个脚步）避免叠音爆音 |
| 音乐淡入淡出 | 组件 `Music Fade Time` | `0.35s` 交叉淡化；硬切填 0 |
| 3D 播放位置 | — | 事件音在**角色位置**播放（`PlaySoundAtLocation`），音乐是 `SpawnSound2D`（不受衰减影响） |

### 8.7 怎么验证（没有资源也能验）

1. **看事件**：角色蓝图里订阅 4 个事件 → `Print String`（或 `Draw Debug String`），走动/起跳/落地时逐条确认；
2. **看状态**：`Get Movement Audio Debug String` 打出来是一行可读状态
   （状态名 / speed2D / 三个阈值 / run / air / crouch / 音乐是否在播 / 是否派发接口）；
3. **看状态切换日志**：组件上勾 `bLog State Changes`，每次切换会 `LogTemp` 一行；
4. **冒烟测试**：先把 `Default Set` 的三个槽都指向工程里已有的
   `/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02`，跑一遍确认"位置与时机"对了，再换成真实音效。

### 8.8 现象 → 排查

| 现象 | 原因 |
|---|---|
| 起跳/落地有声音，走动没有 | 脚步声只由 AnimNotify 或距离兜底触发 → 检查 `bAuto Footstep By Distance` 是否打开（本工程已开） |
| 完全没声音，但事件在打印 | `Default Set` 的对应槽为空（设计如此：空槽只广播不播放） |
| 只有一部分地面有声音 | `Surface Sets` 里缺该材质条目，且 `Default Set` 也没填 |
| 脚步声像机枪 | 一个槽反复播同一段 → 用 SoundCue 的 Random 节点放多个 wav，或放宽音高随机 |
| 落地总是同一个音量 | 未做轻重分级 → 用 `OnLand` 的 `FallSpeed` 分流（8.5） |
| 跑动时没有"持续"的感觉 | 那是 `State Music` 的职责，不是 `Default Set` |
| 第一次播有轻微卡顿 | 软引用 `LoadSynchronous()` 首次加载；想彻底避免可在蓝图预先 `Async Load Asset` |
### 8.9 本项目当前已接入的内容（可直接 PIE 验证）

`Content/sound_resources/*.mp3` 已导入为 `SoundWave`（**UE 5.7 支持直接导入 mp3**），资产在 `/Game/sound_resources/`。
角色 `BP_FirstPersonCharacter` 的 `MovementAudio` 组件当前接线：

| 项 | 值 | 触发时机 |
|---|---|---|
| `Default Set → Jump` | `jumping-on-wooden-floor-41234`（0.62 s） | 起跳瞬间（进入 Falling 且 Z 速度 > 0） |
| `Default Set → Land` | 同上 | 落地瞬间（`LandedDelegate`；事件带 `FallSpeed`） |
| `Default Set → Footstep` | **空** | 没有单步素材；距离兜底已开，填入即每 180 cm 响一次 |
| `State Music → Run` | `running-6358`（5.93 s，已勾 **Looping**） | 进入 Run 状态时淡入 0.35 s |
| `State Music → Sprint` | 同上 | 进入 Sprint 状态时淡入 |
| `bAuto Footstep By Distance` | **true** | 角色尚无动画序列，跑动靠它触发 |
| `bAuto Detect Jump And Land` | true | 起跳/落地自动，不需要动画 |

**⚠ 阈值关系（决定你实际听到哪个状态）**：`SprintSpeedThreshold = 500`，而 `MaxWalkSpeed = 600`，
所以**正常跑就已经是 Sprint**，`Run`（300–500）只在加速过程中短暂出现。当前两个状态接了同一素材，
所以听感一致。若想"跑 / 疾跑"用不同素材：把 `SprintSpeedThreshold` 提到 **620**
（> 600，只有按 Shift 的疾跑 650 才算 Sprint），再分别填两个状态的 `State Music`。

**已导入但本次未接的素材**（留给门 / 灯的交互音）：开门 `soundreality-opening-door-411632`、
关门 `freesound_community-door-close-79921`、灯开 `dragon-studio-light-switch-on-382714`、
灯关 `freesound_community-light-switch-off-86314`、开关 `freesound_community-switch-100999`。

**验证**：PIE 里跑起来应听到循环跑步声（约 0.35 s 淡入）、按空格起跳一声、落地一声；停下跑步声淡出。

**★ 脚本写软引用的正确姿势（血泪）**：`TSoftObjectPtr` 属性**不能**用字符串，
也不要用 `unreal.SoftObjectPath`（5.7 的 Python 绑定里它既没有可用的路径构造，也找不到可写属性）。
正确做法是传**已加载的 `UObject`**：

```python
s = unreal.MovementAudioSet()
s.set_editor_property("Jump", unreal.EditorAssetLibrary.load_asset("/Game/sound_resources/x.x"))  # ✓
comp.set_editor_property("DefaultSet", s,
    notify_mode=unreal.PropertyAccessChangeNotifyMode.ALWAYS)   # SCS 模板必须用 ALWAYS
```

否则会得到"写进去了但读回来是 `None`"的静默失败（本组件与传送的软引用都踩过同一个坑）。
