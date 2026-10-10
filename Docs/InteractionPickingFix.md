# 交互拾取 / 门洞通行：实测分析与修复

## 0. 实测基线

玩家侧探测器（`/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter` 上的 `InteractionDetector`）：
`PickMode=LINE_TRACE`、`TraceDistance=400`、`InteractionRadius=250`、`MinFacingCosine=0`、
`bDrawDebug=true`、`FocusStickinessBonus=150`、`InteractionAction=IA_Interact`。

几何：门板 `Door` 世界厚 12.0 cm；门框 `Door_Frame` 14.7 cm；门洞净宽 113.6 × 高 247.9 cm；
玩家胶囊半径 34.0（直径 68）高 192 cm。门框两侧都比门板凸出（+X 面 1.56 cm，-X 面 1.12 cm）。

## 1. 需求：物体处于开启（切换过）状态后仍要能被检测，**不再依赖持续聚焦**

原实现只在拾取射线命中时才有焦点；门一开：

- 门板（唯一有碰撞的部件）**转出了准星**；
- `bDisableCollisionWhenOpen=true` 又把它的碰撞切成 `NoCollision`；

于是瞄准射线和重叠兜底**都找不到任何东西**，只有靠 `IsStillValidTarget` 保留陈旧焦点才能在第二次按 E 时关上——
这正是要取消的前提。

### 修复：交互代理（Interaction Proxy）

`UInteractableComponent` 在 `BeginPlay` 于**关闭姿态**创建一个不可见的查询盒：

| 属性 | 默认 | 说明 |
|---|---|---|
| `bUseInteractionProxy` | true | 只有 `bUseBuiltInToggle` 为真时才创建 |
| `InteractionProxyExtent` | ZeroVector | 零 = 由开关组件的关闭姿态包围盒自动推导 |
| `InteractionProxyPadding` | 0 | 每个轴额外放大的半长；填 1~2 可让代理越过凸出的门框止口 |

代理的碰撞设置是**只对交互射线可见**：

```cpp
Proxy->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
Proxy->SetCollisionObjectType(ECC_WorldDynamic);
Proxy->SetCollisionResponseToAllChannels(ECR_Ignore);
Proxy->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
Proxy->SetHiddenInGame(true);
Proxy->SetVisibility(false);
```

- 它**永不移动**，所以门开着也照样能被瞄准射线命中 → 第二次按 E 必然可用；
- 它**只挡 Visibility**，不挡 Pawn/物理 → 玩家可以直接穿过去，也不会被它绊住;
- 它是不可见的，而描边只处理可见图元，所以不会被描边。

## 2. 让射线直接报出"是谁挡住的"

调试颜色骗过人（Kismet 的绿=命中几何体，与是否可交互无关），所以现在由组件自己画：
**绿=真的选中了可交互对象，红=没有**。同时把命中过程写进日志：

```
[Interaction] <pawn>: pierced non-interactable 'menkuang2' to reach 'men2'.
[Interaction] <pawn>: aimed pick found NO interactable - the ray hit 'Combined' first.
```

两行都只在该遮挡物**变化时**打印一次，不会刷屏。
**站在门的两侧各看一眼日志，就能直接知道是谁挡住了。** 这是本轮最快的现场定位手段。

拾取本身还有两层保险：`bPierceOccluders`（默认开，改用 `LineTraceMulti` 收集射线上
**所有**可交互对象）与 `bFallbackToOverlap`（默认开，瞄准拾取为空时用半径重叠兜底，
朝向仍由 `ScoreCandidate` 强制）。

## 3. 门洞通行

### 已做：门框不再阻挡玩家

`menkuang*`（17 个 `StaticMeshActor`）的网格组件碰撞预设改为内置 **`IgnoreOnlyPawn`**
（= 只忽略 Pawn 通道，其余照常阻挡）。不触碰 `Door_Frame` 资源本体，可一行还原。

**跨进程核验**（保存 → 重新 `load_map` → 再读）：

| 项 | 值 |
|---|---|
| profile | 17/17 = `IgnoreOnlyPawn` |
| `ECC_Pawn` 响应 | 17/17 = `ECR_IGNORE` |
| 关卡文件 | 943.4 → 952.6 KB（确认真的写盘） |

