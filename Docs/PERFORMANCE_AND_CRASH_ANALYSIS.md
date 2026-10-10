# 性能与崩溃分析及优化实施方案

> 工程：**MHY_PROJ_v0_1**（Unreal Engine 5.7.4 / `E:\BaiduNetdiskDownload\my2`）
> 依据：`Saved/Crashes` 下 4 次崩溃的 `CrashContext.runtime-xml`、`Saved/Logs`、注册表显示适配器信息、`Content` 资产清单
> 约束：**本文档只做诊断与实施路径说明，未修改任何 `.uasset` / `.umap` 资产**

---

## 0. 结论速览

| 优先级 | 问题 | 一句话处置 |
| --- | --- | --- |
| **P0** | UE 实际渲染在**集成显卡**上，而不是 6 GB 的 RTX 3060 | 把 `UnrealEditor.exe` 在 Windows / NVIDIA 控制面板中指定为"高性能" |
| **P0** | 页面文件过小，导致系统内存分配失败 | 虚拟内存改为自定义 16–32 GB |
| **P1** | 6 张 4K 贴图以**未压缩 RGBA16F** 存储，单张 128 MiB | 改压缩设置 + 降最大尺寸，合计 480 MiB → 约 60 MiB |
| **P2** | Lumen + 虚拟阴影贴图在集显上开销过高 | 开发期改用阴影贴图 / 关闭 VSM |
| **P3** | `Lvl_FirstPerson` 是半转换的 World Partition 地图 | 补完转换或弃用该空地图 |

---

## 1. 硬件事实（已核实）

| 项 | 值 | 来源 |
| --- | --- | --- |
| CPU | Intel Core Ultra 9 275HX | `Saved/Logs` 的 `LogCsvProfiler` |
| 物理内存 | 16,496,603,136 B（≈15.4 GiB） | `CrashContext / MemoryStats.TotalPhysical` |
| 独立显卡 | **NVIDIA GeForce RTX 3060 Laptop GPU，6,442,450,944 B（6 GiB VRAM）** | 注册表显示适配器 `…\0000` |
| 集成显卡 | 536,870,912 B（512 MiB 共享显存） | 注册表显示适配器 `…\0001` |
| **崩溃时 UE 使用的主 GPU** | **Intel(R) Graphics（集显）** | `CrashContext / Misc.PrimaryGPUBrand` |
| 页面文件 | 系统托管（`?:\pagefile.sys`） | 注册表 `PagingFiles` |
| 操作系统 | Windows 11 25H2 (10.0.26200) | `Saved/Logs` |

**这是最关键的发现**：机器上有 6 GB 显存的 RTX 3060，但崩溃上下文记录的 `Misc.PrimaryGPUBrand` 是 **Intel(R) Graphics**。集成显卡没有独立显存，其显存来自 16 GB 系统内存——当系统内存被 Lumen、虚拟阴影贴图和 480 MiB 未压缩贴图占满后，D3D12 分配渲染资源就会直接失败。

---

## 2. 崩溃清单

| 时间 | 类型 | CrashGUID | 错误摘要 |
| --- | --- | --- | --- |
| 2026-10-05 08:31 |  | `00E533624288700DC6DF98B1640781E0` |  |
| 2026-10-06 13:58 |  | `329B23874F7B5310085F0F9C11A03AF8` |  |
| 2026-10-05 15:44 |  | `4F2B0E6C4B1A68778AB43CBA8711A0ED` |  |
| 2026-10-02 08:15 |  | `59BCF1D949C9C7800CE42D86F109D4B8` |  |
| 2026-10-03 06:15 |  | `59BCF1D949C9C7800CE42D86F109D4B8` |  |

崩溃上下文统计（`MemoryStats`）：

| 崩溃 | AvailablePhysical | AvailableVirtual | 说明 |
| --- | --- | --- | --- |
| OutOfMemory #1 | 737,382,400 B（703 MiB） | — | `Out of video memory trying to allocate a rendering resource` |
| OutOfMemory #2 | 442,458,112 B（422 MiB） | — | 同上，物理内存几乎见底 |
| OutOfMemory #3 | — | 4,210,270,208 B | 分配 445.6 MiB 失败，附加错误 `页面文件太小，无法完成操作` |

