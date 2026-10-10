# 距离触发的一次性封死屏障（UProximityBarrierComponent）

> 需求：**角色沿指定方向离开得足够远之后，原本隐藏且无碰撞的整组东西（墙体，含门、灯光等其他
> 组件）显形并变成不可越过；此后该整体永久保留，不再移动。**

与 `LightReveal`（受光驱动、"隐形面 ↔ 实体面"）的分工见 `Docs/LightReveal.md` 第 7 节。

---

## 0. 在编辑器里到底在哪找设置（先看这一节）

### 0.1 名字对照表（找不到东西基本都是名字对不上）

| 你在哪儿找 | 实际叫什么 |
|---|---|
| Add Component 搜索框 | **`Proximity Barrier`**（搜 `proximity` 或 `barrier` 都能命中） |
| Components 面板里 | `ProximityBarrier`；在**蓝图**里会显示成 `ProximityBarrier_GEN_VARIABLE` |
| Details 面板的分类名 | **`Proximity Barrier`**（本次专门为此改过，见 0.5） |
| Blueprint 节点搜索 | `Seal Now` / `Is Sealed` / `Get Signed Distance` / `Get Barrier Debug String` / `Get Effective Trigger Distance` |
| C++ 类名 | `UProximityBarrierComponent` |

### 0.2 ⚠️ 你现在这个编辑器会话里，分类还叫 **`Barrier`**

分类名是在 C++ 的 `UPROPERTY(Category=...)` 里写死的，**改它必须重新编译**。
所以在**尚未重启/重编译的当前会话**里，请在 Details 面板的搜索框里输入：

> **`Barrier`**

你会看到三个**子分类**（`Barrier | Trigger` / `Barrier | Group` / `Barrier | State`）。
**如果三个都看不到，先检查它们是不是被折叠了**——每个子分类左侧有一个小三角，点击展开。
（UE 会**跨对象记住**每个分类的折叠状态，所以"以前折过"会导致现在看起来像"没有属性"。）

重编译之后分类会变成单一的 **`Proximity Barrier`**，届时搜 `proximity` 或 `barrier` 都行。

### 0.3 属性对照表（C++ 名 → Details 里的英文显示名 → 含义）

Details 面板显示的是 UE 自动美化过的名字（去掉 `b` 前缀、驼峰加空格），
所以搜 C++ 名字是搜不到的，请按**显示名**找：

| Details 里显示为 | C++ 名 | 分类(旧/新) | 含义 |
|---|---|---|---|
| **Local Axis** | `LocalAxis` | Trigger | 测距方向（owner 局部空间），默认 (1,0,0) |
| **Trigger Distance** | `TriggerDistance` | Trigger | 触发阈值，默认 1000 |
| **Trigger On Negative Side** | `bTriggerOnNegativeSide` | Trigger | 换到 −LocalAxis 方向 |
| **Require Inside First** | `bRequireInsideFirst` | Trigger | 要求"先靠近、再远离" |
| **Player Index** | `PlayerIndex` | Trigger | 用哪个玩家的 pawn 测距 |
| **Update Interval** | `UpdateInterval` | Trigger | 轮询间隔（默认 0.05 s） |
| **Draw Debug** | `bDrawDebug` | Trigger | 视口画测距轴与触发平面 |
| **Draw On Screen Debug** | `bDrawOnScreenDebug` | Trigger | 屏幕状态行 *（本轮新增，需重编译）* |
| **Extra Actors** | `ExtraActors` | Group | 一起显形的**别的 actor**（门/灯） |
| **Include Attached Actors** | `bIncludeAttachedActors` | Group | 递归带上 attach 的子 actor |
| **Force Collision When Shown** | `bForceCollisionWhenShown` | State | 原本无碰撞的图元显形时强制开碰撞 |
| **Stop Polling After Trigger** | `bStopPollingAfterTrigger` | State | 封死后停止轮询 |
| **Seal Sound** | `SealSound` | State | 封死时播放的音效 |

