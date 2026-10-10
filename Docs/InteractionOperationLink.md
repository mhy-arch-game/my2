# 交互操作联动（Interaction Operation）：让 A 的交互改变 B 的状态

> 需求：**一个接口面向可交互物品（箱子、灯泡、门）；把"触发对应操作"抽象成一个接口
> （开门、开灯、移动到正确区域内…），使得另一个物品的状态发生对应变化。**

实现方式完全建立在**原先的 interact 内容**之上：发送端接 `UInteractableComponent`，
接收端的默认动作直接调用 `UInteractableComponent::SetOpen()`（同一套状态管着门与灯）。

> **主 / 从组绑定（门 · 灯 · 墙三种交互状态）见 §9。**

---

## 1. 数据流

```
[可交互物 A：箱子 / 灯泡 / 门]
        │ 玩家聚焦 + 按 E
        v
UInteractableComponent::NotifyInteract      ← 原先的交互管线，未改动
        │ 先翻转自身开关状态，再广播 OnInteractRequested
        v
UInteractionLinkComponent  （挂在 A 上）
        │ 遍历 Entries，构造 FInteractionOperation
        │   { Operation="Open", Source=A, Instigator=玩家, Location=… }
        v
寻址：显式 Targets  ＋  频道扫描（匹配接收方 Channel）
        │
        ├──► IInteractionOperationReceiver（接口，蓝图/C++ 实现）
        └──► UInteractionOperationReceiverComponent（组件，零蓝图）
                     │
                     v
        [可操作物 B：另一扇门 / 另一盏灯 / 需要被搬进区域的箱子]
              B 自己决定怎么变化（开门 / 开灯 / 移动 / 隐藏…）
```

## 2. 四个组成部分

| 文件 | 作用 |
|---|---|
| `InteractionOperationTypes.h` | `FInteractionOperation`（抽象操作载荷）、`EInteractionOperationAction`（接收端默认动作枚举） |
| `InteractionOperationReceiverInterface.h` | **`IInteractionOperationReceiver`**——本功能唯一面向被操作物的接口，`Blueprintable` |
| `InteractionLinkComponent.h/.cpp` | **发送端组件**：一次交互 → 一批抽象操作 → 分发给接收方 |
| `InteractionOperationReceiverComponent.h/.cpp` | **接收端组件**：操作名 → 动作 映射表，零蓝图改变 owner 状态 |

**全局只有一个接口**：`IInteractionOperationReceiver`。发送端不需要实现接口，它只是调用方。

## 3. 抽象操作 `FInteractionOperation`

| 字段 | 说明 |
|---|---|
| `Operation` | **抽象操作名**（`FName`）。这就是"抽象"的载体 |
| `Instigator` | 发起交互的角色（玩家 Pawn） |
| `Source` | 发起这次操作的可交互物本身（A） |
| `Target` | 可选参照对象（给 `MoveTo`/`AttachTo` 类操作用） |
| `Value` | 通用数值：强度 / 时长 / 高度 |
| `bActive` | 通用开关位 |
| `Location` | 通用落点（`MoveTo` 用） |

### 约定俗成的操作名（不是枚举，可自由扩展）

| 名字 | 语义 |
|---|---|
| `Open` | 打开（门开、灯亮、机关启动） |
| `Close` | 关闭 |
| `Toggle` | 取反 |
| `On` / `Off` | 等价于 `Open` / `Close`，语义更贴近灯 |
| `MoveTo` | 移动到 `Location` |
| `Activate` / `Deactivate` | 通用启用/停用 |

**收发双方只要说好名字就行**，不需要改 C++。

## 4. 两种寻址方式（可以混用）

| 方式 | 怎么配 | 适合 |
|---|---|---|
| **显式引用** | 发送端条目里的 `Targets` 直接放 actor | 关卡里就在手边、一眼能连上的 |
| **频道** | 发送端条目填 `Channel`；接收端 `Channel` 填**同名** | 相距很远、运行时才生成、或者"一群同类目标一起动" |