---

## 3. 根因分析

### 3.1 视频内存耗尽（2 次）——主因是用了集显

两条 `D3D12Util.cpp:810` 的错误信息都是 `Out of video memory trying to allocate a rendering resource`。
在集显上，"显存"就是系统内存的一部分；16 GB 内存被以下因素叠加消耗：

- Lumen 动态全局光照（`r.DynamicGlobalIlluminationMethod=1`）
- 虚拟阴影贴图（`r.Shadow.Virtual.Enable=1`）
- 网格距离场（`r.GenerateMeshDistanceFields=True`）
- 约 480 MiB 的未压缩 4K 贴图

### 3.2 系统内存 / 页面文件耗尽（1 次）

`GenericPlatformMemory.cpp:269` 明确记录：

> Ran out of memory allocating 467249264 (445.6 MiB) bytes … Last error msg: **页面文件太小，无法完成操作。**

这不是"物理内存不够"，而是提交限制（commit limit）耗尽——页面文件太小导致虚拟内存上限过低。

### 3.3 贴图体积异常（可量化）

`Content/texture` 共 512.4 MB，其中 6 张占 480 MiB，且字节数与**未压缩格式精确吻合**：

| 资产 | 文件大小 | 精确等于 | 推断格式 |
| --- | --- | --- | --- |
| `plastered_wall_02_nor_gl_4k` | 128.01 MB | 4096×4096×**8 B** = 128 MiB | 未压缩 RGBA16F（HDR） |
| `checkered_pavement_tiles_nor_gl_4k` | 128.01 MB | 同上 | 未压缩 RGBA16F |
| `plastered_wall_02_rough_4k` | 128.01 MB | 同上 | 未压缩 RGBA16F |
| `checkered_pavement_tiles_disp_4k` | 32.02 MB | 4096×4096×**2 B** = 32 MiB | R16（16 位单通道） |
| `plastered_wall_02_disp_4k` | 32.01 MB | 同上 | R16 |
| `checkered_pavement_tiles_rough_4k` | 32.01 MB | 同上 | R16 |

法线/粗糙度贴图**不应**以 16 位浮点未压缩存储：正确做法是法线用 BC5、粗糙度用 BC4/BC1，显存占用可下降约 6–8 倍。

### 3.4 WorldPartition Ensure `EditorHash`

`WorldPartition.cpp:2438` 的 `Ensure condition failed: EditorHash`，调用栈含 `UnrealEditor-Engine.dll` 与 `UnrealEditor-UnrealEd.dll`，属**编辑器**侧的空间哈希访问失败。
`Content/FirstPerson/Lvl_FirstPerson.umap` 只有 13.9 KB，却已在 `Content/__ExternalActors__/FirstPerson/Lvl_FirstPerson/` 下生成 0–F 外部 actor 分桶，并且旁边有 `WorldPartitionConvertCommandlet` 配置——说明它是一次**未完成的 World Partition 转换残留**。真正的工作场景是 `firstvision.umap`（935.7 KB）。

---

## 4. 优化实施方案（具体实现路径）

### P0-1　强制 UE 使用 RTX 3060

**路径 A：Windows 显卡首选项（推荐，系统级生效）**

1. 桌面右键 → 显示设置 → 图形（或"设置 → 系统 → 显示 → 显卡"）
2. 在"添加桌面应用"中浏览并选择
   `E:\UE5_engine\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe`
3. 选中后点"选项" → 选择 **高性能**
4. 对 `UnrealEditor-Cmd.exe`、`ShaderCompileWorker.exe`、`UnrealGame.exe` 重复同样设置

**路径 B：NVIDIA 控制面板**

1. 桌面右键 → NVIDIA 控制面板 → 管理 3D 设置 → 程序设置
2. "添加" → 选择 `UnrealEditor.exe`
3. "首选图形处理器" → **高性能 NVIDIA 处理器**
4. 应用

**验证**：重启编辑器后查看最新 `Saved/Logs/*.log`，搜索 `Chosen D3D12 Adapter`，应显示 NVIDIA 而非 Intel。

### P0-2　增大页面文件