**Events 区**：Details 面板**最底部**有一段 **Events**（可能被折叠），里面是
**`On Sealed`** —— 点它右边的 **➕** 就能生成事件节点。

### 0.4 三步自检：到底是"没组件"还是"没属性"

**① 组件存不存在？（不用重启也能查）**
编辑器菜单 **Window → Output Log**，把左下角的下拉从 `Cmd` 切成 **`Python`**，执行：

```python
import unreal
print(hasattr(unreal, "ProximityBarrierComponent"))
```

- `True` → 模块里确实有这个类（说明 DLL 已包含它）。
- `False` → 当前编辑器加载的是**旧 DLL**：关掉编辑器重新编译/重启即可。

**② 属性在不在？（顺便看到 Python 侧的属性名）**

```python
import unreal
o = unreal.new_object(unreal.ProximityBarrierComponent)
print(o.get_editor_property("LocalAxis"), o.get_editor_property("TriggerDistance"))
print(o.get_editor_property("bTriggerOnNegativeSide"), o.get_editor_property("bDrawDebug"))
```

能打印出 `(X=1.000,Y=0.000,Z=0.000) 1000.0` 这类值，就说明属性存在且可读。

**③ 属性显示在 Details 的哪一段？**
选中该组件 → Details 搜索框输入 **`Barrier`**（当前会话）或
**`Proximity`**（重编译后）→ 展开看到的分类。

### 0.5 三类"找不到"的原因速查

| 现象 | 原因 | 解法 |
|---|---|---|
| Add Component 里搜 `proximity` **什么都没有** | 编辑器加载的是旧 DLL，类还不存在 | **关掉编辑器 → 重新编译 → 重启**（Live Coding 不能新增 UPROPERTY/UFUNCTION，别用 Ctrl+Alt+F11） |
| 组件加上了，**Details 里看不到属性** | ① 分类折叠了；② 搜的是 `proximity` 而当前分类叫 `Barrier`；③ 没选中组件（选中的是父 actor） | 展开分类三角 / 搜 `Barrier` / 在 Components 面板点中 `ProximityBarrier` 本身 |
| 属性都在，但改了**运行时没反应** | 编辑器还在用旧 DLL（属性是新的但代码是旧的），或没重启 | 关编辑器重新编译后重启 |
| 分类名和我文档写的不一致 | 文档写的是**重编译后**的 `Proximity Barrier`；旧构建里是 `Barrier` | 按 0.2 的说明搜 `Barrier` |

> 变更记录：本轮把 `Category="Barrier|Trigger / Group / State"` 统一改成扁平的
> **`Category="Proximity Barrier"`**。目的有两层：
> ① 让"搜 proximity"也能命中（原来搜不到，正是本节要解决的问题）；
> ② 扁平单一分类没有 `|` 子分类，**不会继承任何历史折叠状态**，属性一定直接可见。
> 该改动**需要重新编译才生效**。

---

## 1. 判定语义：有符号距离

以 owner 的位置为原点，沿 **owner 旋转后**的 `LocalAxis` 投影：

```
D = Dot(PlayerLocation - OwnerLocation, OwnerRotation * LocalAxis)
```

| 配置 | 触发条件 |
|---|---|
| `bTriggerOnNegativeSide = false`（默认） | `D >= +TriggerDistance` |
| `bTriggerOnNegativeSide = true` | `D <= -TriggerDistance` |

**只有"往对应方向远离"才计数**：从反方向靠近、或距离绝对值很大但符号不对，都**不会**触发。
这就是"分正负值 / 只有对应方向"的含义。

因为 `LocalAxis` 是在 **owner 局部空间**解释的，所以**直接旋转墙体 actor 就能改测距方向**，
不用算世界坐标。

`bRequireInsideFirst`（默认关）：打开后要求角色**先进入过**阈值以内
（`|D| < TriggerDistance`）才允许触发，用来避免"出生点本来就在远处"一开局就封死。

## 2. 编排语义：你看到的样子 = 显形后的样子