显式引用优先；同一目标不会被重复投递。**发送端不会发给自己**（避免自触发）。

---

## 5. 蓝图类怎么建（本文档重点）

有三种由浅到深的做法，可以混用。

### 5.1 路线一：全组件、零蓝图（最快跑通）

**发送方 A（例如一个箱子）**
1. Components → **Add Component → Interactable**（已有交互组件就跳过）
2. Components → **Add Component → Interaction Link**
3. 选中 `InteractionLink` → Details 搜 **`Interaction`**（分类名就是 `Interaction Operation`）
4. 展开 **Entries**，加一条：
   - **Operation** = `Open`
   - **Channel** = `Door_Light`（自己起个名字，两端要一致）
   - **Targets** 留空（走频道）
   - **Mirror Source State** = ☑（可选，见 §5.4）

**接收方 B（例如另一扇门 / 另一盏灯）**
1. Components → **Add Component → Interactable**（B 自己也应该能被交互时）
2. Components → **Add Component → Interaction Operation Receiver**
3. 选中它：
   - **Channel** = `Door_Light`（与 A 完全一致）
   - 展开 **Bindings**，加一条：**Operation** = `Open`，**Action** = `Set Open (开 / 亮)`
   - 再加一条：**Operation** = `Close`，**Action** = `Set Closed (关 / 灭)`
4. 完成。按 E 交互 A → B 跟着开门/亮灯。**一行蓝图都不用写。**

### 5.2 路线二：做一个发送端蓝图类

需要复用时把它做成蓝图类：

1. Content Browser → **Add → Blueprint Class**，父类选 **`Actor`**（要当实体就选 `StaticMeshActor`）。
2. 命名如 `BP_InteractionLinker`。
3. 打开后加组件（顺序无所谓）：
   - `StaticMesh`（要看得见的话）
   - **`Interactable`** —— 配 `InteractionPrompt`（提示文案）、需要开合动画就开 `bUseBuiltInToggle`
   - **`InteractionLink`** —— 配 `Entries`
4. ⚠️ **`Targets` 不要填关卡实例**：蓝图默认值引用不到具体关卡对象。
   跨 actor 的目标请用 **`Channel`**，或把目标 **attach** 到本源上后再用显式引用（也不推荐）。
5. **Compile + Save**。

### 5.3 路线三：做一个接收端蓝图类（实现接口，自定义行为）

当映射表那六种动作不够用时（比如"开门"要播一段动画 + 出声音 + 解锁下一步），
就让蓝图类**实现接口**：

1. 打开接收方蓝图（例如门 `BP_...`）。
2. 右上 **Class Settings** → **Interfaces → Implemented Interfaces** → **Add** → 选
   **`Interaction Operation Receiver`**。
3. **Compile**。之后在 **My Blueprint → Interfaces** 下能看到三个事件：
   | 事件 | 什么时候用 |
   |---|---|
   | **Get Operation Channel** | 返回频道名（返回空 = 只能被显式引用） |
   | **Can Receive Operation** | 返回 false 可以拒绝（做"锁着"的条件判断） |
   | **Apply Interaction Operation** | **主逻辑**：按 `Operation` 分支做自己的事 |
4. 在 **Apply Interaction Operation** 的事件图里：
   - 拆 `Operation`（`FName`），用 `Switch on Name` 或 `==` 分支：
     `Open` → 自家 `Interactable` 的 `Set Open (true)`；`Close` → `Set Open (false)`；
     `MoveTo` → `Set Actor Location`（用操作里的 `Location`）；
   - 额外表现随便接：`Play Sound at Location`、`Spawn Emitter`、`Set Timer by Event`…
5. **Compile + Save**。

> 同一个 actor **同时**实现接口又挂接收组件时：**接口优先**，组件不再被调用（避免动作执行两次）。