1. `Win+R` → `sysdm.cpl` → 高级 → 性能"设置" → 高级 → 虚拟内存"更改"
2. 取消"自动管理所有驱动器的分页文件大小"
3. 选择系统盘 → 自定义大小：初始 `16384` MB，最大 `32768` MB（所在盘需 ≥40 GB 空闲）
4. 确定并重启

**依据**：崩溃 #3 的 `AvailableVirtual` 仅 4.2 GB，且错误信息直指页面文件过小。

### P1　贴图降载

**先做（无需改资产，仅改配置）**：在 `Config/DefaultEngine.ini` 追加

```ini
[SystemSettings]
r.Streaming.PoolSize=2000
r.Streaming.LimitPoolSizeToVRAM=1
```

**再做（需在编辑器内改资产属性，会改变画质，请自行决定）**：

1. 内容浏览器筛选 `Content/texture`
2. 用 **Bulk Edit via Property Matrix** 批量修改：
   - 选中 `*_nor_gl_4k`（法线）→ `Compression Settings` = **Normalmap (BC5)**，`Maximum Texture Size` = **2048**
   - 选中 `*_rough_4k`（粗糙度）→ `Compression Settings` = **Grayscale (BC4 或 BC1)**，取消 `sRGB`，`Maximum Texture Size` = **2048**
   - 选中 `*_disp_4k`（置换）→ 运行时不使用，`Maximum Texture Size` = **512**，或从运行时引用中移除
3. 若格式确认为 HDR/16 位：先把 `Compression Settings` 从 `HDR` 改为对应 LDR 格式，再重新压缩

**预期收益**：6 张合计显存占用约 480 MiB → 约 60 MiB。

### P2　开发期渲染设置（`Config/DefaultEngine.ini`，`[/Script/Engine.RendererSettings]`）

| 现有设置 | 现状 | 开发期建议 |
| --- | --- | --- |
| `r.DynamicGlobalIlluminationMethod=1` (Lumen) | 开销大 | 临时改 `0`（关闭 GI）或改用 SSR |
| `r.Shadow.Virtual.Enable=1` (VSM) | 开销大 | 临时改 `0`，用普通阴影贴图 |
| `r.GenerateMeshDistanceFields=True` | 为 Lumen 服务 | 关闭 Lumen 后可设为 `False` |

这些都在**文本可编辑的 ini** 中，改动安全且可随时回滚（已在 git 中）。

### P3　WorldPartition Ensure 修复

三选一：

1. **补完转换**（若确实需要 World Partition）：编辑器打开 `Lvl_FirstPerson` → 菜单 `World Partition` → `Convert Level`，确保 `EditorHashClass` 与 `RuntimeHashClass` 一致（当前为 `WorldPartitionEditorSpatialHash` / `WorldPartitionRuntimeHashSet`）
2. **回退为非 WP 地图**：`Tools` → `Convert Level`（反向），或对空地图直接重建
3. **弃用该地图**（推荐）：既然 `firstvision` 才是工作场景，删除 `Content/FirstPerson/Lvl_FirstPerson.umap` 与 `Content/__ExternalActors__/FirstPerson/Lvl_FirstPerson/`，并移除 `Content/FirstPerson/Lvl_FirstPerson.ini`

**验证**：打开该地图后 `Output Log` 不再出现 `Ensure condition failed: EditorHash`。

---

## 5. 验证清单

- [ ] `Saved/Logs/*.log` 中 `Chosen D3D12 Adapter` 显示 NVIDIA
- [ ] 系统虚拟内存 ≥ 16 GB，且不再出现 `页面文件太小`
- [ ] `Content/texture` 6 张大贴图压缩后总体积显著下降
- [ ] 连续编辑 `firstvision` 30 分钟不再产生新的 `OutOfMemory` 崩溃
- [ ] 打开 `Lvl_FirstPerson` 无 `EditorHash` Ensure（或该地图已移除）

---

## 6. 备注

- 本次工程规范化已完成：`MHY_PROJ_v0_1.uproject`、默认地图 `/Game/FirstPerson/firstvision`、默认 GameMode `BP_FirstPersonGameMode`、旧 `/Game/FirstPersonBP` 失效路径已修正。
- 崩溃报告与自动保存正在清理；`Saved/` 已在 `.gitignore` 中，不进入版本库。
