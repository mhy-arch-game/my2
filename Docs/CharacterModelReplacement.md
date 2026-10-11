# 替换人物模型：兼容性分析与实施方案

> 需求：导入一个新的角色模型，替换掉现有角色模型。本文给出**当前的装配事实**、**四种可能情况的兼容性结论**、
> **风险清单**、**推荐路线与步骤**，以及**需要你提供的信息**（模型文件本身还没给，所以结论按类型分情况给）。

## 0. 现状：要替换的到底是什么

当前玩家角色是 `BP_FirstPersonCharacter`（C++ 基类 `AMHY_ARCH_GAMECharacter`，GameMode 用它当 DefaultPawn）。实测装配：

| 项 | 实测值 |
|---|---|
| 胶囊 | 半高 **96** / 半径 **34**，碰撞配置 `Pawn`（UE5 人体胶囊标准值）|
| **身体网格** | `SkeletalMeshComponent` = `SKM_Manny_Simple`，AnimBP = **`ABP_Unarmed`**，碰撞配置 `CharacterMesh` |
| **第一人称网格** | 第二个 `SkeletalMeshComponent` = `SKM_Manny_Simple`，AnimBP = **`ABP_FP_Copy`**，旁边挂 `CameraComponent` |
| 相机 | `CameraBoom` + `FollowCamera`（C++，挂在 boom 的 socket 上）+ 一个 `CameraComponent` |
| 其它组件 | InteractionDetector / InteractionPrompt / MotionWarping / Climb / MovementAudio / Sprint / TimeShiftTravel / TimeShiftInput |
| 骨骼相关资产 | `ABP_Unarmed`（3P）、`ABP_FP_Copy`（1P）、`CtrlRig_FPWarp`（Control Rig，配合 MotionWarping）|

**好消息**：C++ 里**没有硬编码任何骨骼名 / 插槽名**（全工程只有 `USpringArmComponent::SocketName` 这种组件名），
所以换模型**不会让代码编不过**；所有绑定都集中在**动画资产 / AnimBP / ControlRig / 动画通知**上。

## 1. 判断兼容性的四个前提

| 前提 | 为什么关键 |
|---|---|
| ① 模型是否**带骨骼并已蒙皮** | 没有骨骼＝不能当角色用（无蒙皮、无动画、进不了 AnimBP/Climb/MotionWarping）|
| ② 骨架是否 **UE5 骨架**（或能映射过去）| 决定动画、AnimBP、ControlRig、动画通知能不能直接沿用 |
| ③ **比例 / 身高** | 决定胶囊、相机高度、交互射线起点、攀爬点是否要对齐 |
| ④ 是否**自带动画** | 自带动画要不要重定向；没有动画就要沿用现有动画（那就必须满足 ②）|

## 2. 四种情况的结论

| 情况 | 能否兼容 | 需要做什么 | 工作量 |
|---|---|---|---|
| **A. 用 UE5 骨架（或同骨架变体）** | ✅ **直接兼容** | 把两个 SkeletalMeshComponent 的 `SkeletalMesh` 换成新模型即可（材质自带）；AnimBP / Climb Montage / MotionWarping / 脚步通知**全部照旧** | 20 分钟 |
| **B. 别的骨架（Mixamo / Blender 自制 / 自带 T-Pose）** | ⚠️ **能兼容但要改造** | ① 用 **IK Retargeter** 把现有动画重定向到新骨架（或反向：在新骨架上绑 UE5 骨架的蒙皮）② `ABP_Unarmed` / `ABP_FP_Copy` 里的骨骼引用要改 ③ **`CtrlRig_FPWarp` 必须重建**（Control Rig 与骨架绑定）④ 动画通知（脚步音效）要在新动画上重加 ⑤ Climb 的 Montage 要重定向 | 数小时～数天 |
| **C. 只是静态网格（AI 生成 / Tripo / 三维扫描，无骨骼）** | ❌ **不能当角色直接用** | 只能：① 当挂件挂在相机/胶囊上（不做动画时走路会"滑板"）② 当场景摆件 ③ 先用外部工具**骨骼绑定 + 蒙皮**，再按 A/B 走 | 需要外部绑定 |
| **D. 带骨骼 + 自带动画（FBX 含 anim）** | ⚠️ 同 B | 按 B 处理动画；另外注意 root motion、动画命名、通知、以及**是否与现有移动速度匹配**（否则滑步）| 数小时 |

> **最省事也最稳的路线**：让新模型**使用 UE5 骨架**（在 Blender/Maya 里把新模型的蒙皮绑到 UE5 骨架上，或在导出时用 UE5 骨架的绑定姿态）
> —— 这样走的是 **A**，跳过 B 的全部改造，动画/攀爬/运动音频/ControlRig 一个都不用动。

## 3. 无论哪种情况都要检查的风险清单