### 5.4 让 B 跟着 A 的开合一起动（`Mirror Source State`）

发送端条目里的 **Mirror Source State** 打开后：

- A **打开** → 发 `Operation`（例如 `Open`）
- A **关闭** → 发 **`Closed Operation`**（默认 `Close`）

这样"我开它开、我关它关"用一条条目就够，不用配两条。
（内部读的是 `UInteractableComponent::IsOpen()`，而 `NotifyInteract` 是**先改状态再广播**，
所以读到的一定是新状态。）

> 从物只有"开 / 关"两个状态、纯跟随主物时，**不必**打开 `Mirror Source State`：
> 用操作里的 `bActive` + 接收端 `MirrorActive` 一条映射即可，见 **§9.3**。

### 5.5 接收端八种内置动作（`Action` 枚举）

| Action | 对 owner 做什么 |
|---|---|
| **Set Open (开 / 亮)** | 有 `Interactable` → `SetOpen(true)`（门开 + 灯亮）；没有 → 直接点亮 owner 上的灯 |
| **Set Closed (关 / 灭)** | 同上，`SetOpen(false)` |
| **Toggle Open (取反)** | 按当前状态取反 |
| **Set Actor Hidden (显隐)** | 按操作里的 `bActive` 隐藏/显示 + 开关碰撞 |
| **Move To (移动落点)** | 把 owner 移到操作里的 `Location`；`Duration > 0` 时插值过去，否则瞬移 |
| **Mirror Master State (跟随主物开关)** | 按操作里的 `bActive`（= 主物当前开 / 关）设置 owner：有 `Interactable` 走 `SetOpen(bActive)`，没有则点灯。**从物只做 0-1 跟随就用它** |
| **Set Actor Visible (显形, 墙用)** | 与 `Set Actor Hidden` **极性相反**：`bActive=true` ⇒ 显示 + 开碰撞（主物一开、墙显形） |
| **Nothing (只走蓝图)** | 不做默认改变，只触发 `On Operation Received` / `On Operation Applied` |

---

## 6. 三个完整示例

### 6.1 箱子 → 灯泡（`Open` 联动）

| 端 | 配置 |
|---|---|
| 箱子 | `Interactable`（提示"打开箱子"） + `InteractionLink` → Entries: `Operation=Open`, `Channel=Lamp_01`, `Mirror=☑`, `ClosedOperation=Close` |
| 灯泡 | `Interactable`（`bToggleLights=☑`, 灯光组件名填好） + `InteractionOperationReceiver` → `Channel=Lamp_01`, Bindings: `Open→SetOpen`, `Close→SetClosed` |

按 E 开箱子 → 灯亮；再按 E 关箱子 → 灯灭。

### 6.2 拉杆 → 远处的门

| 端 | 配置 |
|---|---|
| 拉杆 | `Interactable` + `InteractionLink` → Entries: `Operation=Open`, `Channel=Gate_A`, `Mirror=☑` |
| 门 | `Interactable`（`bUseBuiltInToggle=☑`, `ToggleComponentNames=[门板]`） + `InteractionOperationReceiver` → `Channel=Gate_A`, Bindings: `Open→SetOpen`, `Close→SetClosed` |

**为什么用频道而不是引用**：两者在关卡里可能隔一整条走廊，频道比连引用更好维护，
而且复制门实例时频道自动生效。

### 6.3 把物品（箱子）移动到正确区域

| 端 | 配置 |
|---|---|
| 被移动的箱子 | `InteractionOperationReceiver` → `Channel=MovePuzzle`, Bindings: `MoveTo → Move To`, **Duration** = `1.5`（1.5 秒插值过去） |
| 触发者（拉杆 / 压力板） | `InteractionLink` → Entries: `Operation=MoveTo`, `Channel=MovePuzzle`, **Location** = 目标区域内的一点（世界坐标） |

