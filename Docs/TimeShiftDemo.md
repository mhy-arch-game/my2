# TimeShift Demo —— 古今时空切换的可运行用例

本文给出**从零搭一个可运行 demo** 的完整步骤：一张地图里并排摆"古/今"两套庭院，
按键把玩家（以及随行物品）在两套布局之间切换。

前置阅读：`Docs/TimeShift.md`（模块设计、计时锁、锚点与预留接口）。

---

## 一、Demo 目标与结构

```
一张地图（不同世界坐标处两套布局）

  ┌── 古庭院 (X ≈ 0) ──┐            ┌── 今庭院 (X ≈ 8000) ──┐
  │  石制建筑若干        │            │  金属建筑若干          │
  │  ▲ Anchor (LayoutOrigin) │        │  ▲ Anchor (LayoutOrigin) │
  │      Era = Ancient        │        │      Era = Modern         │
  └───────────────────────────┘        └───────────────────────────┘

按键 → 内容显隐互换 + 玩家按"布局参考"换算到另一庭院的对应位置
```

验证点：① 内容按时代显隐；② 玩家被移动到对应位置；③ 计时锁生效；④ 随行物品一起穿越。

---

## 二、准备（编译）

新增了 C++ 类，需先构建工程：

1. 打开 `MHY_ARCH_GAME.uproject` → 提示重建时选 **Yes / Rebuild now**；
2. 内容浏览器勾选 **Show C++ Classes**，`C++ Classes/MHY_ARCH_GAME/` 下应能看到：
   - `TimeShiftAnchor`、`TimeShiftTravelComponent`、`TimeEraComponent`、`TimeShiftInputComponent`、`TimeShiftSubsystem`。
3. 若重建报错，把日志里的 `error C` 行拿出来排查。

---

## 三、步骤 1：关卡

推荐直接复制一个已有可玩关卡，省去光照与 GameMode 配置：

1. 内容浏览器找到 `Content/ThirdPerson/Lvl_ThirdPerson`（或 `Variant_SideScrolling/Lvl_SideScrolling`）；
2. `Ctrl+C / Ctrl+V` 复制一份，重命名为 `Lvl_TimeShiftDemo`；
3. 双击打开，`Ctrl+S` 保存。

> 也可以自己 `New Level → Basic`，但要自行补 `Player Start`、`Directional Light`、地板与 GameMode。

---

## 四、步骤 2：做两套"庭院"蓝图

把一整组建筑做成一个 Actor 蓝图，只需挂一个时代组件，比逐个摆放省事得多。

### 4.1 `BP_AncientCourtyard`（古庭院）
1. 内容浏览器右键 → `Blueprint Class` → 父类 `Actor` → 命名 `BP_AncientCourtyard`；
2. 打开蓝图，`Add → Static Mesh`，命名 `Building`：
   - `Static Mesh` 选 `Content/LevelPrototyping/Meshes/SM_Cube`；
   - 设 `Scale` 成墙体/柱子形状（如 `(1, 3, 4)`），位置 `(0,0,200)`；
3. 想更像"建筑"，可再加几个 Static Mesh 组件拼成门楼/围墙（同样用 `SM_Cube` / `SM_Plane`）；
4. 设材质：给各 Mesh 的 `Element 0` 指定一个偏**石/土色**的材质（如 `MI_DefaultColorway`，或自建）；
5. **Add Component → 搜 `Time Era`**，选中它：
   - `Era = Ancient`
   - `bGate Visibility` / `bGate Collision` 保持勾选
6. `Compile` → `Save`。

### 4.2 `BP_ModernCourtyard`（今庭院）
同法复制一份（右键 `BP_AncientCourtyard` → `Duplicate`），改名 `BP_ModernCourtyard`：
- 把材质换成偏**金属/冷色**；
- 把 `Time Era` 组件的 `Era` 改成 **`Modern`**；
- 也可把部分方块位置/尺寸改一改，让两个时代看得出差别；
- `Compile` → `Save`。

### 4.3 摆进关卡
1. 把 `BP_AncientCourtyard` 拖进关卡，放在 `(0, 0, 0)`；
2. 把 `BP_ModernCourtyard` 拖进关卡，放在 `(8000, 0, 0)`（**相距足够远，切换才看得明显**）；
3. 在古庭院旁放一个 `Player Start`（或复用关卡原有的），让玩家开局在古庭院一侧。