| # | 风险 | 具体后果 | 处理 |
|---|---|---|---|
| 1 | **第一人称有两套网格** | 现在 3P 身体与 1P 网格各一个，用的是同一个 `SKM_Manny_Simple`；新模型可能只有一套 | 先决定：只换身体 / 只换 1P / 两套都换；两套都换时要处理**头挡视野**（隐藏头部骨骼或用 1P 专用网格）|
| 2 | **胶囊与身高不匹配** | 胶囊还是 96/34，模型 1.6m 或 2.2m ⇒ 脚悬空/穿地、相机过高过低 | 按新模型调 `CapsuleHalfHeight`/`CapsuleRadius`，并把相机（`CameraBoom` / `CameraComponent`）高度跟着调 |
| 3 | **交互射线与提示** | 探测器从相机打射线；模型带碰撞块或体型变了会挡射线/让提示位置不对 | 骨骼网格保持 `CharacterMesh` 碰撞配置（不要 Block）；必要时调 `InteractionDetector` 的探测距离与 `InteractionPrompt` 的偏移 |
| 4 | **动画通知（脚步音效）** | 脚步音效**靠 `UAnimNotify_MovementAudio`**（`bAutoFootstepByDistance` 默认关）⇒ 换动画后**脚步会没声音** | 在新动画上加同名通知；或临时打开 `bAutoFootstepByDistance` 用距离兜底 |
| 5 | **ControlRig / MotionWarping** | `CtrlRig_FPWarp` 与骨架绑定，换骨架即失效 | 走 A 路线可忽略；走 B 必须重建 Control Rig（或先关掉 MotionWarping 相关效果）|
| 6 | **攀爬系统** | `ClimbComponent` 用 `GetMesh()->GetAnimInstance()` + `PlayAnimMontage` + `OnMontageEnded` ⇒ 蒙太奇必须与新骨架兼容 | A 路线照旧；B/D 路线要重定向攀爬蒙太奇 |
| 7 | **材质与渲染** | 骨骼网格材质必须勾 **`Used with Skeletal Mesh`**；Nanite **不适用于骨骼网格**；4K 贴图 + 无 LOD 会掉帧 | 导入后检查材质 Usage；按需减面 / 做 LOD |
| 8 | **单位与缩放** | FBX/GLB 单位不同 ⇒ 导入后 100 倍/0.01 倍大小 | 导入时用正确的缩放（GLB 一般 1 单位=1cm 需 ×100；FBX 勾 Convert Scene）|
| 9 | **打包 / 联机无关** | 骨骼网格、材质、贴图会被硬引用，自动 cook 进包 ✓；C++ 不依赖具体骨架 ✓ | 无需额外打包设置 |
| 10 | **旧模型的引用点** | 现有 `ABP_FP_Copy`、`CtrlRig_FPWarp`、Climb Montage、MovementAudio 通知都指向旧骨架 | 替换前先列出这些引用（我可以脚本扫一遍），避免换完发现某处还在用旧骨架 |

## 4. 推荐实施步骤（按 A 路线）

1. **把模型文件放进工程**（例如 `Content/models/` 或新建 `Content/Characters/NewModel/`），把路径告诉我；
2. 我先做一次**只读体检**（headless 脚本）：骨架名 / 骨骼数 / 是否已蒙皮 / 三角形数 / LOD 数 / 材质槽与 Usage / 包围盒与身高 / 是否带动画；
   —— 这一步就能给出"走 A 还是 B/C"的确定结论；
3. 选一个目标网格（走 A）：把 `BP_FirstPersonCharacter` 里**身体网格**（和/或 **1P 网格**）的 `SkeletalMesh` 换掉；
4. 按新模型身高调胶囊半高/半径 + 相机高度；
5. 检查材质 Usage（`Used with Skeletal Mesh`）与 LOD；
6. **PIE 验收**（见 §5）；
7. 需要的话我再把改动整理成一次提交。

## 5. 验收清单

1. 站姿/蹲伏/跳跃时身体与胶囊贴合（脚不悬空、不穿地）；
2. 第一人称视角里**看不到自己的头/身体内部**（或按需保留手臂）；
3. 走动/跑/跳/落地动画正常（不滑步、不扭曲）；
4. **脚步音效在**（换动画后重点检查这条）；跳跃/落地音效本来靠自动检测，不受影响 ✓；
5. 攀爬（Climb）能正常播蒙太奇并回到移动；
6. 交互提示与描边（overlay）位置正常，射线没被身体挡；
7. `stat unit` 看性能（骨骼网格蒙皮开销 + 三角形数）可接受；
8. 出一次 Shipping 包，成品里模型生效（骨骼网格/材质会被硬引用，自动进包 ✓）。

## 6. 需要你提供的信息（给了就能给确定结论）

1. **模型文件**（路径 + 格式：FBX / GLB / USD / …），是否**带骨骼**、是否**自带动画**；
2. 骨架是**UE5 骨架**、**Mixamo**、还是自制？（如果有 `.fbx` 的骨架名，直接告诉我）
3. 目标：**只换身体**、**只换第一人称手臂**、还是**两套都换**？第一人称要不要保留手臂？
4. 期望**身高**（cm），以及是否允许我调胶囊尺寸；
5. 攀爬动画是否要保留（要的话建议走 A 路线）；
6. 有没有**必须保留**的现有表现（脚步音效、MotionWarping 的位移吸附等）？