操作里的 `Location` 由发送端条目提供；接收端 `Duration > 0` 就用插值，
否则用操作自带的 `Value` 秒数，再不填就瞬移。

> 也可以反过来：把 **`MoveTo` 的 `Target`** 设成区域中心那个 actor，
> 接收端在蓝图里读 `Target.Location` —— 适合落点由另一个物体决定的情况。

---

## 7. 调试

### 7.1 日志（已内置，开箱即用）

```
[OperationLink] <箱子>: 自动创建 InteractableComponent            ← 没挂 Interactable 时
[OperationLink] <箱子> -> 'Open' (channel 'Lamp_01'): 1 receiver(s).   ← 每次派发
[OperationReceiver] <灯>: 应用操作 'Open'。                              ← 每次成功改变状态
```

两条最关键：

- **`... 0 receiver(s)`** 或出现
  `交互发出了 N 个条目，但没有任何接收方被命中` → 寻址问题（见 7.3）；
- **`... 没有匹配的映射，已忽略`**（Verbose 级）→ 接收端 `Bindings` 里没有这个操作名。

### 7.2 蓝图里自检

| 节点 | 用途 |
|---|---|
| **`Resolve Entry Targets`**（发送端） | 传条目索引，返回它会命中哪些 actor。**这是排查"为什么没联动"最快的节点**——返回空就是寻址没对上 |
| **`Get Last Dispatched Count`**（发送端） | 最近一次派发命中几个接收方 |
| **`Dispatch Entries`**（发送端） | 不用交互，手动发一轮（接个按键就能测） |
| **`Dispatch Operation`**（发送端） | 立刻发一条指定操作（`Operation` + `Channel`） |
| **`Get Last Applied Operation`**（接收端） | 最近一次命中的操作名 |
| **`On Dispatched`**（发送端事件） | 每次派发后触发，接 `Print String` 看操作名与命中数 |
| **`On Operation Received` / `On Operation Applied`**（接收端事件） | 收到操作 / 成功应用时触发 |

### 7.3 故障排查表

| 现象 | 原因 | 解法 |
|---|---|---|
| 按 E 完全没有 `[OperationLink]` 日志 | 组件没绑上交互 | 确认发送端有 `Interactable`（或让 `bAutoBindInteractable` 开着自动建）；确认玩家确实聚焦到了这个物体 |
| 有派发日志但 `0 receiver(s)` | 频道名不一致 / 目标不在频道上 | 两端 `Channel` 必须**完全相同**；用 `Resolve Entry Targets` 检查。注意接收端**必须**实现接口或挂接收组件，两者都没有就不会被扫到 |
| 接收端有日志但状态没变 | `Bindings` 里缺这个操作名 | 补一条 Binding，或改用接口在蓝图里处理 |
| 灯/门动了一下又弹回去 | 两边都在被别的系统改同一个状态 | 同一个 actor 不要同时受多套门控（例如既被 `ProximityBarrier` 隐藏、又被操作接收） |
| 动作执行了两次 | 同一个 actor **既实现了接口又挂了接收组件** | 只保留一种；接口优先，组件会被跳过（但如果蓝图里又手动调了一次就会重复） |
| 一次交互影响了好几个物体 | 用了同一个 `Channel` | 这是频道寻址的**预期行为**（"一群同类目标一起动"）。要独立控制就换独立频道或改用显式引用 |
| `MoveTo` 没动 | `Location` 填的是 (0,0,0)，或 `Duration`/`Value` 都为 0 且落点与当前位置相同 | 检查条目里的 `Location`（世界坐标） |

### 7.4 找不到设置怎么办（分类名）

本功能的**所有**属性都在**同一个扁平分类 `Interaction Operation`** 下：

- Details 搜索框输入 **`interaction`** 或 **`operation`** 都能命中；
- 故意做成扁平（没有 `A|B` 子分类），因为子分类会继承 UE 记住的折叠状态，
  容易造成"看起来没有属性"。如果你仍看不到，先展开分类左侧的三角。