> 此时两套庭院都"存在"于关卡中，但开局只有 `Ancient` 的那套会显示（见步骤 5 的时代初值）。

---

## 五、步骤 3：摆"布局参考"锚点（决定对应位置怎么算）

由于两套庭院**一一对应**，只需**每个时代各放 1 个布局参考锚点**（不再需要逐区域摆锚点对）。

1. `Place Actors` 面板搜 **`Time Shift Anchor`**，拖入关卡；
2. 放在**古庭院**的基准点（例如庭院中心、主门地面），Details 设：
   - `Era = Ancient`
   - 勾选 **`bDefines Layout Origin`**
   - `Anchor Id` **留空**（它只作布局参考，不参与锚点配对）
3. 再拖一个到**今庭院**的**同一语义位置**（同样是中心/主门地面，相对各自庭院的偏移保持一致），设：
   - `Era = Modern`
   - 勾选 **`bDefines Layout Origin`**
   - `Anchor Id` 同样留空
4. 完成。之后玩家在古庭院任何位置切换，都会被换算到今庭院的**对应位置**：
   `新位置 = 今参考 * 古参考⁻¹ * 旧位置`。

> 两个参考锚点之间的**位置差即两套布局的偏移**。若两套布局还相对旋转过（例如今庭院整体转了 90°），
> 勾选完整变换即可自动带过去，无需额外配置。
> 若只在古侧勾了布局参考、今侧没勾：切换到"今"时会退回锚点对模式，再取不到数据则**玩家留在原地**（预期降级），内容仍会切换。

---

## 六、步骤 4：配置玩家

打开本关卡使用的角色蓝图（如 `BP_ThirdPersonCharacter`）。

> 建议先 `Duplicate` 出 `BP_DemoCharacter` 专用，避免影响其它关卡的玩法；本项目为演示工程，直接改也可以——但要记得**本关卡与其它关卡若共用同一角色，改动会同时生效**。

1. **Add Component → `Time Shift Input`**
   - `Switch Action`：指向一个输入动作。
     - 想快速验证：直接指 `Content/Variant_SideScrolling/Input/Actions/IA_Interact`（按 E 触发）；
     - 想干净：右键 `Input → Input Action` 新建 `IA_TimeShift`，再到所用 `IMC_*` 里加一个按键（如 `Tab`）映射。
2. **Add Component → `Time Shift Travel`**
   - `bTeleport Owner` 保持勾选；
   - 其余保持默认（`bAutoCollect ByTag` 开启，标签 `TimeTraveler`）。
3. `Compile` → `Save`。

---

## 七、步骤 5：可选配置

编辑 `Config/DefaultGame.ini`（文件里已有注释模板）：

```ini
[/Script/MHY_ARCH_GAME.TimeShiftSubsystem]
InitialEra=Ancient      ; 开局所在时代；改 Modern 则开局显示今庭院
CooldownDuration=2.0    ; 切换后的计时锁（秒）；0 = 关闭
```

- **关键**：`InitialEra` 必须与"玩家出生点所在的那套庭院"一致，否则开局会看到另一套建筑而人站在空地上。

---

## 八、步骤 6：运行验证

点 **Play**，逐项确认：

| 验证点 | 预期现象 |
|---|---|
| 开局 | 只显示 `InitialEra` 对应的那套庭院；另一套不可见且无碰撞 |
| 按切换键 | 建筑内容互换（古隐藏、今显示），**玩家瞬移到今庭院对应位置** |
| 连按 | 2 秒内无效（计时锁）；可用 `GetCooldownRatio()` 接 UI 进度条观察 |
| 再按一次 | 切回古庭院，位置回到古侧对应点 |
| 站位偏移 | 切换前站在参考点偏左 2 米，切换后仍偏左 2 米（刚体映射，不会吸附到参考点） |

**调试技巧**：
- 视口切到 `View Mode → Collision`，或控制台 `show Collision`，可确认隐藏时代的内容确实关掉了碰撞；
- 用 `Get Game Instance → Get Subsystem (Time Shift) → Get Era` 在关卡蓝图里打印当前时代。

---

## 九、进阶：随行物品一起穿越

