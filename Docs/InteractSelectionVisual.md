# 交互“选中”视觉效果：选型与实现方案

> 需求：interact 模块里**被聚焦/选中的物体**要有明确的“被选中”视觉反馈。
> 本文先说清**现状**（C++ 其实已经写完一半），再给可选方案、逐步实现方案与验收方法。

## 0. 现状：C++ 端已就绪，渲染端全缺

### 0.1 已经有的（UInteractionDetectorComponent）

| 能力 | 说明 |
|---|---|
| 聚焦即写 custom depth + stencil | ApplyFocusOutline() 对目标**所有可见图元**写 SetRenderCustomDepth(true) + SetCustomDepthStencilValue(FocusedOutlineStencil)（默认 **2**）|
| 备份 / 还原 | 每个被改的图元都存一份 FInteractionOutlineBackup，失焦或组件销毁时 ClearFocusOutline() 精确还原（不会残留）|
| 只描可见图元 | bOutlineVisiblePrimitivesOnly 开启 —— 交互代理盒/探测器这类隐藏碰撞体不会产生多余描边 |
| 厚度走 MPC | OutlineParameterCollection + OutlineThicknessParameter（默认 OutlineThickness）：聚焦推 FocusedOutlineThickness(4.0)，静息推 RestingOutlineThickness(1.5) |
| 另一套自带描边 | UStructureBlockComponent 有 bUseOutline / HighlightStencilValue（关卡里暂无实例）|

### 0.2 缺的（所以现在**看不到任何选中效果**）

| 缺失 | 后果 | 现状 |
|---|---|---|
| r.CustomDepth 未设置 | 引擎**不分配 stencil**，写入的 stencil 没人能读；引擎警告原文：PostProcessMaterial uses stencil test, but stencil not allocated. Set r.CustomDepth to 3 to allocate custom stencil. | **已修复**：Config/DefaultEngine.ini 的 [/Script/Engine.RendererSettings] 已写入 r.CustomDepth=3 |
| 没有描边后处理材质 | 没有东西去读 stencil 并画线 | 待建（见 2.1）|
| 关卡里 PostProcessVolume 的 blendables **是空的** | 材质无处挂载 | 待挂（见 2.1 步骤 3）|
| MPC 未创建、探测器上未指 | 厚度参数无效（不影响出线）| 可选（见 2.1 步骤 4）|

## 1. 选型总览（先选方向）

| # | 方案 | 视觉表现 | 需要什么 | 引擎设置 | 成本 | 适合 |
|---|---|---|---|---|---|---|
| **A** | **Stencil 后处理描边**（推荐）| 物体轮廓外一圈干净的线，可做成穿透 | PP 材质 + blendable | r.CustomDepth=3 | 低（全屏 1 次采样）| 通用、最“标准”的选中反馈 |
| **B** | **Overlay Material 覆盖材质** ✅ **已落地**（见第 5 节）| 物体自身像素上的边缘光/发光 | 1 个 Surface 材质（已建）| 无 | 极低 | **当前采用**：所有可交互物聚焦即生效 |
| **C** | 动态材质实例 / Custom Primitive Data 染色 | 物体自身发光/呼吸/扫描 | 材质要有参数 | 无 | 低 | 美术向、要与美术风格融合 |
| **D** | 反向壳（倒法线外扩）描边 | 粗卡通描边 | 复制网格 + 壳材质 | 无 | 中高（draw call 翻倍）| 卡通风格、单个简单网格 |
| **E** | Stencil 简易染色（无边缘检测） | 选中物体**整块**变色 | PP 材质（4~6 节点）| r.CustomDepth=3 | 极低 | 兜底方案、10 分钟出效果 |
| **F** | 世界空间图标/标记 | 物体上方一个图标/箭头 | 图标资产（或复用提示弹窗）| 无 | 极低 | 补充“这里能互动”，但不是“选中”本身 |
| **G** | 穿透高亮（X-ray）| 隔着墙也能看到轮廓 | A 的材质加一路分支 | 同 A | 低 | 解谜找物、要被遮挡也可见 |