---

## 8. 与既有系统的分工

| 想做的事 | 用哪个 |
|---|---|
| 物体被玩家聚焦/按键的**入口** | `UInteractableComponent` / `IInteractableInterface`（**原先的 interact，未改动**） |
| 物体自己的开合与灯光 | `UInteractableComponent` 的内置开关（`bUseBuiltInToggle` / `bToggleLights`） |
| 物体只持有"开 / 关"两态、不驱动任何部件 | `UInteractableComponent` 的 **`bTrackOpenState`**（本轮新增） |
| **A 的交互去改变 B 的状态** | **本文档**：`InteractionLink` + `IInteractionOperationReceiver` |
| 交互后把玩家传送走 | `UTimeEraPortalComponent`（`Docs/TimeEraPortal.md`） |
| 按距离触发、整组显形并永久封死 | `UProximityBarrierComponent`（`Docs/ProximityBarrier.md`） |
| 受光驱动的隐藏面/显形面 | `LightReveal`（`Docs/LightReveal.md`） |

**为什么不把"开门/开灯"硬编码成枚举？** 因为需求里明确要"抽象为一个接口对接"：
操作名是**数据**，收发双方约定即可扩展（`PlayAnimation`、`Unlock`、`Spawn`…），
C++ 不需要改。内置的八种 `Action` 只是"接收端偷懒用的默认实现"。

---

## 9. ★ 主从组绑定：门 / 灯 / 墙（本轮展开）

> 需求：三种交互状态 —— **门（开闭）/ 灯（开关）/ 墙（显隐）**；物体分**主 / 从**；
> **主操作物的交互形态与 interact 模块完全一致**（聚焦 → 提示 → 按 E → 状态改变）；
> **从物只做 0-1 两种状态**，且仅仅因为主物变化而改变。

### 9.1 主 / 从 的职责与硬性约束

| | 主物（Master） | 从物（Slave） |
|---|---|---|
| 交互入口 | **完整走 interact 模块**：`Interactable`（聚焦 / 描边 / 提示 / 按 E 一个不少） | **不参与交互**：`Interactable` 的 **`bEnabled = ✗`**（探测器直接跳过，也不出提示） |
| 行为 | `bUseBuiltInToggle` 开门 / `bToggleLights` 开灯 / **`bTrackOpenState` 只存状态** / 蓝图自定义 | 只被 `SetOpen(true/false)` 被动改变（门转、灯亮灭），或用 `SetActorVisible` 显隐 |
| 组件 | `Interactable` + **`Interaction Link`** | `Interactable`（仅作状态与表现载体；墙可省） + **`Interaction Operation Receiver`** |
| 状态位 | `bUseBuiltInToggle` / `bToggleLights` / **`bTrackOpenState`** 三者**至少开一个** | 同上（否则 `SetOpen` 直接返回，从物不会动） |

**⚠ 状态的来源（最容易踩的一条）**：`UInteractableComponent::SetOpen()` 在
`bUseBuiltInToggle`、`bToggleLights`、`bTrackOpenState` **三者都为关**时**直接 return**，
`bIsOpen` 永远是 false ⇒ 主物发给从物的 `bActive` 永远是"关"，从物永远不动。
**主物至少要有一个状态来源**，三种选一：

| 主物是什么 | 状态来源 | 说明 |
|---|---|---|
| 有开合部件（门 / 闸 / 升降墙） | `bUseBuiltInToggle` | 状态与位移由同一套驱动 |
| 只管灯（灯泡 / 灯柱） | `bToggleLights` | 状态与灯亮灭由同一套驱动 |
| **纯蓝图效果 / 什么都不动** | **`bTrackOpenState`（本轮新增）** | **只保存 0/1 状态、不驱动任何部件**：按 E 翻转它、`OnToggleChanged` 广播，`InteractionLink` 就能把它当 `bActive` 转发给从物 |

