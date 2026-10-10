# LiquidLight —— 选定的光呈现液体流动效果（实现说明）

关联代码：`Source/MHY_ARCH_GAME/LiquidLight/LiquidLightSurface.{h,cpp}`
代码宿主类：`ALiquidLightSurface : public AActor`（方案①，最小类树，已审核通过）

本方案优先使用 **UE5 引擎自带能力**，主体为**材质自流动**（无 Niagara、无第三方插件）；
Niagara 仅给出“增强方向”，不在本地落地。

---

## 1. 总体思路与层次

```
                    ALiquidLightSurface (宿主 Actor)
   ┌──────────────────────────┼──────────────────────────┐
 FluidSurface           OptionalLight              公开接口(蓝图可调)
 (UStaticMeshComponent)  (UPointLightComponent)    SetFlowActive / SetFlowStrength / IsFlowActive
        │                        │
        └── 材质 M_LiquidGlowFlow ┘ (参数: FlowStrength / Opacity)
                (自流动: 双层法线平移 + FlowMap 扰动 + Emissive + Fresnel)
```

- **“流动”来自材质**：通过 UV 平移 + 法线扰动让表面像液体一样缓缓流动。
- **“发光/被点亮的光”来自材质 Emissive**，并用宿主上可选 `OptionalLight` 一起点亮。
- 宿主只负责**开关 + 强度驱动**，把 `FlowStrength`、`Opacity` 写入材质动态实例。

---

## 2. 需要你在编辑器内手动创建的资产

> `.uasset` 是二进制，无法用代码生成，以下为手动步骤。

### 2.1 材质 `M_LiquidGlowFlow`（Content/LiquidLight/Materials/）
BlendMode = **Translucent**，LightingModel 默认，双面建议开启。
节点要点（关键“流动感”）：
1. `TextureCoordinate` 做两套，各自 `Multiply`（UV 平铺，建议 2~4）后做**平移**：
   - `Panner`(Speed=(0.05,0.02)) → 作为细节法线扰动 UV；
   - `Panner`(Speed=(-0.03,0.06)) → 作为第二层扰动（方向错开才像液体而非单向平移）。
2. 法线：两个扰动 UV 各采样一张法线贴图，`BlendAngleCorrectedNormals` 混合后进 `Normal` 输出。
3. FlowMap（可选增强）：采样一张 FlowMap 纹理（Flowmap Pack）驱动上面的 panner 速度/方向，形成“由光点向外铺开”的流向。
4. 发光：`BaseColor`（如偏青/蓝的发光色）+ `EmissiveColor = BaseColor * EmissiveIntensity`。
5. 半透明：`Opacity` 输入到 **Material Attribute/Opacity**，连到标量参数 `Opacity`。
6. 边缘高亮：`Fresnel`(Power 2~3) 乘 Emissive，让边缘更像液体反光。
7. 材质标量参数（均创建后可在 BP/代码里 SetScalarParameterValue）：
   - `FlowStrength`（默认 1）：乘到 panner 速度、EmissiveIntensity，控制“流得有多快/多亮”。
   - `Opacity`（默认 1）：控制透明度。
   - 可选 `BaseColor`、`EmissiveIntensity`。

纹理建议（均可先用引擎内置 `T_WaterNormal_A` 一类，或自己导入法线/FlowMap 贴图）。

### 2.2 面片网格
给 `FluidSurface` 设一个 StaticMesh：细分平面 `SM_Plane`（Content/LevelPrototyping/Meshes/SM_Plane）即可；
如需有厚度可用 Box 或自建低模平面。把上面材质设到该 Mesh 的材质 0。

---

## 3. 使用/验证
1. 编辑器里右键 `ALiquidLightSurface` → Create Blueprint class（如 `BP_LiquidLightSurface`）。
2. 在 BP 里把 `FluidSurface` 的 StaticMesh 指定为平面，Material 0 设为 `M_LiquidGlowFlow`。
3. 把 BP 拖进关卡；可勾 `bDriveLightWithFlow` 让 `OptionalLight` 一并点亮。
4. 想测触发：运行时调用 `SetFlowActive(true)` / `SetFlowActive(false)`（蓝图 BeginPlay、触发器、键盘事件均可），
   观察表面是否由“平静/隐藏”过渡到“发光流动”。
5. 后续如需接入 LightReveal 的“受光变可通行”：让该类实现 `IRevealableInterface` 并把 `RevealOn→SetFlowActive(true)`，
   `RevealOff→SetFlowActive(false)` 即可，公开 API 无需改动。

---

## 4. 增强方向（Niagara，仅“修改方向”，不落地）

若想要“真正的粒子水流/由光点向区域浇灌/随高度上升填充”，走 Niagara：
1. 在 `.uproject` 勾选启用 **Niagara** 插件（引擎自带）。
2. 新建 Niagara System：
   - Emitter（Sprite/GPU）喷出粒子，Position 由“被选中的光点”给出，沿表面法线向四周扩散；
   - 或一个覆盖区域的 Emitter，用 `Particle Attribute Reader` 模拟水位随时间上升填充，表现“点亮后像水填满”。
3. 接入宿主：给 `ALiquidLightSurface` 增加可选 `UNiagaraComponent`，在 `RefreshSurface()` 里
   `SetNiagaraVariableFloat` 把 `CurrentFlow` 同步给 Niagara 用户参数（如 `FlowStrength`、`FillAmount`）。
4. 开销提示：Niagara 粒子在移动端/大量实例需控制发射率与分辨率；材质自流动仍是最省方案，
   Niagara 建议只作“被选中的光点”上的点缀，而非整面水体。

---

## 5. 工程文件清单（本次新增/修改）
- 新增 `Source/MHY_ARCH_GAME/LiquidLight/LiquidLightSurface.h` / `.cpp`
- 修改 `Source/MHY_ARCH_GAME/MHY_ARCH_GAME.Build.cs`：PublicIncludePaths 增加 `MHY_ARCH_GAME/LiquidLight`
- 需手动创建：`M_LiquidGlowFlow` 材质、BP `BP_LiquidLightSurface`、关卡摆放。
