# OverlapPassage —— 重叠即通行 & 重叠变材质/透明 Demo 搭建指南

关联代码：
- `Source/MHY_ARCH_GAME/OverlapPassage/OverlapPassageComponent.h`
- `Source/MHY_ARCH_GAME/OverlapPassage/OverlapPassageComponent.cpp`

组件类：`UOverlapPassageComponent`（挂载式 `UActorComponent`，方案 B：细分代理碰撞体近似）

---

## 一、功能目标

搭两个**可移动方块**，路径交叉、会动态重叠：

- **重叠时，交集区域变成可通行**（角色能从重叠处穿过，非重叠处仍阻挡）；
- **重叠时，方块切换成另一材质 / 变透明**；分离后自动恢复。

> 前提：先让工程编译通过，`OverlapPassageComponent` 才会出现在组件的 Add 列表里。

---

## 二、UE 编辑器基本操作速览

| 操作 | 方式 |
|---|---|
| 视口导航 | 右键拖拽 = 旋转视角；按住右键 + `W/A/S/D` = 飞行；中键拖拽 = 平移；`F` = 聚焦选中物体；`G` = 切换游戏视图 |
| 复制物体 | 选中后 `Alt` + 拖动，或 `Ctrl+C` / `Ctrl+V` |
| 新建资源 | 内容浏览器空白处右键 → `Blueprint Class` / `Material` / `New Folder` |
| 从 C++ 类建蓝图 | 内容浏览器勾选 `Show C++ Classes` → 找到类 → 右键 → `Create Blueprint class based on ...` |
| 添加组件 | 打开蓝图 → 左上角 **Add** → 搜组件名（如 `OverlapPassage`、`Static Mesh`） |
| 修改属性 | 选中组件/节点 → 右侧 **Details** 面板 |
| 编译 / 保存 | 蓝图工具栏 **Compile** → **Save** |
| 运行测试 | 主编辑器工具栏 **Play**（`Alt+P`），`Esc` 停止 |
| 控制台 | 播放时按 `~` 输入命令，如 `show Collision` 显示碰撞 |

---

## 三、具体搭建步骤

### 步骤 1：建一个"重叠用"半透明材质（可选，用于外观切换）

1. 内容浏览器新建文件夹，如 `Content/OverlapDemo/`。
2. 右键 → `Material`，命名 `M_OverlapHighlight`，双击打开。
3. Details 里 `Blend Mode = Translucent`，`Shading Model = Unlit`（或 Default Lit）。
4. 加 `Constant3Vector`（醒目色，如青色）连到 **Emissive Color**。
5. 加 `ScalarParameter` 命名 **`Opacity`**（值 0.4），连到 **Opacity** 输出。
6. 保存。→ 该材质既可用于"换材质"模式，也可用于"透明"模式。

### 步骤 2：建移动方块蓝图 `BP_MovingBlock`

1. 内容浏览器右键 → `Blueprint Class` → 父类选 **Actor**，命名 `BP_MovingBlock`。
2. 打开后根组件默认为 `DefaultSceneRoot`。**Add → Static Mesh**，命名 `Mesh`：
   - Details 里 `Static Mesh` 选 `Content/LevelPrototyping/Meshes/SM_Cube`；
   - 将其设为根（拖到 `DefaultSceneRoot` 上，或删除 `DefaultSceneRoot`）；
   - 建议缩放成一块板/墙（如 `Scale=(1, 3, 3)`），并设一个基础材质。
3. **Add → 搜索 `Overlap Passage`**，添加 `OverlapPassage` 组件：
   - `Divisions` 先默认 `4,4,4`；想更精细可调大（如 `8,8,8`）；
   - 先不管 `OtherActor`（稍后在关卡实例里指定）。
4. **让方块自己动（用 Timeline，纯蓝图无需代码）**：
   - 图表空白处右键 → `Add Timeline`，命名 `MoveTimeline`，双击进入；
   - `Length` 设 2.0；加一条 Float Track，两个关键帧：时间 0 值 0、时间 2 值 1；
   - 勾上 Timeline 的 **Loop** 与 **Auto Reverse**（来回往复）；
   - 回到事件图表：`Event BeginPlay` → `MoveTimeline` 的 **Play from Start**；
   - `MoveTimeline` 的 **Update** 输出 → `Lerp (A=起点, B=终点, Alpha=输出)`；
   - `Lerp` 结果 → **Set Actor Location** 节点，`Sweep` **不勾选**（重要：不勾才能彼此穿插重叠）；
   - `Compile` → `Save`。
5. 关卡里放两份实例即可（推荐同一蓝图放两个实例，省事），或复制蓝图命名 `BP_MovingBlock_B`。

### 步骤 3：放进关卡并交叉摆放