**推荐组合**：**A 为主**（轮廓最像“选中”），需要穿透就加 **G**，**F** 已有交互提示可顺带补图标；
若想先看到东西再定风格，**先用 E 或 B 落地**，之后升级到 A（C++ 一行都不用改）。

## 2. 各方案的实现步骤

### 2.1 A（推荐）Stencil 后处理描边

**前置（已完成）**：`Config/DefaultEngine.ini` →

```ini
[/Script/Engine.RendererSettings]
r.CustomDepth=3
```

**步骤 1 · 建后处理材质 `M_PP_InteractionOutline`**

- Material Domain = **Post Process**；Blendable Location 二选一：`Before Tonemapping`（与场景色一致）/ `After Tonemapping`（更 UI 感，颜色不受曝光影响）；
- 节点配方（核心思路：**自己不是选中物、但邻居是 ⇒ 描边**）：

```
ScreenPosition/TextureCoordinate(ViewportUV) ─┬─> (自身) SceneTexture:CustomStencil ─> StencilSelf
                                             └─> 8 个方向偏移(±Thickness/ViewSize) ─> 各采一次 CustomStencil ─> Max ─> StencilNeighbor

mask = saturate( (StencilNeighbor == FocusedStencil) - (StencilSelf == FocusedStencil) )
最终颜色 = Lerp( SceneTexture:PostProcessInput0 , OutlineColor , mask * Intensity )
输出 -> Emissive Color
```

- **注意量程**：材质里读到的 CustomStencil 是 **0–255 的原始值**（引擎 `CalcSceneCustomStencil()` 返回 uint 直接转 float），
  所以比较写 `== 2`（聚焦档）或 `>= 1`（若以后加静息档），不要跟 0–1 混用；
- 厚度：把 8 个方向的偏移量乘 `CollectionParameter: OutlineThickness`（见步骤 4），这样 C++ 推的 1.5 / 4.0 直接控制粗细；
- **先跑通再美化**：最简版本就是 `SceneTexture:CustomStencil == 2` ⇒ `Lerp(SceneColor, 描边色, 1)`（即方案 E），
  确认 stencil 通了（能看到整块变色）再加边缘检测。

**步骤 2 · 确认材质能编译**（材质编辑器无红色错误；Post Process 域必须连到 `Emissive Color`）。

**步骤 3 · 挂到后处理（二选一，别同时挂两处）**

- **推荐**：关卡里已有的 PostProcessVolume（`bUnbound = True`，无限范围）→ `Rendering Features → Post Process Materials` 加一项指向该材质，Weight = 1；
- 或者：给 `BP_FirstPersonCharacter` 加一个 PostProcessComponent（`bUnbound = 开`）同样挂上去（好处是换关卡也带着走）。

**步骤 4 · （可选）MPC 让厚度可调**

1. 新建 Material Parameter Collection：`/Game/MHY_ARCH_GAME/UI/MPC_InteractionOutline`，加 Scalar `OutlineThickness`，默认 `1.5`；
2. 材质里步骤 1 的偏移量乘这个参数；
3. **在 `BP_FirstPersonCharacter` 的 InteractionDetector 组件默认值里**把 `OutlineParameterCollection` 指到这个 MPC；
   注意：要改**蓝图上的组件默认值** —— 游戏里的 Pawn 是从蓝图类生成的，改关卡里那个实例读不到（这一点很容易踩）。

**步骤 5 · （可选）两档描边**：给“可交互但未聚焦”的物体写 stencil = 1（细），聚焦写 2（粗）。
现有 C++ 只写聚焦那一档（`FocusedOutlineStencil`，默认 2）；要待机档需要在探测器刷新 / `UInteractableComponent` 里补一段
“给场景里所有可交互图元写 1”的逻辑（可以做成开关 `bWriteRestingStencil`，需要的话我补）。

### 2.2 B Overlay Material（最快看到效果）

