# Climb —— 指定位置触发攀爬并抵达同 X/Y 的平台最高点

关联代码目录：`Source/MHY_ARCH_GAME/Climb/`

**方案**：Root Motion + **Motion Warping**（引擎自带插件），进入攀爬点范围自动触发，平台顶面**自动探测**。

---

## 一、机制

```
角色走进 Climb Spot 的触发范围
   → AClimbSpot 找到角色身上的 UClimbComponent → TryClimb(this)
   → 校验：未在攀爬中 / 站在地面 / 蒙太奇存在 / 上方探测到平台
   → 对齐角色到攀爬点（可选）
   → 求终点：从 (X, Y, 起点Z + TraceUpHeight) 向下打一条射线
             命中平台顶面 Z → 终点 = (起点X, 起点Y, 顶面Z + 胶囊半高)
   → 锁定移动（切 MOVE_Flying + 停速）、锁定朝向
   → 播放攀爬蒙太奇，并把终点写入 MotionWarping 的 Warp Target
   → 蒙太奇结束：精确吸附到终点 → 恢复移动/朝向 → 广播 OnClimbFinished
```

**终点保证**：X/Y 与**对齐后**的角色位置严格相同（代码只改 Z），Z 为平台顶面 + 胶囊半高 → 角色脚底正好落在平台最高点。

**拒绝条件**（任一满足则不触发、不播放动画）：
- 上方射线**没打中**平台；
- 落差 ≤ 0（不是往上爬）；
- 落差超过攀爬点的 `Max Climb Height`；
- 角色不在 `MOVE_Walking`（例如正在空中/坠落）。

---

## 二、类清单

| 类 | 基类 | 职责 |
|---|---|---|
| `EClimbMoveMode` | `UENUM` | `MotionWarping`（主）/ `CurveDriven`（无 MotionWarping 组件时自动回退） |
| `AClimbSpot` | `AActor` | **指定攀爬位置**：触发盒 + 对齐偏移/朝向 + `Max Climb Height` + 可覆盖蒙太奇 + `bAutoTrigger OnOverlap` + `bSingle Use` |
| `UClimbComponent` | `UActorComponent` | 挂在任意 `ACharacter` 上：`TryClimb / CancelClimb / IsClimbing`、目标探测、锁移动、播蒙太奇、驱动位移、收尾吸附、`OnClimbStarted / OnClimbFinished` |

依赖：`.uproject` 已启用 **MotionWarping** 插件；`Build.cs` 已加入 `MotionWarping` 模块与 `MHY_ARCH_GAME/Climb` 包含路径。

---

## 三、引擎内搭建步骤

### 3.1 准备攀爬蒙太奇（关键）
1. 选一段**带 Root Motion** 的攀爬动画，创建 `Anim Montage`；
2. 在蒙太奇时间轴上、覆盖"位移那段"的位置添加 **`AnimNotifyState_MotionWarping`**；
3. 打开该 Notify 的 Details：
   - **`Warp Target Name` 必须与组件上的 `Warp Target Name` 一致**（默认 `ClimbTarget`）；
   - `Root Motion Modifier` 保持默认的 `MotionWarping`（位移扭曲）即可；
   - 如需同时转向，可再加 `Rotation` 修饰器。
4. 保存。

> 若动画不带 Root Motion，也**不必卡住**：组件检测不到 `UMotionWarpingComponent` 时会自动改用 `CurveDriven` 垂直插值完成位移（会有脚滑风险，需自行调参或改用带 Root Motion 的动画）。

### 3.2 给角色加组件
打开角色蓝图（如 `BP_ThirdPersonCharacter`）：
1. **Add Component → `Motion Warping`**（引擎自带，若列表里没有，说明插件没启用）；
2. **Add Component → `Climb`**（`UClimbComponent`）：
   - `Climb Montage`：指向上一步的攀爬蒙太奇；
   - `Move Mode`：`Motion Warping`（默认）；
   - `Warp Target Name`：`ClimbTarget`（与 Notify 一致）；
   - `Trace Up Height`：400（探测起点高度，需大于平台高度）；
   - `Ledge Trace Channel`：`Visibility`（确保平台**会阻挡该通道**，否则探不到）；
   - `bSnap To Target On Finish`：勾选（保证 X/Y 严格一致）。
3. `Compile` → `Save`。