1. 打开一个可玩关卡（推荐 `Content/Variant_SideScrolling/Lvl_SideScrolling`，或 `Content/ThirdPerson/Lvl_ThirdPerson`）。
2. 把 `BP_MovingBlock` 拖进关卡两次：
   - 实例 A：放在某位置；
   - 实例 B：放在**路径与 A 交叉**的位置（如 A 沿 Y 移动，B 置于 A 路径中间）。
   - 用 `W` 调整位置；`F` 聚焦确认。
3. 按需把玩家出生点/路径安排在能让角色走进**重叠区**的位置。

### 步骤 4：互相指定 `OtherActor`（关键）

1. 关卡中选中**实例 A**，Details 展开 `OverlapPassage` 组件：
   - 找到 **`Other Actor`**，点下拉/吸管，选中 **实例 B**。
2. 选中实例 B，把它的 `OverlapPassage → Other Actor` 指向 **实例 A**。
   - 两边都设 → 交集对双方都挖空，角色从交集穿过；
   - 只设一边 → 只有那一边可穿。

### 步骤 5：配置"重叠变材质/透明"

在每个实例的 `OverlapPassage` 组件里，展开 **`OverlapPassage|Appearance`**：

- 勾 **`bChangeAppearanceOnOverlap`**；
- 方式 A（换材质）：把 **`Overlap Material`** 设成步骤 1 的 `M_OverlapHighlight`；
- 方式 B（变透明）：勾 **`bDriveOpacityParameter`**，`Overlap Opacity` 设 `0.35`，
  `Opacity Parameter Name` 保持 `Opacity`（基础材质须为 Translucent 且有该参数）；
- `Target Meshes` 留空 = 作用于该物体所有 Mesh 组件。

> 想让两个方块都变化，就在两个实例上都配一遍。

### 步骤 6：运行验证

1. 点 **Play**。
2. 观察：
   - 两方块移动到重叠 → 外观切换/变透明；
   - 角色走进**重叠处** → 能穿过；走到**非重叠处** → 被挡；
   - 方块分离后 → 外观恢复、通行区消失。
3. 想直观看到"挖空的格子"：播放时按 `~` 输入 **`show Collision`**，以线框显示碰撞体，
   重叠区的代理盒碰撞会消失/变色。

---

## 四、参数速查（`OverlapPassage` 组件）

| 属性 | 说明 | 默认 |
|---|---|---|
| `Other Actor` | 对面物体（决定交集） | 空 |
| `Divisions` | 代理网格细分（越大越精细、代理盒越多） | `4,4,4` |
| `Update Interval` | 刷新限频（秒） | `0.05` |
| `bDisableHostCollision` | 接管宿主原碰撞 | `true` |
| `bOnlyWhenAABBOverlap` | 不相交时跳过计算 | `true` |
| `Probe Object Types` | 探测量子时考虑的对象类型 | WorldStatic/WorldDynamic/Pawn/PhysicsBody |
| `bChangeAppearanceOnOverlap` | 重叠时是否改变外观 | `false` |
| `OverlapMaterial` | 重叠时套用的材质 | 空 |
| `bDriveOpacityParameter` | 改用动态实例驱动透明度 | `false` |
| `OverlapOpacity` | 透明模式透明度值 | `0.35` |
| `OpacityParameterName` | 透明参数名 | `Opacity` |
| `TargetMeshes` | 指定要变的网格（空=宿主全部网格） | 空 |

蓝图接口：`RefreshPassage()`、`RebuildProxyGrid()`、`IsOverlapping()`。

---

## 五、调试与常见问题

| 现象 | 原因 / 处理 |
|---|---|
| Add 组件里找不到 `Overlap Passage` | C++ 未编译成功 → 先构建工程 |
| 完全不生效 | 关卡实例里 `Other Actor` 没设，或没指对对方 |
| 两个方块撞住、无法重叠 | 移动方式带 Sweep（如 InterpToMovement 组件）→ 改用蓝图 `Set Actor Location` 且 **Sweep 不勾** |
| 角色在任何地方都能穿 | `Divisions` 太小或两边都挖空；检查 `Other Actor` 是否指反；用 `show Collision` 看格子 |
| 通行区呈阶梯状 | 方案 B 的量化特性 → 调大 `Divisions`（8 或 16），代理盒会更多 |
| 外观不变化 | 只勾了 `bChangeAppearanceOnOverlap` 但未给 `Overlap Material` 也未勾透明模式；或材质不是 Translucent / 无 `Opacity` 参数 |
| 外观未还原 | 检查是否手动改过材质；组件只还原它记录过的原材质 |

---

## 六、已知限制（方案 B 固有）

- 通行区为**网格量化近似**，取决于 `Divisions`。
- 单元探测使用**轴对齐盒**，宿主旋转时略有误差。
- 会**接管宿主根组件原碰撞**（`bDisableHostCollision` 可关闭，但原网格可能仍挡路）。
- **外观按整个网格**切换，不是只给被挖空的格子换材质。
- 仅处理根 Primitive；多碰撞组件的复杂 Actor 需自行扩展。
