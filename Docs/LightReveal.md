# LightReveal：墙/地板的"消失 ↔ 出现"（受光显形）

> 这一层负责**面**的显隐与通行性：默认是"看不见、也踩不住"的隐形面，被"光"照到之后
> 变成可见且可站立/可阻挡的实体。**"墙消失"= 未受光态，"墙出现"= 受光态。**

## 1. 结构：两层 + 一个接口，完全解耦

```
ARevealLightVolume（光区触发器）  --RevealOn/RevealOff-->  IRevealableInterface  -->  ARevealPlatform（面）
```

接口只有两个方法（`LightReveal/Interfaces/RevealableInterface.h`）：

| 方法 | 含义 |
|---|---|
| `RevealOn()` | 被照亮 → 面变为**可通行**（可见 + 有碰撞） |
| `RevealOff()` | 光离开 → 面回到**不可通行**（隐藏 + 无碰撞） |

光区只持有 `AActor*` 并**只通过接口**跟面说话，不知道具体平台类型。

## 2. 状态语义（最容易搞反的地方）

| 状态 | 可见性 | 碰撞 | 玩家体验 |
|---|---|---|---|
| 未受光（impassable，默认） | **隐藏** | **关闭** | 直接穿过去 / 掉下去 |
| 受光（revealed，passable） | **显示** | **开启** | 可以站上去、被挡住 |

两个设计细节值得注意：

- **引用计数**：`RevealOn`/`RevealOff` 是成对的计数（`RevealRefCount`），所以同一个面被多个光区
  同时照到时不会提前熄灭。
- **不会把玩家扔下去**：平台上有一个 **`DetectorBox`**（只做 overlap、向上延伸），
  记录"当前有几个角色站在上面"。`RevealOff` 会等到没人站着才真正隐藏（`bUnrevealPending`）。

## 3. `ARevealPlatform` 的关键属性

| 属性 | 默认 | 说明 |
|---|---|---|
| `bStaysRevealed` | false | **单向门**：一旦被照过就永久可通行，不再熄灭 |
| `OpacityParameterName` | `Opacity` | 若 `VisualMesh` 的材质有这个标量参数，就由它驱动淡入淡出 |
| `SolidBox` | — | 物理支撑；碰撞随显隐开关，**阻挡角色**（不产生 overlap 事件） |
| `DetectorBox` | — | 仅 overlap 的检测盒，向上延伸；用于"有没有人站在上面" |
| `VisualMesh` | — | 不可通行时隐藏的可见网格 |
| `IsRevealed()` | — | BlueprintPure，当前是否可通行 |

## 4. `ARevealLightVolume` 的关键属性

| 属性 | 默认 | 说明 |
|---|---|---|
| `RevealedActors` | 空 | 被点亮时要 reveal 的 actor（应实现 `IRevealableInterface`） |
| `bAnyPawnCarriesLight` | true | 任意 Pawn 进入即算"带来光" |
| `LightZone` | — | 触发器盒（overlap） |

只要**至少一个**合格载体在盒内，这组面就一直亮；最后一个离开才熄灭
（除非面的 `bStaysRevealed` 已把它锁成永久，或还有别的光区在照）。

## 5. 蓝图类如何实现

已有两个现成的蓝图类（父类就是对应 C++ 类）：

| 蓝图 | 父类 |
|---|---|
| `/Game/MHY_ARCH_GAME/Blueprints/BP_RevealLightVolume` | `ARevealLightVolume` |
| `/Game/MHY_ARCH_GAME/Blueprints/BP_RevealPlatform` | `ARevealPlatform` |

### 5.1 放一个光区（BP_RevealLightVolume）

1. 拖进关卡，摆到"有光/有光源"的位置。
2. 调 `LightZone` 的大小：选中组件后直接缩放，或在 Details 里改 Box Extent。
   **注意它应当是 overlap 盒**——角色走进去就算"带来光"，不需要真的做光照计算。
3. ⚠️ **最容易漏的一步**：Details → **LightReveal → Revealed Actors** 里**手动加入**要受光显形的
   `BP_RevealPlatform` **关卡实例**。
   该属性是 `EditInstanceOnly`，**只能在关卡实例上填，不能填在蓝图默认值里**
   （也因此它不能是"所有平台自动生效"，必须逐个指定）。
4. 谁能带光：
   - `bAnyPawnCarriesLight = true`（默认）→ 角色进入即可；
   - 关掉它 → 只有带 **`LightCarrier`** Actor Tag 的对象才算带光（例如你可以让一个提灯道具带光）。

### 5.2 放一个面（BP_RevealPlatform）

1. 拖进关卡，缩放成一块地板 / 一面墙的形状。
2. `SolidBox` 与 `DetectorBox` 的比例要匹配：`DetectorBox` 应比 `SolidBox` **更高一点**
   （默认已配好）。`DetectorBox` 是"有人站在上面"的判定区，太小会导致玩家站上去后平台被熄灭。