> `bTrackOpenState` 打开后 `bStartOpen` 同样有效（决定初始状态），并且**不会**让组件进入无意义的插值 tick。
> 想完全自己维护状态也可以：蓝图里存一个布尔，在交互事件里调 `Dispatch Entries`。

### 9.2 三种从物怎么配（照抄）

| 从物 | 要挂的组件 | 关键字段 | 接收端动作 |
|---|---|---|---|
| **门（开闭）** | `Interactable` + `Interaction Operation Receiver` | `Interactable`：`bUseBuiltInToggle=☑`、`ToggleComponentNames=[门板组件名]`、`bUseAxisRotation=☑` + 轴 / 枢轴 / 角度、`ToggleDuration`、`bStartOpen` 与主物一致；**`bEnabled=✗`** | **`Mirror Active`** |
| **灯（开关）** | 同上 | `Interactable`：`bToggleLights=☑`、`LightComponentNames=[灯组件名]`、`bStartOpen` 与主物一致；**`bEnabled=✗`** | **`Mirror Active`** |
| **墙（显隐）** | `Interaction Operation Receiver`（墙多半不需要门 / 灯那套，**可以不挂 `Interactable`**） | 若挂了 `Interactable`：**`bEnabled=✗`** | **`Set Actor Visible`**（`bActive=true` ⇒ 显示 + 开碰撞） |

三种从物**都是一条映射**搞定 —— 这正是本轮新增 `MirrorActive` / `SetActorVisible` 的原因。

### 9.3 最小配置（主物 1 条条目 + 从物 1 条映射）

**主物**

| 字段 | 值 |
|---|---|
| `Interactable → Interaction Prompt` | 例如"打开机关"（**提示只有主物有**） |
| `Interaction Link → Entries` `+` 一条 | `Operation = Sync`（名字任意，两端一致即可） |
| | `Channel = Group_A`（或 `Targets` 直接引用几个从物） |
| | **`Mirror Source State = ✗`**（留关，见下方说明） |

**从物**

| 字段 | 值 |
|---|---|
| `Interaction Operation Receiver → Channel` | `Group_A`（与主物**完全相同**） |
| `Bindings` `+` 一条 | `Operation = Sync` → `Action = Mirror Master State`（门 / 灯）或 `Set Actor Visible`（墙） |

为什么 `Mirror Source State` 关着也能"我开它开、我关它关"？
因为 `MirrorActive` 读的是操作里的 **`bActive`**，而发送端把它填成**主物当前的开 / 关状态**，
所以主物无论开还是关都发同一个操作名 `Sync`，从物照着 `bActive` 走即可。

> 旧写法（`Mirror Source State=☑` + `Open→SetOpen`、`Close→SetClosed` 两行）同样可行，
> 只是行数多、容易漏一行。两种可以混用（不同条目互不影响）。

### 9.4 蓝图搭建步骤

**主物蓝图（例：`BP_Master_Switch`）**

1. 新建蓝图类（父类 `Actor`）→ 加你需要的网格 / 灯；
2. **Add Component → `Interactable`**：
   - `Interaction Prompt` 填提示；
   - 门就勾 `bUse Built In Toggle` + 填 `Toggle Component Names`；灯就勾 `bToggle Lights` + 填 `Light Component Names`；
3. **Add Component → `Interaction Link`**：`Entries` 加一条（`Operation` / `Channel` 见 9.3）；
4. （可选）勾 **`Sync On Begin Play`**，让整组在关卡开始时对齐主物状态（见 9.5）；
5. 关卡里放实例即可，**不需要任何蓝图连线**。

**从物蓝图（例：`BP_Slave_Door`）**