1. 建材质 `M_Overlay_Focus`：Domain = Surface、Blend Mode = `Translucent`（或 `Additive`）、Shading Model = `Unlit`；
   内容：`Fresnel(Exponent≈3) × 颜色` → `Emissive Color`（透明度也可用同一值）；
2. C++ 扩展点：在 `ApplyFocusOutline()` 里对每个图元 `Cast<UMeshComponent>->SetOverlayMaterial(Material)`，
   在 `ClearFocusOutline()` 里还原（**同样要备份原 overlay**，与现有 stencil 备份一个套路）—— 这部分我可以直接加；
3. 组件上暴露一个 `TObjectPtr<UMaterialInterface> FocusOverlayMaterial`，不填就退回现有 stencil 方案。

### 2.3 C 动态材质实例 / Custom Primitive Data

- 材质侧：可交互物的材质加 Scalar `HighlightAmount`（或勾上 `Use Custom Primitive Data` 用 CPD 的 float 槽）；
- 代码侧：聚焦时 `CreateAndSetMaterialInstanceDynamic` 后 `SetScalarParameterValue("HighlightAmount", 1)`，失焦归零；
  用 CPD 则 `Primitive->SetCustomPrimitiveDataFloat(0, 1.0f)`（不额外产生 DMI，性能更好）；
- 优点：表现最自由（呼吸 / 扫描线 / 描边 + 流光），也最贴美术；缺点：**依赖美术把参数接进材质**，纯贴图或合并网格上不好做。

### 2.4 D 反向壳描边

- 壳材质 `M_Outline_Shell`：`Unlit`、双面关、法线取反或 `WorldPositionOffset = Normal * 外扩量`；
- 代码侧：聚焦时给目标临时加一个同网格组件（或用 overlay 的进阶做法），缩放 ×1.02，失焦移除；
- 本项目注意：机关墙是**合并大网格**，加壳的 draw call 与穿帮风险都偏高，一般不做首选。

### 2.5 E Stencil 简易染色（10 分钟兜底）

- Post Process 材质：`SceneTexture:CustomStencil` → `Compare(==2)` → `Lerp(SceneColor, TintColor, 0.35)` → `Emissive Color`；
- 只需 `r.CustomDepth=3`（已配）+ 挂到后处理（同 2.1 步骤 3）；没有轮廓线，但“选中感”立刻可见，适合先验证链路。

### 2.6 F 世界空间标记

- 复用现成的 `UInteractionPromptComponent` 世界空间弹窗（已实现），再叠一个图标（Widget 或 Sprite 材质）；
- 只解决“这里能交互”的提示，不解决“物体本身被选中”，建议与 A 组合使用。

### 2.7 G 穿透高亮（X-ray）

- 在 A 的材质里另加一路：不比较“自身像素”的 stencil，而是“邻域有 stencil”就画 ⇒ 被墙挡住的物体也会漏出轮廓；
- 或对选中图元单独走一个关深度测试的 pass（材质里开 `DisableDepthTest`）；
- 解谜场景很常用，但**不要全程都开**（会看不清前后空间关系）。

## 3. 参数与验收

| 参数（组件上）| 默认 | 作用 |
|---|---|---|
| `bApplyFocusOutline` | 开 | 总开关 |
| `bUseStencilOutline` | 开 | 是否写 stencil（关掉则完全不动渲染状态）|
| `FocusedOutlineStencil` | 2 | 聚焦档 stencil 值（材质按它比较）|
| `bOutlineVisiblePrimitivesOnly` | 开 | 隐藏碰撞代理不描边 |
| `FocusedOutlineThickness` / `RestingOutlineThickness` | 4.0 / 1.5 | 经 MPC 调线宽 |
| `OutlineParameterCollection` / `OutlineThicknessParameter` | 空 / `OutlineThickness` | MPC 与参数名 |

**验收清单**