`BeginPlay` 的顺序：

1. `CollectTargets()` —— 收集整组，并把**当前编排状态记成"显形后的状态"**：
   每个图元的碰撞设置、每个灯的开关与强度、actor 的可见性；
2. `ApplyHiddenState()` —— 整组隐藏 + 关闭碰撞 + 关闭灯光。

所以**在编辑器里按成品摆放、正常调灯就行**，不需要反着配"隐藏态"。

触发时 `ApplyShownState()` 把上面记下的状态还回去。若某个图元原本是 `NoCollision`，
`bForceCollisionWhenShown`（默认开）会强制成 `QueryAndPhysics`——保证"不可越过"不会
因为你忘了配碰撞而失效。

## 3. 这一组包含哪些东西

| 来源 | 说明 |
|---|---|
| owner 本体 | 墙体 actor 自身的全部组件（网格、门组件、灯组件…） |
| `ExtraActors` | 门 / 灯挂在**别的 actor** 上时填这里 |
| `bIncludeAttachedActors` | 默认开，递归带上 attach 到上述 actor 上的子 actor |

隐藏与碰撞走 **actor 级**开关（`SetActorHiddenInGame` / `SetActorEnableCollision`），
这样每个组件自身的设置不会在切换中丢失。
**灯光单独显式处理**（`Light->SetVisibility(...)` + 恢复记录的强度），因为
`SetActorHiddenInGame` 并不保证把灯关掉。

## 4. 属性与接口速查

| 成员 | 默认 | 说明 |
|---|---|---|
| `LocalAxis` | (1,0,0) | 测距方向，owner 局部空间；符号决定哪一侧算"远离" |
| `TriggerDistance` | 1000 | 阈值（≥0） |
| `bTriggerOnNegativeSide` | false | 换到 -LocalAxis 方向 |
| `bRequireInsideFirst` | false | 要求"先靠近、再远离" |
| `PlayerIndex` | 0 | 用哪个玩家的 pawn 测距 |
| `UpdateInterval` | 0.05 s | 轮询间隔（不是每帧） |
| `ExtraActors` | 空 | 门/灯在别的 actor 上时填这里 |
| `bIncludeAttachedActors` | true | 递归带上 attach 的子 actor |
| `bForceCollisionWhenShown` | true | 原本无碰撞的图元显形时强制开启碰撞 |
| `bStopPollingAfterTrigger` | true | 封死后停止轮询 |
| `SealSound` | 空 | 封死时播放的音效 |
| `bDrawDebug` / `bDrawOnScreenDebug` | false | 两种调试可视化（见第 7 节） |
| `IsSealed()` | — | 是否已永久封死 |
| `GetSignedDistance()` | — | 当前有符号距离 |
| `GetEffectiveTriggerDistance()` | — | 当前方向上的实际阈值（负方向时为负） |
| `GetBarrierDebugString()` | — | 一行状态文本，给 Print String / UE_LOG 用 |
| `SealNow()` | — | 忽略距离条件，立刻显形并永久封死 |
| `OnSealed(Barrier, TriggerActor)` | — | 封死瞬间广播一次 |

**锁存**：`bSealed` 一旦为真永不复位，所以"此后始终保留、不再移动"是结构性保证，
与触发时机无关。

---

## 5. 蓝图类如何实现

### 路线 A（推荐，β方案）：挂在已有的墙 actor / 蓝图上

1. 选中墙体 actor（关卡实例），或双击打开它的蓝图。
2. **Add Component → ProximityBarrier**。
3. 按"成品的样子"摆好墙体 / 门 / 灯，调好灯的开关与强度、图元的碰撞——这就是显形后的样子。
4. 设 `LocalAxis` 与 `TriggerDistance`：`LocalAxis` 在 owner 局部空间解释，
   旋转墙体 actor 即可改方向；符号决定哪一侧算"远离"。
5. 若门 / 灯是**独立的 actor**：加进 `ExtraActors`；或把它们 attach 到墙体上
   （保持 `bIncludeAttachedActors` 打开）。