1. 新建蓝图类 → 放门板网格，记下组件名（如 `DoorMesh`）；
2. **Add Component → `Interactable`**：
   - **`bEnabled = ✗`**（关键：从物不可被玩家交互）；
   - 勾 `bUse Built In Toggle` + `Toggle Component Names=[DoorMesh]`；
   - 铰链参数照原先那套填（`bUseAxisRotation` / `RotationAxis` / `RotationPivot` / `OpenAngleDegrees` / `ToggleDuration`）；
   - `bStartOpen` 与主物一致；
3. **Add Component → `Interaction Operation Receiver`**：填 `Channel` + 一条 `Bindings`（见 9.3）；
4. 关卡里复制任意多份 —— **同一 `Channel` 的从物会被同一次交互一起带动**。

**成组**：同一组用同一个 `Channel`（建议 `<关卡>_<组名>`，例如 `Vestibule_LampGroup`），
一组通常 1 主 + N 从；也可以多主共用一个 `Channel`（谁被按都带动整组）。

### 9.5 初始状态一致（`Sync On Begin Play`）

从物不会"自动知道"主物一开始是什么状态，两种做法：

| 做法 | 说明 |
|---|---|
| **手工对齐（默认）** | 主物与从物的 `bStartOpen` 填成一致 |
| **自动同步（推荐给大组）** | 主物勾 **`Sync On Begin Play`** ⇒ 关卡开始后**下一帧**自动发一轮 Entries，把整组拉齐到主物状态 |

> 为什么是"下一帧"而不是 BeginPlay 当场做：关卡里各 Actor 的 BeginPlay 顺序不确定，
> 抢在从物的 `Interactable::BeginPlay` 之前写状态，会被它自己的 `bStartOpen` 覆盖。

### 9.6 与时空（Era）的关系

主从**各自**是否随时空显隐，只取决于各自有没有 `TimeEraComponent`：

- 想让整组只在"现代"出现：给**每一个**成员都挂 `TimeEraComponent` 且 `Era=Modern`；
- 只给主物挂：从物**不会**跟着隐藏（从物只跟随"交互状态"，不跟随 Era）。

> 组绑定解决"**一次交互改变多个物体的状态**"；时空显隐是另一套（`Docs/TimeShift.md`）。两者可叠加。

### 9.7 验证清单

1. 走近主物 → 有提示与描边；**从物不应有任何提示**；
2. 按 E → 日志 `[OperationLink] <主物> -> "Sync" (channel "Group_A"): N receiver(s).`
   （`N` 应等于该组从物数量；`0` 就是寻址没对上，用 `Resolve Entry Targets` 查）；
3. 同时出现 N 条 `[OperationReceiver] <从物>: 应用操作 "Sync"。`；
4. 门应转动、灯应亮灭、墙应显隐；**再按一次 E 应整体回退**（0↔1 两态）；
5. 勾了 `Sync On Begin Play` 时，关卡开始的下一帧应有一轮派发日志。

### 9.8 排查（主从场景）

| 现象 | 原因 |
|---|---|
| 从物被玩家直接交互了 | 从物的 `Interactable` 忘了关 `bEnabled` |
| 从物完全不动，日志有"应用操作" | 从物 `Interactable` 的 `bUseBuiltInToggle` / `bToggleLights` / `bTrackOpenState` 都没开 ⇒ `SetOpen()` 直接返回 |
| 只有"开"有反应，"关"没反应 | 用了旧写法 `Mirror Source State=☑` 但 `Bindings` 只写了一行 |
| 主物按 E 后从物收到的永远是"关" | 主物没有状态位（见 9.1 的 ⚠） |
| 墙方向反了（开 → 墙消失） | 用了 `SetActorHidden`（隐藏 = `bActive`）；改用 **`SetActorVisible`**（显形 = `bActive`） |
| **从物卡在已切换状态、第二次交互没反应** | 见 §9.9 极性陷阱（两行写法必错） |
| Binding 的 Action 显示为 `(INVALID)` | 该值越界（多半来自枚举版本不同的编辑器/分支）⇒ 重新选一次动作即可 |
| 关卡一开始主从不一致 | 用 `Sync On Begin Play`，或把两边 `bStartOpen` 对齐 |
| 同一组里别的物体也动了 | 它们 `Channel` 相同 —— 频道就是"组"的语义，要独立就换频道 |