1. 对着门 / 灯 / 传送器 / 机关墙：出现描边（或染色），移开视线后**立即消失且无残留**；
2. 快速左右扫视、连续切换目标：不闪残留、不叠加；
3. **隐藏的交互代理 / 探测器**不该描边（`bOutlineVisiblePrimitivesOnly` 开着时应看不到）；
4. 多图元物体（门 = 门板 + 门框）应**整组**一起描边；
5. 被墙挡住时要不要看到（决定是否上 G）；
6. 性能：`stat unit` / `stat gpu` 对比 `r.CustomDepth 0 / 1 / 3`，确认 CustomDepth pass 开销可接受。

## 4. 需要你定的三件事

1. **选哪个组合**：推荐 A（+ 可选 G）；想先看到效果就先 E 或 B。
2. **材质谁做**：美术手工（本文 2.1 已给节点配方）/ 我程序化生成（可以做，但材质渲染效果无法在无窗口环境里验证，需要你在编辑器里看一眼再定）。
3. **要不要两档描边**（待机细 + 聚焦粗）：要的话我补一段“给所有可交互图元写 stencil=1”的代码（带开关）。

## 6. 实测复盘：一开始“没有亮光”的两个原因与修法（2026-10）

| # | 原因 | 修法 |
|---|---|---|
| 1 | 材质只用 `Fresnel → Opacity`：**正对表面时 Fresnel 接近 0**（默认下限 0.05）⇒ 几乎全透明，平视门/墙根本看不见 | 改成 `Opacity = clamp(Fresnel + FocusBaseFill)`（默认 0.45），并把 `FocusBaseFill` / `FocusColor` / `FocusIntensity` 三个都做成参数 |
| 2 | **overlay 材质在 Nanite 网格上不渲染**：引擎里 `bOverlayMaterial` 的消费者只有普通静态网格 / 骨骼网格 / 实例网格三种代理，**Nanite 渲染路径里 0 处处理**；而 37 个 enabled 可交互物里 **34 个是 Nanite**（门 24 个 + 传送器 10 个）| 把可交互物用到的两个网格关掉 Nanite |

### 6.1 关掉 Nanite 的网格（本项目现状）

| 网格 | 用它的可交互物 | 规模 | Nanite |
|---|---|---|---|
| `/Game/Fab/Modern_Door/Door` | 24 个门（men1…men25）| **256 三角形 / 1 级 LOD** | 已关 ✓ |
| `/Game/models/tripo_convert_a27cf087-…` | 10 个传送器 | **1766 三角形 / 1 级 LOD** | 已关 ✓ |
| `/Game/Fab/Wall_light_2/wall_light_2` | 3 个机关灯 | 小网格 | 本来就没开 ✓ |

两者都是小网格、只有 1 级 LOD，Nanite 对它们本来就没有收益，关掉不影响性能（反而省了 Nanite 的固定开销）。

### 6.2 如果以后还想保留 Nanite

那 B 方案覆盖不了这些物体（引擎限制），要改用 **A（自定义深度 stencil + 后处理描边）**：
`r.CustomDepth=3` 已配、C++ 的 stencil 写入路径也还在（聚焦写 2 / 失焦还原），**只差一个后处理材质**（配方见本文 2.1）。
自定义深度对 Nanite 是支持的，所以 A 能覆盖所有网格。

### 6.3 PIE 里的三步自查

1. 看日志有没有 `[Interaction] 聚焦高亮：<物体>（overlay 图元 N 个，机制=Overlay Material）`：
   - **没有这行** ⇒ 注意力/射线没聚焦到物体（靠近 + 对准）；
   - `N=0` 或 `机制=Stencil` ⇒ overlay 没盖上（检查 `bUseOverlayHighlight` 与 `FocusOverlayMaterial`）；
2. `N>0` 但肉眼看不见 ⇒ 材质太淡：把 `FocusBaseFill`（默认 0.45）或 `FocusIntensity`（默认 2.5）调大；
3. 只有部分物体亮 ⇒ 那些物体的网格是不是又开了 Nanite（查看 `nanite_settings.enabled`）。