### 但真正压住门洞的，很可能不是门框

包围盒相交分析发现门洞处还压着一整块**合并后的建筑网格**：

| 门 | 压在门洞上的 actor | extent | Z 范围 |
|---|---|---|---|
| `men2` | `Combined`（`Combined_4D61396D`） | **4420 × 7000 × 270** | `[-20, 520]` 罩住门洞 `100..347.9` |
| `men3` | `CubeGridToolOutput2` | 850 × 850 × 3850 | `[0, 7700]` 罩住门洞 `1300..1547.9` |

门是**贴/埋在**这层建筑网格上的（这也解释了单面失效：门被埋进墙里，一面看得见门，另一面先打到墙）。
**这一层我无法从脚本侧判定"墙上是否真有洞"**：

- commandlet 里物理查询完全不可用（连正对门板的射线都返回空）；
- `EditorStaticMeshLibrary.get_simple_collision_count()` 返回 -1（碰撞未构建）；
- `BodySetup.AggGeom` 的 `box_elems`/`convex_elems` 在这版 Python 里没有暴露；
- 无渲染（`-nullrhi`），没法截图看。

### 30 秒判定 + 对应解法

1. **PIE 里执行 `show Collision`**（控制台）→ 直接画碰撞线框，门洞被谁封死一眼可见；
2. 或者走到门两侧看日志 —— 新日志会点名挡住交互射线的那个 actor。

| 看到的 | 结论 | 解法 |
|---|---|---|
| 门框的碰撞线框封住洞口 | 门框碰撞是实心板 | 已修：`IgnoreOnlyPawn`（本轮）；若想让门框仍然挡其他人，可给 `Door_Frame` 资源改用 Complex Collision 或重建空心简单碰撞 |
| `Combined` 的碰撞线框横跨洞口 | 合并后的墙没有真正的洞 | 需要用建模工具在墙上开出洞口（或把该处拆分出来单独处理），代码改不动它 |
| 只有门板线框 | 门没开 | 修交互（本轮已完成） |

## 4. 本轮踩到的编辑器 Python 坑（都很隐蔽）

1. **`get_editor_property("body_instance")` 返回的是副本**。改完响应后把这个副本写回去，
   **会把刚改的响应覆盖掉**——我曾因此得到"设置成功但值没变"的假象。
2. **逐通道碰撞响应不会随关卡保存持久化**（内存里对，重新加载变回 `ECR_BLOCK`），
   而 **profile 名会持久化**。所以正确的做法是用碰撞预设（`IgnoreOnlyPawn`）而不是逐通道改。
3. **`save_current_level()` 可能是空操作**（返回 True 但文件 mtime 不变 = 没有东西被标脏）。
   可靠做法：先 `actor.modify()` / `component.modify()`，改动后带
   `notify_mode=PropertyAccessChangeNotifyMode.ALWAYS` 把结构体推回去，再用
   `EditorLoadingAndSavingUtils.save_map(world, map_path)` 落盘。
4. 枚举名：`ECR_IGNORE` 属于 **`unreal.CollisionResponseType`**（不是 `CollisionResponse`）；
   `unreal.CollisionChannel.ECC_PAWN` 保留 `ECC_` 前缀，而 `unreal.CollisionEnabled.NO_COLLISION` 不带前缀。
5. `USceneComponent::GetLocalBounds()` 在 5.7 返回 `FBoxSphereBounds`（无出参）；要世界空间包围盒
   直接用 `UPrimitiveComponent::Bounds`。

## 5. 现状清单

| 项 | 状态 |
|---|---|
| 交互代理（开启后仍可检测） | ✅ 已实现并编译 |
| 穿透遮挡 + 重叠兜底 + 自绘调试色 | ✅ 已实现并编译 |
| 射线点名遮挡物日志 | ✅ 已实现并编译 |
| 门框不再阻挡玩家 | ✅ 已写入并跨进程核验 |
| 门蓝图里多余的 `InteractionDetector` | ✅ 已不在（0/17） |
| 门洞是否真被墙封死 | ⏳ 需 PIE 里 `show Collision` 或看日志判定 |
| **编辑器需重启**才能加载新 DLL | ⏳ 待你操作 |