6. 编译并**重启编辑器**后组件才会出现在 Add Component 列表里。

> 注意：不要把 **`InteractionDetector`**（玩家侧组件）挂到墙上——它对非 Pawn 的 owner
> 会自行停用并打 Warning（见 `Docs/InteractionPickingFix.md`）。

### 路线 B：新建一个专用蓝图类

如果这一组东西需要反复复用，就做成蓝图类：

1. Content Browser → **Add → Blueprint Class**。
2. 父类选 **`StaticMeshActor`**（推荐：它自带一个 `StaticMesh` 组件，直接当墙体用；
   若需要多个网格/灯，选 `Actor` 也行）。
3. 命名例如 `BP_ProximityBarrier`，放在 `Content/MHY_ARCH_GAME/Blueprints/`。
4. 打开蓝图，在 Components 面板：
   - 选中继承来的 `StaticMesh` → 指定墙体网格；
   - 需要门/灯时继续 **Add Component** 加 `Static Mesh` / `Spot Light` 等，**并把它们
     attach 到墙体**上（这样 `bIncludeAttachedActors` 会自动带上它们）；
   - **Add Component → ProximityBarrier**。
5. 在组件默认值里设好 `LocalAxis` / `TriggerDistance` / `bTriggerOnNegativeSide`。
   ⚠️ `ExtraActors` 里**不要**放关卡实例（蓝图默认值引用不到具体关卡对象）——
   跨 actor 的引用请放在**关卡实例**上填，或改用 attach 的方式。
6. **Compile + Save**。之后每次拖这个蓝图进关卡就是一个完整屏障。

### 5.1 事件接线（演出）

选中 `ProximityBarrier` 组件 → Details → Events → **`OnSealed`** 后面的 **+** 生成事件节点：

- 接 `Play Sound at Location`（也可以直接用组件上的 `SealSound`，不用连蓝图）；
- 接 `Player Camera Manager → Start Camera Shake` 做震屏；
- 接 `Set Timer by Event` 做后续演出；
- 接 HUD/字幕。

### 5.2 从蓝图主动触发 / 查询

| 节点 | 用途 |
|---|---|
| `Seal Now` | 忽略距离条件立刻封死（例如被另一个触发器驱动） |
| `Is Sealed` | 判断是否已经封死（避免重复演出） |
| `Get Signed Distance` | 自己写条件，或打屏调试 |
| `Get Barrier Debug String` | 直接 `Print String` 出完整状态 |

---

## 6. 调试

### 6.1 三种可视化，按需开

| 开关 | 看到什么 |
|---|---|
| `bDrawDebug` | 视口里画**青线 = 测距轴**、**青点 = 触发平面**、**黄点 = 原点**（世界空间） |
| `bDrawOnScreenDebug` | 屏幕左上角一行实时状态（用组件指针当 key，只覆盖不刷屏） |
| 蓝图 `Get Barrier Debug String` → `Print String` | 同一行文本，挂在任意事件里随时打 |

状态行格式：

```
Barrier <owner> | sealed=no | signed=812.3 (along X=1.000 Y=0.000 Z=0.000) | need >=1000.0 | inside=no | group 1 actor / 3 prim / 1 light
```

一眼能看出四件事：**当前有符号距离**、**需要多远**、**是否已经进入过阈值内**、**组里收进了多少东西**。

### 6.2 日志（每次生命周期只打两条）

```
[Barrier] <owner>: group = N actor(s), M primitive(s), K light(s).
[Barrier] <owner>: sealed at signed distance 1234.5 - the group stays materialised.
```

- 第一条在 `BeginPlay` 出现。**`N / M / K` 不符合预期就是配置问题**——最常见的是
  门/灯是独立 actor 却没进 `ExtraActors`，或者没有 attach 到墙体上。
- 第二条只在真正封死时出现。没出现就是距离条件没满足（看第 6.1 节的距离值）。

