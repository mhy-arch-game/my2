# TimeShift —— 古今时空切换（同地图双区域）

关联代码目录：`Source/MHY_ARCH_GAME/TimeShift/`

**载体**：**单张地图内加载两套代表不同时代的建筑**（分别摆在不同世界坐标），按键**不加载关卡**，而是：
1. 切换时代状态；
2. 由 `UTimeEraComponent` 门控各建筑/道具的**显隐、碰撞、Tick**；
3. 由 `UTimeShiftTravelComponent` 依据**时代锚点配对**，把玩家与随行物品**移到另一时代对应的位置**。

**后续功能**：移动一个时空里的物品会影响另一个时空（跨时空交互）——**仅预留接口**，未实现逻辑。

> 历史沿革：早期版本采用"两个独立关卡 + `OpenLevel`"的载体，现已按需求**彻底移除**跨关卡切换（含关卡名配置与跳转看门狗）。

---

## 一、类清单

| 类 | 基类 | 职责 |
|---|---|---|
| `ETimeEra` | `UENUM` | `Ancient`(古) / `Modern`(今) |
| `ETimeShiftMappingMode` | `UENUM` | 位置映射模式：`LayoutOrigin`（布局参考映射）/ `AnchorPair`（锚点配对） |
| `FTimeShiftAnchorPair` | `USTRUCT` | 同一 `AnchorId` 在古今两侧的锚点引用 |
| `FTimeLinkState` | `USTRUCT` | **★预留**：跨时空状态载荷 |
| `ITimeLinkableInterface` | `UINTERFACE` | **★预留**：`GetTimeLinkId()` / `OnLinkedStateChanged()` |
| `UTimeShiftSubsystem` | `UGameInstanceSubsystem` | 权威时代状态、计时锁、事件广播、**锚点注册表**、**布局参考注册表**、**★预留**链接注册表 |
| `UTimeEraComponent` | `UActorComponent` | 把建筑/道具绑定到某个时代；时代变化时开关显隐/碰撞/Tick |
| `ATimeShiftAnchor` | `AActor` | 时代锚点：`AnchorId` + `Era`（+ `bDefinesLayoutOrigin` 可充当布局参考），自动注册 |
| `UTimeShiftTravelComponent` | `UActorComponent` | 挂玩家：时代变化时按映射把 Owner（+随行物品）移到另一时代对应位置 |
| `UTimeShiftInputComponent` | `UActorComponent` | 绑定可配置 `InputAction` → `SwitchEra()`；`CanRequestSwitch()` 供 UI 置灰 |

新增包含路径：`MHY_ARCH_GAME/TimeShift`、`MHY_ARCH_GAME/TimeShift/Interfaces`。

---

## 二、切换流程

```
玩家按下绑定的按键（Enhanced Input, Started）
   │
   ▼
UTimeShiftInputComponent::HandleSwitchInput() → RequestSwitch()
   │
   ▼
UTimeShiftSubsystem::SwitchEra() → SetEra(另一个时代)
   │   ├─ CanSwitchEra() 校验：未在处理中 且 未处于计时锁
   │   ├─ CurrentEra 更新
   │   ├─ 广播 OnEraChanged(新时代)                    ← 关键：内容门控 + 位置切换都在此发生
   │   │     ├─ UTimeEraComponent：按 Era 开关各自对象的 显隐/碰撞/Tick
   │   │     └─ UTimeShiftTravelComponent：
   │   │           求"时代→时代"的映射变换 M
   │   │           → 对玩家与随行物品执行 NewTransform = M * OldTransform
   │   └─ StartCooldown() 开启计时锁
```

**刚体映射而非"吸附"**：映射是把旧布局世界坐标**换算到**新布局坐标系，因此玩家相对参照点的位置、朝向都会保留；
两套布局只差平移时，`M` 退化为纯平移，等价于"整体偏移"。

**降级**：首选模式取不到数据时自动回退到另一模式；两者都没有 → **留在原地**（不移动）。

---

## 二·补、位置映射模式（ETimeShiftMappingMode）

| 模式 | 依据 | 适用 | 需要摆几个锚点 |
|---|---|---|---|
| **`LayoutOrigin`（默认）** | 每个时代各一个**布局参考**（`Time Shift Anchor` 勾 `bDefines Layout Origin`），映射 `M = 今布局 * 古布局⁻¹` | **两套布局一一对应**（可活动区域、可交互对象完全对应） | **每个时代 1 个**，共 2 个 |
| `AnchorPair` | 就近锚点 → 同名锚点差值 | 两套布局**不统一**、分区域各自不同 | 每个可切换区域 1 对 |

- `LayoutOrigin` 支持布局之间存在**旋转甚至缩放**（用完整变换而非单纯偏移）；
- `AnchorPair` 作为回退：即便没配布局参考，摆过锚点对也能工作；
- 若两种模式都不满足 → 内容照常切换，玩家**留在原地**。

**为什么默认用 `LayoutOrigin`**：既然两套布局一一对应，用"一个全局参考"即可覆盖全部位置，
无需为每个区域成对摆放锚点；搭配"可交互对象也完全对应"，后续跨时空联动也能按相对坐标自动配对。

