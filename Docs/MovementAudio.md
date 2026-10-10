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
| `bAutoFootstepByDistance` | **过渡用**：没接 AnimNotify 前，按行进距离自动踩点 | false |
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
4. 距离兜底模式加入"速度→步频"映射，让过渡期节奏更自然。
