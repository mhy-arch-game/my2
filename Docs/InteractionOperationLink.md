# 交互操作联动（Interaction Operation）：让 A 的交互改变 B 的状态

> 需求：**一个接口面向可交互物品（箱子、灯泡、门）；把"触发对应操作"抽象成一个接口
> （开门、开灯、移动到正确区域内…），使得另一个物品的状态发生对应变化。**

实现方式完全建立在**原先的 interact 内容**之上：发送端接 `UInteractableComponent`，
接收端的默认动作直接调用 `UInteractableComponent::SetOpen()`（同一套状态管着门与灯）。

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

### 5.5 接收端六种内置动作（`Action` 枚举）

| Action | 对 owner 做什么 |
|---|---|
| **Set Open (开 / 亮)** | 有 `Interactable` → `SetOpen(true)`（门开 + 灯亮）；没有 → 直接点亮 owner 上的灯 |
| **Set Closed (关 / 灭)** | 同上，`SetOpen(false)` |
| **Toggle Open (取反)** | 按当前状态取反 |
| **Set Actor Hidden (显隐)** | 按操作里的 `bActive` 隐藏/显示 + 开关碰撞 |
| **Move To (移动落点)** | 把 owner 移到操作里的 `Location`；`Duration > 0` 时插值过去，否则瞬移 |
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
| **A 的交互去改变 B 的状态** | **本文档**：`InteractionLink` + `IInteractionOperationReceiver` |
| 交互后把玩家传送走 | `UTimeEraPortalComponent`（`Docs/TimeEraPortal.md`） |
| 按距离触发、整组显形并永久封死 | `UProximityBarrierComponent`（`Docs/ProximityBarrier.md`） |
| 受光驱动的隐藏面/显形面 | `LightReveal`（`Docs/LightReveal.md`） |

**为什么不把"开门/开灯"硬编码成枚举？** 因为需求里明确要"抽象为一个接口对接"：
操作名是**数据**，收发双方约定即可扩展（`PlayAnimation`、`Unlock`、`Spawn`…），
C++ 不需要改。内置的六种 `Action` 只是"接收端偷懒用的默认实现"。

---

## 9. 现状（已核验）

| 项 | 状态 |
|---|---|
| `UInteractionLinkComponent` | ✅ 已编译并反射（`Entries` 默认空，`bAutoBindInteractable=true`） |
| `UInteractionOperationReceiverComponent` | ✅ 已编译并反射（`Channel=None`，`Bindings` 空，`bEnabled=true`） |
| `IInteractionOperationReceiver` | ✅ 已编译并反射（Blueprintable） |
| `EInteractionOperationAction` | ✅ 六项：`SET_OPEN / SET_CLOSED / TOGGLE_OPEN / SET_ACTOR_HIDDEN / MOVE_TO / NOTHING` |
| 构建脚本 `Scripts/probe_oplink.py` | Python 侧自检用（`hasattr` + 读默认值） |
| 运行时联动验证 | ⏳ 尚未在关卡里实测（需要按 §5 配一组 A/B） |
