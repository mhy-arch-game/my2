# 门铰链：按 mesh 包围盒取左边缘

## 1. 结论

17 个 `active_door` 实例（label = `active_door`、`men2`…`men17`）已全部改为绕铰链旋转：

| 属性 | 写入值 |
|---|---|
| `bUseAxisRotation` | `true` |
| `RotationAxis` | `(0, 0, 1)` —— 竖直轴承 |
| `RotationPivot` | `(0, -21.8287, 0)` |
| `OpenAngleDegrees` | 90（沿用原 `OpenRelativeTransform` 的 90° yaw） |

脚本：`Scripts/apply_door_hinge.py`，结果：`Scripts/door_hinge_result.json`
（`doors=17 failed=0 level_saved=true`）。改动前备份：`Saved/Backup/firstvision_before_hinge.umap`。

## 2. 宽度轴是 Y，不是 X

既有代码注释里的示例写的是「宽 100 的门设 `(-50,0,0)` 铰在 -X 边」。但本项目的门
（`/Game/Fab/Modern_Door/Door`）实测包围盒是：

```
X [-2.4009,  2.2338]   厚度 4.63
Y [-21.8287, 21.8646]  宽度 43.69   <-- 门宽在 Y 上
Z [ 0.0932, 95.3314]   高度 95.24
```

所以脚本**从包围盒推断宽度轴**（门是薄板：厚度是最小的水平尺寸，宽度是较大的那个），
得到 `width_axis=y`，铰链取该轴的**最小值**边 → `(0, -21.8287, 0)`。
若照抄注释里的 -X，铰链会落在离原点 2.4 单位处，门看起来还是在原地打转。

## 3. 为什么 pivot 必须是 mesh 局部坐标

`UInteractableComponent::GetTargetTransform()` 返回
`ClosedRelativeTransform * (T(P) · R(axis,θ) · T(-P))`，再整体 `SetRelativeTransform` 到开关组件
（这里就是 `DoorMesh`）。要让铰链点 h 在旋转下不动，需要 `Rot·h = h`，即 **P 必须等于 h 的
组件局部坐标**——正好就是 `get_local_bounds()` 给出的空间。与组件自身的相对变换
`ClosedRelativeTransform` 是否为 identity 无关（等式两边同乘）。

## 4. 脚本参数

```python
APPLY        = True          # False = 只算不写
HINGE_EDGE   = "min"         # "min" = 左边缘（该轴最小值）| "max" = 右边缘
WIDTH_AXIS   = "auto"        # "auto" | "x" | "y"
ROTATION_AXIS = (0, 0, 1)    # 竖直轴承
```

用到的编辑器 Python API（已实测可用）：
- `scene_component.get_local_bounds()` → `(min, max)` 两个 `unreal.Vector`
- `static_mesh.get_bounding_box()` / `get_bounds()`（备用）
- `unreal.Vector` **不可下标**，必须先转成 `[x, y, z]` 列表再索引

## 5. 如果开合方向不对

两种等效改法，改一行重跑脚本即可：

- 铰链换到另一边：`HINGE_EDGE = "max"` → pivot 变 `(0, +21.8646, 0)`；
- 或保留铰链、反转开合角：把 `OpenAngleDegrees` 设为 `-90`。

「左」在只有包围盒时只能定义为「宽度轴的最小边」；面向门的观察方向在数据里无从判断，
所以两种都留成参数。

## 6. 真正的故障点：属性写进了内存，但没被保存（已修复）

第一版脚本用的是普通的 `comp.set_editor_property("RotationPivot", ...)`：当次会话里立刻读回来
是对的，但**保存关卡再重新加载后 `RotationPivot` 回到 `(0,0,0)`**（而 `bUseAxisRotation` 留下来了）。
于是门依旧是绕**自身原点**转，"轴/铰链变换"看起来根本没实现。

修复：写入时显式要求改动通知，关卡保存才会真正记录这次覆盖。

```python
comp.set_editor_property("RotationPivot", unreal.Vector(*pivot),
                         unreal.PropertyAccessChangeNotifyMode.ALWAYS)
```

**核验方式：保存 → 重新 `load_map` → 再读。** 同一次会话里读回不算数——这正是第一版漏掉的检查。

### 现状（重新加载关卡后实测）

| 项 | 值 |
|---|---|
| 门数 | 17 |
| `bUseAxisRotation` | 17/17 = `true` |
| `RotationAxis` | 17/17 = `(0,0,1)` |
| `RotationPivot` | 17/17 = `(0, -21.828739, 0)` |

同样的值也写进了**蓝图组件模板** `active_door_C:Interactable_GEN_VARIABLE`，
以后新拖进来的门自带铰链。