### 6.3 看碰撞（确认"不可越过"）

PIE 里执行 **`show Collision`**：

| 看到 | 含义 |
|---|---|
| 显形前墙体的碰撞线框**不存在** | 正确：隐藏态无碰撞 |
| 显形后墙体的碰撞线框**出现** | 正确：已是实体 |
| 显形后**仍然没有**线框 | 该图元的碰撞没恢复——检查是不是被别的系统在改（例如它同时在某个 `bDisableCollisionWhenOpen` 组里） |
| 墙体看不见但**走不过去** | 反过来：还有别的东西在挡（用 `show Collision` 找线框来源），或 `ApplyShownState` 之外的碰撞没关干净 |

### 6.4 常见故障表

| 现象 | 原因 | 解法 |
|---|---|---|
| 一开局就封死 | 出生点本来就在阈值外 | 打开 `bRequireInsideFirst`，或把 `TriggerDistance` 调大 / 把原点摆到正确一侧 |
| 走到很远处也不触发 | 方向反了 | 勾 `bTriggerOnNegativeSide`，或把 `LocalAxis` 取反 |
| 反方向靠近反而触发了 | 同上（符号） | 同上；用 `bDrawDebug` 看青线朝向确认哪边是"正" |
| 门/灯没有跟着变 | 它们是独立 actor | 加进 `ExtraActors`，或在编辑器里 attach 到墙体上 |
| 灯亮着/灭着不对 | 灯的开关是"显形后"的状态 | 因为编排语义是"你看到的样子 = 显形后的样子"，请在编辑器里把灯调成**显形后应有的样子** |
| 封死后又自己隐藏了 | 不应发生（锁存） | 若有别的系统在写同一个 actor 的可见性（例如 `TimeEraComponent` 按时代门控），两者会打架 → 不要让同一个 actor 同时受两种门控 |
| 隐藏期间就能跟里面的门交互 | 已被 `IsHidden()` 拦掉 | 若仍能交互，说明那个可交互物**没有被隐藏**（不在组里）→ 检查组范围日志 |

---

## 7. 与交互系统的联动

隐藏的 actor **仍然会挡住交互射线**：它自己的碰撞虽然关了，但 `UInteractableComponent`
为"开启后仍可检测"创建的**交互代理盒**只挡 Visibility、不受 actor 隐藏影响。若不处理，
屏障显形之前玩家就能隔空交互到里面的门。

因此 `UInteractionDetectorComponent::IsInteractableTarget()` 增加：

```cpp
if (Target->IsHidden())
{
    return false;   // 隐藏的 actor 不可交互
}
```

这条对所有被隐藏的对象都生效，不只是屏障组。

---

## 8. 现状 / 待办

| 项 | 状态 |
|---|---|
| `UProximityBarrierComponent` 实现 | ✅ 已编译进模块（上一次成功构建） |
| `bDrawDebug` / 日志 / `SealNow` / `OnSealed` | ✅ 已编译 |
| `bDrawOnScreenDebug` / `GetBarrierDebugString` / `GetEffectiveTriggerDistance` | ⚠️ **源码已写，尚未编译**（编辑器开着时 UBT 被 Live Coding 挡住；UHT 已通过） |
| Details 分类统一为 `Proximity Barrier`（原 `Barrier|Trigger/Group/State`） | ⚠️ **源码已改，尚未编译**；未重编译前请按第 0.2 节搜 `Barrier` |
| ⚠️ 这两个改动**必须重编译 + 重启编辑器**才生效 | 不要用 Live Coding（Ctrl+Alt+F11）：新增 UPROPERTY/UFUNCTION 与改分类都属于反射层变更，Live Coding 处理不了，可能让模块状态不一致 |
| 在关卡里挂实例做 PIE 验证 | ⏳ 待做（挂好后先看第 6.2 节的 group 日志） |
| 蓝图类 `BP_ProximityBarrier` | ⏳ 未创建（按路线 B 手工建，约 30 秒；或告知墙体名称由脚本代建） |