3. 外观（可选）：给 `VisualMesh` 指定网格与材质；如果材质里有标量参数（默认名 `Opacity`），
   把名字填进 `OpacityParameterName` 就能做淡入淡出。
4. **想做成"此后永久保留、不再恢复隐藏"**：勾 **`bStaysRevealed`** —— 这就是单向门。
5. 想复用同一套逻辑换外观 → 基于 `BP_RevealPlatform` 建子类即可（网格/材质/音效都随子类走）。

### 5.3 想自己写一个"受光显形"的对象？

⚠️ **`IRevealableInterface` 是 `NotBlueprintable`**（`RevealableInterface.h:21`），
所以**蓝图里无法实现这个接口**。只有两条路：

| 路线 | 做法 |
|---|---|
| (a) C++ 子类 | 新建 `AActor` 子类，照抄 `ARevealPlatform` 的模式实现 `RevealOn/RevealOff` |
| (b) 复用平台 | 直接拿 `ARevealPlatform` / `BP_RevealPlatform` 改网格和材质——它已经处理好引用计数和"玩家站着时不熄灭" |

## 6. 如何调试

> ⚠️ **现状**：`ARevealPlatform` / `ARevealLightVolume` 目前**没有任何 `UE_LOG`**
> （已全目录确认）。所以这一层的调试主要靠下面这些手段，而不是看日志。
> 建议后续补两三条日志（见第 8 节）。

### 6.1 运行时确认状态

- 在关卡蓝图 / 角色蓝图中对平台实例调用 **`IsRevealed`**（BlueprintPure）→ `Print String`，
  就能看到"当前是否可通行"。
- 想看光区里的载体计数：目前没有暴露接口，**最快的办法是看 `LightZone` 是否与角色重叠**——
  PIE 里选中角色，看它和光区盒的位置关系。

### 6.2 看碰撞（这是最直观的）

PIE 里执行 **`show Collision`**：

| 看到 | 含义 |
|---|---|
| `SolidBox` 有线框 | 平台是**可通行**态（受光中） |
| `SolidBox` 没有线框 | 平台是**不可通行**态（隐藏中）——玩家会掉下去 |
| `DetectorBox` 有线框 | 检测盒存在（它始终存在，用于判断有没有人站着） |

### 6.3 常见故障

| 现象 | 原因 | 解法 |
|---|---|---|
| 走进光区，平台还是踩不住 | `RevealedActors` 没填 | 在**关卡实例**的 Details 里把平台加进 `Revealed Actors`（`EditInstanceOnly`，蓝图默认值里填没用） |
| 同上，但填了 | 光区盒没和角色重叠 | 选中 `LightZone` 看盒体范围；角色是 Pawn，默认 `bAnyPawnCarriesLight=true` 就该算 |
| 同上，但只认特定物体 | `bAnyPawnCarriesLight` 被关了 | 给带光的那个 actor 加 Tag **`LightCarrier`** |
| 站上平台后被弹下去/掉下去 | `DetectorBox` 太小或没覆盖站立面 | 把 `DetectorBox` 放大、向上延伸，确保玩家站在上面时确实与之重叠 |
| 多个光区重叠时平台闪 | 正常情况下引用计数会保护；若仍闪，检查是不是有光区在重叠瞬间反复进出 | 用 `bStaysRevealed` 做单向门，彻底避免反复 |
| 想要"永久保留" | `bStaysRevealed` 默认 false | 勾上 `bStaysRevealed` |

## 7. 与 `ProximityBarrier` 的分工（什么时候用哪个）

| 需求 | 用哪个 |
|---|---|
| 受**光照/区域**驱动，面在"隐形可穿过 ↔ 实体可站立"之间切换 | **LightReveal**（本文档） |
| 受**角色沿某方向的距离**驱动，整组（墙+门+灯）一次性**显形并永久封死** | **`UProximityBarrierComponent`**（见 `Docs/ProximityBarrier.md`） |
| 已经有一个可见的墙，只想让它**消失** | 用 `ProximityBarrier` 的反向门槛做不到；用 `LightReveal` 的平台（默认就是隐藏态），或把墙挂进一个 `bStaysRevealed=false` 的平台组 |

## 8. 建议补充（尚未做，等可以编译时一起）

这两层目前**零日志**，建议在以下位置各加一条：

- `ARevealPlatform::RevealOn / RevealOff` → 打印 `RefCount` 与"是否因为有人站着而延后隐藏"；
- `ARevealLightVolume` 的 overlap begin/end → 打印载体数，以及 `Qualifies()` 失败的原因
  （"不是 Pawn" / "没有 LightCarrier tag"）。

这样第 6.3 节的排查就能直接从日志定位，而不用靠视觉判断。
**当前不做**：编辑器正开着，UBT 会被 Live Coding 挡住，改动的 C++ 无法编译验证，
不把未验证的代码堆进你的下一次构建。