---


## 三、搭建步骤（引擎内）

### 3.1 一张地图里摆两套建筑
1. 在关卡中搭好"古"的一整套建筑，再搭好"今"的一整套建筑，**两者放在地图的不同位置**（互不重叠即可）。
2. 给每个建筑（或整组建筑的根 Actor）挂 `Time Era` 组件：
   - `Era` = 该建筑所属时代（`Ancient` / `Modern`）；
   - `bGate Visibility` / `bGate Collision` 默认开启；
   - 需要"两个时代都存在"的道具（如地形、公共装饰）勾 `bExists In Both Eras`。
3. 切换时，不属于当前时代的建筑会被自动隐藏并关闭碰撞。

### 3.2 摆放"布局参考"锚点（决定对应位置怎么算）

**推荐做法（布局一一对应时）——每个时代只放 1 个：**
1. 在"古"布局里选一个**基准点**（例如世界原点、主门中心）放一个 `Time Shift Anchor`：
   - `Era = Ancient`
   - 勾上 **`bDefines Layout Origin`**
   - `Anchor Id` **可留空**（它只当布局参考，不参与锚点配对）
2. 在"今"布局的**同一语义位置**放另一个锚点：
   - `Era = Modern`
   - 同样勾 **`bDefines Layout Origin`**
3. 两个锚点之间的**变换差**（位置/旋转/缩放）就是两套布局的换算关系。
   之后任意位置的玩家都会被换算出对应位置，无需逐区域配置。

**可选做法（布局不统一时）——`AnchorPair` 模式：**
1. 在需要切换的区域各放一对锚点，**两侧 `Anchor Id` 完全相同**；
2. 把玩家 `Time Shift Travel` 组件的 `Mapping Mode` 改成 `Anchor Pair`。

### 3.3 玩家配置
1. 在角色蓝图挂：
   - `Time Shift Input`：`Switch Action` 指向你复用/新建的输入动作；
   - `Time Shift Travel`：默认 `bTeleport Owner` 已勾选。
2. **随行物品**：给要一起穿越的 Actor 打上 **`TimeTraveler`** 标签（默认按标签 + 半径 800 自动收集），
   或显式登记到 `Time Shift Travel → Carried Actors`（附着在玩家身上的物品会自动跟随，组件会跳过它们以避免位移叠加）。

### 3.4 运行验证
- 按切换键 → 当前时代的建筑消失、另一时代的建筑出现，玩家被移动到**换算后的对应位置**；
- 站在任意位置切换，切换后仍处于"相对布局参考点相同的偏移"处（不会吸附到锚点）；
- 连续按 → 受计时锁限制（见第四节）；
- 未配置布局参考、也没有可用锚点对 → 建筑切换但角色留在原地（预期降级）。

---

## 四、计时锁（冷却 / Timed Lock）

切换成功后会施加一段计时锁：锁定期内 `CanSwitchEra()` 为假，切换请求被拒绝。

| 项 | 行为 |
|---|---|
| **起算时机** | 时代切换**应用完成的瞬间**（本载体无关卡加载，故立即可靠起算） |
| **跨世界存活** | 由 **Core Ticker（`FTSTicker`）** 驱动，不依赖世界计时器 |
| **查询接口** | `IsOnCooldown()` / `GetCooldownRemaining()` / `GetCooldownRatio()` 均为纯查询，UI 可每帧轮询 |
| **事件** | `OnCooldownStarted` / `OnCooldownFinished` |
| **被拒绝的请求** | 不触发、不重置计时（连按不会无限续期） |
| **配置** | `CooldownDuration=2.0`（秒），`0` = 关闭；亦可运行时 `SetCooldownDuration()` |

接口一览：`CanSwitchEra()`、`IsOnCooldown()`、`GetCooldownRemaining()`、`GetCooldownRatio()`、
`GetCooldownDuration()`、`SetCooldownDuration()`、`StartCooldown()`、`ClearCooldown()`、
`OnCooldownStarted`、`OnCooldownFinished`，以及 `UTimeShiftInputComponent::CanRequestSwitch()`。

---

## 五、★预留：跨时空交互接口（本期不实现逻辑）

| 预留项 | 位置 | 状态 |
|---|---|---|
| `ITimeLinkableInterface` | `Interfaces/TimeLinkableInterface.h` | 已定义契约 |
| `FTimeLinkState` | `TimeShiftTypes.h` | 已定义载荷（变换/启用/标签） |
| `RegisterLinkable / UnregisterLinkable` | `UTimeShiftSubsystem` | 已实现注册表（弱引用，关卡卸载自动清理） |
| `BroadcastLinkState(LinkId, State)` | `UTimeShiftSubsystem` | 已实现广播：通知同 `LinkId` 的已注册对象 |
| `GetLinkablesById(LinkId)` | `UTimeShiftSubsystem` | 已实现查询 |

> ⚠️ 当前**没有任何调用方**：没有对象自动注册，也没有代码自动触发 `OnLinkedStateChanged`。
> 这些是"插座"，接线留待后续功能实现。