### 3.3 关卡里摆攀爬点
1. `Place Actors` 搜 **`Climb Spot`**，拖到墙根/台边；
2. 调整它的**位置与朝向**：**让 Actor 的箭头朝向墙面/平台**（朝向就是角色攀爬时的面向）；
3. 设 `Align Offset`：角色对齐点相对该 Actor 的偏移（一般稍微离墙一点，避免卡进墙里）；
4. 如果需要限制可攀爬高度，调 `Max Climb Height`（默认 300）；
5. 想一次性使用就勾 `bSingle Use`。

### 3.4 运行验证
| 验证点 | 预期 |
|---|---|
| 走进攀爬点范围 | 自动触发攀爬，播放蒙太奇并上升 |
| 结束时位置 | 落在平台顶面，**X/Y 与起始位置一致**（可在结束回调里打印坐标比对） |
| 平台过高 | 超过 `Max Climb Height` → 不触发 |
| 上方无平台 | 射线未命中 → 不触发、不播动画 |
| 空中经过 | 不在地面 → 不触发 |
| 触发后 | 移动/朝向被锁，结束后恢复（含原来的 `bUseControllerRotationYaw` 设置） |

调试：控制台 `show Collision` 查看攀爬点触发盒；临时把 `Ledge Trace Channel` 改成 `Camera` 之类专用通道，避免误命中装饰物。

---

## 四、参数速查

### `AClimbSpot`
| 属性 | 说明 | 默认 |
|---|---|---|
| `Align Offset` | 角色对齐点相对 Actor 的偏移 | 0 |
| `bAlign Character On Start` | 开始时把角色对齐到该点 | true |
| `bAlign Character Rotation` | 是否用 Actor 朝向覆盖角色朝向 | true |
| `Max Climb Height` | 允许的最大攀爬落差 | 300 |
| `Climb Montage Override` | 覆盖组件默认蒙太奇 | 空 |
| `bAuto Trigger On Overlap` | 进入范围自动触发 | true |
| `bSingle Use` | 只能爬一次 | false |

### `UClimbComponent`
| 属性 | 说明 | 默认 |
|---|---|---|
| `Climb Montage` | 默认攀爬蒙太奇 | 空 |
| `Move Mode` | `MotionWarping` / `CurveDriven` | MotionWarping |
| `Warp Target Name` | 与 Notify 里的名字一致 | `ClimbTarget` |
| `Trace Up Height` | 向下探测的起始高度 | 400 |
| `Ledge Trace Channel` | 探测通道 | Visibility |
| `Montage Play Rate` | 播放速率 | 1.0 |
| `bLock Rotation During Climb` | 攀爬时锁朝向 | true |
| `bSnap To Target On Finish` | 结束时精确吸附（保证 X/Y） | true |
| `Climb Height Curve` | CurveDriven 模式的垂直曲线（0..1） | 空（用 SmoothStep） |

蓝图接口：`TryClimb(Spot)`、`CancelClimb()`、`IsClimbing()`、`GetClimbTargetLocation()`、`OnClimbStarted`、`OnClimbFinished`。

---

## 五、边界与已知限制

| 情形 | 说明 / 处理 |
|---|---|
| 角色没挂 `MotionWarping` 组件 | 自动回退 `CurveDriven`（打 Warning），功能仍可用 |
| 蒙太奇上没有 MotionWarping Notify | 动画的 Root Motion 会照常播放，但落点不精确 → 靠结束时的**吸附**兜底 |
| 蒙太奇被中断（受击等） | 仍会完成收尾并吸附到目标，避免角色卡在墙中间 |
| 攀爬点对齐距离过大 | 会看到"瞬移对齐" → 把 Spot 摆在角色自然会站的位置 |
| 平台不阻挡 `Ledge Trace Channel` | 探测不到 → 改成平台会阻挡的通道（或开启其碰撞） |
| 攀爬中不响应输入 | 已 `StopMovement + MOVE_Flying`；如需可在 `OnClimbStarted` 里禁用输入映射 |
| 目标 X/Y | 由设计固定为起点 X/Y；如需"爬到平台内侧"，可后续增加 `bOffsetForward` 参数 |
| 导航 | 攀爬过程中 `NavMesh` 不感知，攀爬结束后角色已在新位置，AI 可能需要重寻路 |

---

## 六、后续可扩展项
1. `bOffsetForward`：落点在同 X/Y 基础上再向平台内侧推进一段（更自然的收尾）。
2. 攀爬中的输入拦截（禁用 `IA_*` 映射）与 IK 手部贴合。
3. 攀爬点自动生成（沿可攀爬边缘批量布点）。
4. `CancelClimb` 的中途放弃表现（当前会直接落回并掉下）。