## 5. 方案 B 已落地：实现说明（2026-10）

### 5.1 结论：**不需要给任何物体逐个接线**

“被选中”的表现做在**玩家侧的探测器**上：谁被聚焦，就给谁盖 overlay。所以只要 Pawn 上有探测器（角色蓝图本来就有），
**场上所有可交互物**（67 个可交互网格都算）聚焦时都会亮 —— 新增物体也不用再配。

### 5.2 代码（`UInteractionDetectorComponent`）

| 项 | 说明 |
|---|---|
| `bUseOverlayHighlight` | 新增，**默认开** —— 选中效果的开关 |
| `FocusOverlayMaterial` | 新增；**类默认值**已在构造函数里用 `ConstructorHelpers::FObjectFinder` 指向 `M_InteractFocusOverlay`，所以游戏里生成的 Pawn 自带（不依赖蓝图/关卡实例上再指一次）|
| 聚焦时 | 对目标所有**可见网格组件**（`UMeshComponent`）`SetOverlayMaterial(FocusOverlayMaterial)`，并记录它原本的 overlay |
| 失焦时 | 只还原**我们盖过**的那些（`bOverlayApplied`）—— 不会误清美术/其它系统设的 overlay；stencil 一律按备份还原（运行中关开关也不留残留）|
| 与 stencil 的关系 | **二选一**：盖上 overlay 的图元不再写 custom depth / stencil（省一次 pass）；非网格图元（可见碰撞盒之类）仍走 stencil 那条路 |
| 日志 | 每次聚焦变化打一行：`[Interaction] 聚焦高亮：<物体>（overlay 图元 N 个，机制=Overlay Material）`，PIE 里一眼验证；Shipping 自动没有 |

### 5.3 材质 `/Game/MHY_ARCH_GAME/UI/M_InteractFocusOverlay`

- Domain = Surface，Blend Mode = Translucent，Shading Model = Unlit；
- 图：`Fresnel(Exponent=4, BaseReflect=0.05)` × `FocusColor(默认 0.25,0.75,1)` × `FocusIntensity(默认 3)` → **Emissive**；`Fresnel` → **Opacity**（只有边缘发亮、中间透明，就是“边缘光/选中光”）；
- 可调参数：`FocusColor`、`FocusIntensity`；想更“描边感”把 `Exponent` 调大；想要整块染色就把 Opacity 换成常数（例如 0.35）；
- 已勾的 Usage：Skeletal Mesh / Instanced Static Meshes / **Nanite** —— 本项目的 `menkuang*`、门的网格开了 Nanite，材质必须带 `Used with Nanite` 才会在 Nanite 几何上渲染。

### 5.4 验收（PIE）

1. 对着**门**（men1/men2/…）、**机关灯**（jiguandeng1-3）、**传送器**（kongjianchuansuoqi*）：物体边缘出现蓝色光；
2. 视线移开：光立刻消失、无残留；快速扫视连续切换也不闪；
3. 门这种“门板 + 门框”多图元物体应**整组一起亮**；
4. 隐藏的交互代理/探测器**不该亮**（`bOutlineVisiblePrimitivesOnly` 开着时）；
5. 看日志确认机制走的是 Overlay（`机制=Overlay Material`）；
6. 若某个 **Nanite** 物体不出效果：先确认材质的 `Used with Nanite` 已勾（已勾），再临时关掉该网格的 Nanite 对比验证。

### 5.5 后续可选

- 想要真·外描边（轮廓外一圈线）→ 按第 2.1 节做 `M_PP_InteractionOutline` 后处理材质（`r.CustomDepth=3` 已配好，C++ 的 stencil 路径还在，随时可用）；
- 想要呼吸/闪烁 → 在材质里给 `FocusIntensity` 乘一个 `Sine(Time)`，或聚焦时用 Dynamic Material Instance 推参数；
- 想要“待机细描边 + 聚焦粗描边”两档 → 见 2.1 步骤 5。



