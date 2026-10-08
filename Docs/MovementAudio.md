# MovementAudio —— 移动音频接口（跳跃 / 行走 / 奔跑）

关联代码目录：`Source/MHY_ARCH_GAME/MovementAudio/`

**定位**：这是一套**音频接口（挂点）**，不是音效包。所有声音槽都是**可留空的软引用**：
- **现在**：不填任何资源也能工作——组件照常追踪移动状态并**广播事件**；
- **后续**：把 `TSoftObjectPtr<USoundBase>` 槽位填上资源即可自动播放，**无需改代码**；
- 也可以完全不填槽，只订阅事件，把声音接到 Wwise / MetaSound / 蓝图。

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