**后续实现路径**（届时只需加这些，不动切换代码）：
1. 让"两个时代的同一件东西"实现 `ITimeLinkableInterface`，返回**相同的 `GetTimeLinkId()`**；
2. `BeginPlay` 注册 / `EndPlay` 注销：`Subsystem->RegisterLinkable(this)`；
3. 对象变化时组装 `FTimeLinkState` 并 `Subsystem->BroadcastLinkState(LinkId, State)`；
4. 在 `OnLinkedStateChanged(State)` 中把状态应用到本对象；
5. 子系统为 `GameInstanceSubsystem`，可作为持久状态层（如需更多字段可在此扩展）。

---

## 六、边界与注意事项

| 情形 | 说明 |
|---|---|
| 新时代无可用映射 | **留在原地**，只切换内容显隐（可预期的降级） |
| 两套布局只差平移 | 映射退化为纯平移（整体偏移），最常见也最省心 |
| 两套布局存在旋转/缩放 | `LayoutOrigin` 模式用完整变换映射，位置与朝向都会换算过去 |
| 新旧参照点完全相同 | 映射为单位变换 → 不做位移（视为同位置双时空） |
| 随行物品已附着在玩家上 | 自动跟随，组件会跳过它以免位移叠加 |
| 随行物品是关卡内独立 Actor | 给它打上 `TimeTraveler` 标签即可被自动收集（默认半径 800，可调 `Auto Collect Radius`）；也可显式登记到 `Carried Actors` |
| 连续快速按键 | 计时锁 + 切换中守卫共同拒绝；不会重复位移 |
| 计时锁时长设为 0 | 关闭计时锁，仅受"切换中"守卫限制 |
| 布局不统一（分区域各自不同） | 把 `Mapping Mode` 改为 `Anchor Pair`，为每个区域成对摆锚点 |
| 两个时代的建筑重叠摆放 | 也可行（映射为恒等），此时等价于"同位置显隐切换" |
| 组件绑定的清理 | `UTimeEraComponent` / `UTimeShiftTravelComponent` 均在 `EndPlay` 注销委托 |
| 与 `StructureInteraction` 的关系 | 互不依赖；如需"某时代才能交互"，可在 `CanInteract` 中查询 `GetEra()`（后续扩展） |

---

## 七、后续完善清单（backlog）
1. 跨时空交互（`ITimeLinkableInterface` 接线 + 双时代对象配对）。
2. 切换过渡表现（过场/特效/音效，可参考 `Docs/LiquidLight.md` 的表现思路）。
3. 计时锁的 HUD 表现：`GetCooldownRatio()` 驱动环形/条形填充，`OnCooldownFinished` 恢复提示（属呈现层，预留）。
4. "切换过程中禁止移动/输入"等手感细节。
5. 时代相关的解锁条件（结合 `Docs/LightReveal` 的受光逻辑）。
6. 若区域很大、需要流式加载而非同图常驻，可再评估分区/子关卡方案。

---

## 八、相关文档

| 文档 | 内容 |
|---|---|
| `Docs/TimeShiftDemo.md` | **可运行 demo 用例**：一张地图搭古/今双庭院、摆锚点、配玩家、验证与常见问题 |
| `Docs/InteractionLogic.md` | 交互模块的人机活动逻辑与解耦设计原理 |
| `Docs/StructureInteraction.md` | 可交互建筑结构框架 |
| `Docs/OverlapDemo.md` | 重叠即通行 + 变材质/透明 |
| `Docs/LiquidLight.md` | 光照液体流动表现 |

---

## ★ 跨时空都要存在的道具：勾 `bExistsInBothEras`

`UTimeEraComponent` 在 BeginPlay 与每次换时空时都会：

```
bActiveInCurrentEra = bExistsInBothEras || (Era == 当前时空)
SetActorHiddenInGame(!bActive)     // bGateVisibility 默认开
SetActorEnableCollision(bActive)   // bGateCollision 默认开
```

⇒ **不勾 `bExistsInBothEras` 的物体在运行时会被隐藏**，而编辑器视口不做这件事 ——
典型症状是"编辑器里看得到、一运行就少了一半"（本项目 10 个传送装置就踩过：
`InitialEra = Ancient` 时 5 个 Modern 装置全被隐藏）。

| 物体 | 该怎么设 |
|---|---|
| 只属于一个时空的景物（建筑 / 植被） | `Era` 填对，`bExistsInBothEras = ✗` |
| **跨时空都要在的道具**（传送装置、公共地面、UI 锚点） | `bExistsInBothEras = ☑`（此时 `Era` 仍可作为"它属于哪一侧"的语义标签被其它系统读取，例如 `UTimeEraPortalComponent` 的配对方向） |
| 与时空完全无关的物体 | 不要挂 `TimeEraComponent`（挂了也不会隐藏，但没必要） |

> 初始时空由 `UTimeShiftSubsystem::InitialEra` 决定，是 `config=Game` 属性：
> 可以在 `Config/DefaultGame.ini` 的 `[/Script/MHY_ARCH_GAME.TimeShiftSubsystem]` 段落里覆盖。