1. 做一个箱子：`Blueprint Class (Actor)` → 命名 `BP_TimeCrate`：
   - `Add → Static Mesh`，选 `SM_Cube`，设 `Scale` 约 `0.5`，`Collision Preset = BlockAll`；
   - 勾 `Simulate Physics` 可选（纯装饰也可）；
   - `Compile` → `Save`。
2. 把 `BP_TimeCrate` 拖进关卡，放在**古庭院参考点附近**；
3. 选中该箱子实例 → Details → 顶部 **Actor → Tags** → 添加一个标签 **`TimeTraveler`**；
4. 再 Play，按切换键 → 箱子与玩家**一起**移动到今庭院对应位置。

> 原理：`UTimeShiftTravelComponent` 在位移前记录玩家脚下位置，然后把**该位置附近 800 单位内、带 `TimeTraveler` 标签的 Actor**一起位移；已附着在玩家身上的物品会自动跟随并被跳过，避免位移叠加两次。
> 想改半径或标签名：改玩家 `Time Shift Travel` 组件的 `Auto Collect Radius` / `Carried Actor Tag`。

### 为什么不用手动指定引用？
因为玩家是运行时从 `Player Start` 生成的，角色蓝图的默认值里**无法引用关卡中摆放的箱子**。标签 + 半径是这种情况下最实用的做法。若你有运行时动态持有的物品（如抓取系统），也可以直接往组件的 `Carried Actors` 数组里 `Add`。

---

## 十、进阶：多区域 / 布局不统一

**默认（`Layout Origin` 模式）无需任何额外配置**：因为映射是全局的，玩家在古庭院、走廊、街道任何位置切换，都会被换算到今布局的对应位置——前提是两套布局保持**一一对应**。

如果后续两套布局**不再统一**（例如今时代某区域被改造、地形不同），有两种做法：

1. **把该区域单独处理**：给玩家组件保留 `Anchor Pair` 回退即可——在统一区域摆布局参考、在不统一区域成对摆锚点；
   组件默认先试 `Layout Origin`，取不到布局参考时才走锚点。
2. **整体切到 `Anchor Pair` 模式**：把 `Mapping Mode` 改为 `Anchor Pair`，为每个可切换区域各放一对同名锚点：

```
区域 A: Anchor "Courtyard"  (Ancient / Modern)
区域 B: Anchor "Street"     (Ancient / Modern)
```

`AnchorPair` 模式下组件会自动选**离玩家最近的旧时代锚点**，并跳到**同名**的新时代锚点。

---

## 十一、常见问题

| 现象 | 原因 / 处理 |
|---|---|
| 组件列表里搜不到 `Time Era` / `Time Shift Travel` | C++ 未编译成功 → 重新构建工程 |
| 按切换键没反应 | ① 角色没挂 `Time Shift Input`；② `Switch Action` 为空；③ 该 Input Action 没在当前 `IMC` 里绑定按键；④ 正处于计时锁内 |
| 内容切换了但角色没动 | 两个时代都没有勾 `bDefines Layout Origin` 的锚点（且无可用锚点对）→ 补齐布局参考锚点 |
| 角色被传送到空中/墙体里 | 两个布局参考锚点不在"同一语义位置" → 核对相对偏移是否与两套布局一致 |
| 切换后角色朝向也变了 | 两套布局之间存在旋转（参考锚点旋转不同）→ 属正常映射；不想转就统一参考锚点的旋转 |
| 两个庭院都可见 | 建筑没挂 `Time Era` 组件，或 `Era` 设错 |
| 开局就站在空地上 | `InitialEra` 与出生点所在庭院不一致 |
| 箱子没跟着走 | 箱子没打 `TimeTraveler` 标签，或距离超过 `Auto Collect Radius` |
| 角色瞬移后卡住 | 目标位置附近有实体碰撞（新建筑）→ 核对两套布局的对应关系与碰撞体 |

---

## 十二、相关文档
| 文档 | 内容 |
|---|---|
| `Docs/TimeShift.md` | 模块设计：类清单、切换流程、位置映射模式（Layout Origin / Anchor Pair）、计时锁、★预留的跨时空接口 |
| `Docs/InteractionLogic.md` | 交互模块的人机活动逻辑与解耦设计原理 |
| `Docs/StructureInteraction.md` | 可交互建筑结构框架 |
| `Docs/OverlapDemo.md` | 重叠即通行 + 变材质/透明的操作与 demo |