### 9.9 ★ 极性陷阱：从物"卡在 1 状态"的两个典型原因（实测）

`SetActorHidden` 与 `SetActorVisible` **都读操作里的 `bActive`**，只是极性相反：

| Action | 实现 | `bActive=true` | `bActive=false` |
|---|---|---|---|
| `Set Actor Hidden` | `SetActorHiddenInGame(bActive)` | **隐藏** | **显形** |
| `Set Actor Visible` | `SetActorHiddenInGame(!bActive)` | **显形** | **隐藏** |

**错误写法（实测卡住的配置）**：主物 `MirrorSourceState=☑`、从物两行
`Open→SetActorHidden` + `Close→SetActorVisible`。
第一次交互：`bActive=true`、发 `Open` → 隐藏 ✓；
第二次交互：`bActive=false`、发 `Close` → `SetActorVisible` 在 `bActive=false` 时**还是隐藏** ✗
⇒ 两行都在隐藏，从物看起来"永远停在已切换状态" ✓ 完全吻合"0-1-0 后回到不了 0"。

**推荐写法（单行 + 状态驱动）**——本项目 4 组机关墙已按此重写：

| 端 | 配置 |
|---|---|
| 主物 `Interaction Link → Entries` | `Operation = Open`（名字任意）、`Mirror Source State = ✗`、`Channel = <组名>` |
| 从物 `Bindings` | **只一行**：`Open → Set Actor Hidden` |

于是：主物开 → `bActive=true` → 墙隐藏；主物关 → `bActive=false` → 墙显形 ✓（一次交互一次翻转）。

> 若必须用两行写法（`MirrorSourceState=☑` + `Open`/`Close`），**两行要用同一个动作**
> （都填 `SetActorHidden`），绝不能一行 `Hidden` 一行 `Visible`。

**另一个同症状原因**：Binding 的 `Action` 存成了越界值 —— 编辑器里它读作 `None`、
`export_text()` 显示 `Action=(INVALID)`，命中该行时 switch 落到 default，什么都不做。
多半来自枚举版本不同的编辑器/分支。解决办法：重新选一次动作（本项目 `jiguanqiang3` 就是这一例）。

---

## 10. 现状（已核验）

| 项 | 状态 |
|---|---|
| `UInteractionLinkComponent` | ✅ 已编译并反射（`Entries` 默认空，`bAutoBindInteractable=true`） |
| `UInteractionOperationReceiverComponent` | ✅ 已编译并反射（`Channel=None`，`Bindings` 空，`bEnabled=true`） |
| `IInteractionOperationReceiver` | ✅ 已编译并反射（Blueprintable） |
| `EInteractionOperationAction` | ✅ **八项**：`SET_OPEN / SET_CLOSED / TOGGLE_OPEN / SET_ACTOR_HIDDEN / SET_ACTOR_VISIBLE / MOVE_TO / MIRROR_ACTIVE / NOTHING` |
| `UInteractionLinkComponent::bSyncOnBeginPlay` | ✅ 新增（默认 ✗）：下一帧把整组拉齐到主物状态 |
| 构建脚本 `Scripts/probe_oplink.py` | Python 侧自检用（`hasattr` + 读默认值） |
| 运行时联动验证 | ⏳ 尚未在关卡里实测（需要按 §9 搭一组主 / 从） |
| 主从三件套（门 / 灯 / 墙）的蓝图 | ⏳ **由你搭建**，§9.4 给了逐步清单 |

> 本轮的两个新动作与一个开关都是**追加式**改动：既有枚举数值不变（新项追加在末尾），
> 既有"两行写法"、`Set Actor Hidden` 等行为**完全不受影响**。
