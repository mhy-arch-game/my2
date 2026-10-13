# 面向用户端的打包构建（Windows · Shipping）

> 目标：把工程打成**可以直接发给玩家**的独立程序（不带编辑器、不带调试功能）。
> 本文的命令都在本机实测过（引擎 `E:\UE5_engine\UE_5.7`，工程 `E:\BaiduNetdiskDownload\my2`）。

## 0. 打包前的一次性准备

| 项 | 要求 |
|---|---|
| 引擎 | `E:\UE5_engine\UE_5.7`（二进制版，5.7.4）|
| 编译工具链 | Visual Studio 2022（`E:\VS-2022`，MSVC 14.44 + Win10 SDK 26100）|
| **编辑器必须完全关闭** | 编辑器开着会触发 Live Coding 锁，构建会直接失败（`Unable to build while Live Coding is active`）|
| Shipping 目标能编译 | 已实测通过：命令见 §2.1，产物 `Binaries/Win64/MHY_PROJ_v0_1-Win64-Shipping.exe` |

## 1. 调试功能已禁用（面向用户端不应出现的东西）

| 项 | 处理 |
|---|---|
| `InteractionDetectorComponent::bDrawDebug`（游戏里画交互拾取射线）| 关卡实例上原为 **True**，**已置 False**；C++ 类默认本来就是 false，角色蓝图没有组件覆盖（组件全部来自 C++ 类），所以游戏里生成的 Player 不会画调试线 |
| `ps.AutoTrigger`（ProximitySequence 的调试触发钩子）| 已用 `#if !UE_BUILD_SHIPPING` 包起来，**Shipping 包里根本不存在这个开关**（开发用 Development 时仍可用）|
| `ProximitySequenceComponent::DebugLogRange` / `bDebugLogSlowZone` | 关卡两堵墙上的值均为 `0` / `False`（默认就是关的）|
| 大量 `UE_LOG(...)` 诊断日志 | Shipping 下**默认根本不会被编译进去**：引擎 `Misc/Build.h` 里 `NO_LOGGING = !USE_LOGGING_IN_SHIPPING`，本工程没有定义 `USE_LOGGING_IN_SHIPPING`，所以 `UE_LOG` 全是空操作 |
| UiCue 演示 cue / `UiCueDemo_Volume` | 早已删除（见 `Docs/UiCue.md` 第 10 节）|
| 打包内容里的编辑器工具 | `Scripts/`、`Saved/`、`Docs/`、`Binaries/` 之外的编辑器资产都不参与 cook；只有 `Content/` 里的运行时资产进包 |

> 如果以后想**在 Shipping 里也保留日志**用于现场排查，在 `Source/MHY_PROJ_v0_1.Target.cs` 的 `GlobalDefinitions` 里加
> `"USE_LOGGING_IN_SHIPPING=1"` 即可 —— 面向用户端的正式包**不要**加。

## 2. 打包（命令行，推荐 · 可重复）

### 2.1 先单独编译 Shipping 目标（可选但推荐，出错信息更好读）

```powershell
& "E:\UE5_engine\UE_5.7\Engine\Build\BatchFiles\Build.bat" `
  MHY_PROJ_v0_1 Win64 Shipping `
  -Project=E:\BaiduNetdiskDownload\my2\MHY_PROJ_v0_1.uproject `
  -WaitMutex -NoUBA `
  -TempDirectory=E:\BaiduNetdiskDownload\my2\Saved\Temp\UnrealBuildTool `
  -Log=E:\BaiduNetdiskDownload\my2\Saved\Logs\build_shipping.log
```

成功标志：日志末尾 `Result: Succeeded`，并生成 `Binaries\Win64\MHY_PROJ_v0_1-Win64-Shipping.exe`。

### 2.2 一键打包（cook + stage + pak + archive）

```powershell
& "E:\UE5_engine\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  -project="E:\BaiduNetdiskDownload\my2\MHY_PROJ_v0_1.uproject" `
  -noP4 `
  -platform=Win64 `
  -clientconfig=Shipping `
  -build -cook -allmaps -stage -pak -archive `
  -archivedirectory="E:\BaiduNetdiskDownload\my2\Saved\Packaged" `
  -ddc=InstalledNoZenLocalFallback `
  -nocompileeditor -utf8output
```

| 参数 | 作用 |
|---|---|
| `-clientconfig=Shipping` | **关键**：用 Shipping 配置（无调试功能、不需要 VC 调试库、性能最好）|
| `-build` | 先编译目标的 Shipping 可执行文件（等价于 §2.1）|
| `-cook -allmaps` | 烘培所有关卡（本工程只有 `firstvision`；也可以只烘一张：`-map=/Game/FirstPerson/firstvision`）|
| `-stage -pak` | 组装到暂存目录并打成 `.pak` |
| `-archive -archivedirectory=…` | 把成品复制到指定目录（发给玩家的就是这里的东西）|
| `-ddc=InstalledNoZenLocalFallback` | **本机必需**：这台机器的 Zen/DDC 服务起不来（日志里反复 `Failed launch service`），不加会卡在 DDC 开销或直接失败 |
| `-nocompileeditor -noP4` | 不重编编辑器、不走 Perforce |

### 2.3 产物长什么样

```
Saved\Packaged\Windows\
  MHY_PROJ_v0_1.exe                                   0.16 MB   ← 玩家双击这个（启动器）
  MHY_PROJ_v0_1\
    Binaries\Win64\MHY_PROJ_v0_1-Win64-Shipping.exe   137.4 MB
    Content\Paks\MHY_PROJ_v0_1-Windows.pak             10.3 MB   ← 所有烘培过的资产
    Engine\...                                                  ← 引擎运行时（含 shader）
  Manifest_*.txt
```

> **本次实测（2026-10）**：Shipping 目标编译 107 秒；`BuildCookRun` 全程 **4 分 07 秒**；
> 整包 **895 MB**（大头是引擎运行时与 shader，资产 pak 只有 10 MB 左右，音频压缩率很高）。
> 打完后把 `MHY_PROJ_v0_1.exe` 实际起过一次：**运行 20 秒无崩溃**，且**没有生成任何日志文件** —— 
> 印证 Shipping 下 `UE_LOG` 确实被裁掉了。

### 发布记录

| 提交 | 出包日期 | 归档目录 | 整包 | 验收 |
|---|---|---|---|---|
| `0b953c6 <feat> debug and constructure`（当前版本）| 2026-10-11 | `Saved\Packaged\UserBuild_0b953c6_20261011\Windows` | **895 MB**（Shipping exe 137.4 MB + 资产 pak 10.3 MB）| 启动 20 秒无崩溃 ✓；manifest 含 `M_InteractFocusOverlay` 与 `firstvision` ✓ |
| `925df4c`（同日的验证包）| 2026-10-11 | `Saved\Packaged\Windows` | 895 MB | 启动 20 秒无崩溃 ✓ |

> 出包命令见 §2.2；把 `-archivedirectory` 指到新文件夹即可为指定版本单独归档（本表第一行就是这么出的）。

| `e6c3ded <fix> 打包时强制收 UiCue 表与配音`| 2026-10-11 | `Saved\Packaged\UserBuild_e6c3ded_20261011\Windows` | **895.7 MB**（pak 10.3 MB）| 启动 20 秒无崩溃 ✓；manifest **3110** 条，含 `DA_UiCueSet` 与全部 12 段 UiCue 配音（对比上一版新增 58 个文件）✓ |
| `ffd9867 + 工作区改动 <fix> 修复发行版 UiCue 只触发第一条`| 2026-10-11 | `Saved\Packaged\Release_20261011\Windows` | **695.3 MB**（删 pdb 前 896.3 MB；Shipping exe 137.5 MB + pak 10.3 MB）| 真 Shipping 包自检 `-UiCueSelfTest`：**9/9 条 cue 全部对上物件**（8 条按 `UiCue:xxx` 标签命中，`ui_opening` 是 GameStart）✓；双击启动 22 秒无崩溃 ✓ |
| `工作区改动 <feat> 结束游戏物件 end_of_the_game + 永续自转`（**当前版本**）| 2026-10-11 | `Saved\Packaged\Release_20261011\Windows` | **710.9 MB**（删 pdb 前 921.9 MB：exe 144.1 MB + ucas 525.1 MB）| 打包 `BUILD SUCCESSFUL` ✓；archive 与 staging 逐文件一致（只少故意删掉的 pdb）✓；cook 出的 `bclass_source/end_of_the_game` 存在、cooked `firstvision.umap/.uexp` 里能搜到 `end_of_the_game` ✓；新贴图/材质 `concrete_wall_009_diff_4k` / `_disp_4k` / `_rough_4k`（含各自 `_Mat`）已进包 ✓；发行版 `-UiCueSelfTest` 8 条 OK / 0 条 UNRESOLVED（第 9 条 `ui_opening` 是 GameStart）✓；双击启动 25 秒无崩溃 ✓；**本次为"改了关卡与贴图后再打一遍"**（`firstvision.umap` 20:56:51、两张 `_Mat` 20:57）|

> ⚠️ `Release_20261011` 是**原地重新打包**（目录名没变，里面 exe 18:57、pak/ucas 20:59 都是最新版）：只触发第一条 cue 的旧版、以及"还没有结束游戏物件"的版本都已被覆盖，不要再单独分发旧副本。
>
> 出包后手工删掉了 `Windows\MHY_PROJ_v0_1\Binaries\Win64\MHY_PROJ_v0_1-Win64-Shipping.pdb`（**210.8 MB**，发行给玩家的包不需要符号文件）。
> **不要**改用 `-nodebuginfo` 来代替这一步：UAT 会把 `Engine\Content\SlateDebug\Fonts\LastResort.*` 也当成调试文件一起剔掉
> （实测该包比 staging 少 3 个文件），为了少一个 pdb 而动运行期文件不值得 —— 归档后手删 pdb 即可。

### 踩坑记录：编辑器专有的 ActorLabel 不会进包（发行版 UiCue 只触发第一条）

UiCue 的 `InteractFirstToggle` / `ProximityToActor` 原来靠"物件的名字/标签"匹配 `TriggerId`，
而关卡里物件的**对象名是自动生成的**（`active_door_C_0`、`modern_swift_actor_C_0`、`StaticMeshActor_12`…），
我们平时说的 `men1` / `jiguanqiang1` 其实是**编辑器标签 `ActorLabel`** —— 它属于 `WITH_EDITORONLY_DATA`，
**打包后根本不存在**，`GetActorNameOrLabel()` 会退化成 `GetName()`。于是那 8 条 cue 在发行版里一条都对不上，
只剩不需要物件的 `GameStart`（`ui_opening`）会触发 —— 现象就是"触发且仅触发了第一个元事件"。

修法与约定见 `Docs/UiCue.md` §11：改为按 **对象名 / `UiCue:xxx` Actor Tag** 匹配，关卡里被引用的 8 个物件已补上 `UiCue:<原标签>`。
**凡是用编辑器标签当 id 的运行时逻辑，打包后一定失配** —— 换成对象名或 Actor Tag。

### 踩坑记录：只在运行时"扫 AssetRegistry"找到的资产不会被 cook

UiCue 的 cue 表 `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet` 是模块在运行时扫描 AssetRegistry **自动找**的，
**没有任何硬/软引用指向它** —— 于是 cooker 根本不知道它的存在，第一版包里既没有 cue 表、也没有它引用的 12 段配音
（实测：那版 manifest 里 `DA_UiCueSet` / `opening` / `kongjianzhuanhuanqi` 全是 0 命中，play 起来自然没有 UiCue）。

修法：在 `Config/DefaultGame.ini` 里显式声明要收的目录（`Project Settings → Packaging` 里也是这两项）：

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/MHY_ARCH_GAME/UI")
+DirectoriesToAlwaysCook=(Path="/Game/sound_resources")
```

### Git LFS 归档（含 UiCue 的那个包）

| 项 | 值 |
|---|---|
| 归档内容 | `Saved/Packaged/UserBuild_e6c3ded_20261011`（**含 UiCue** 的 Windows Shipping 包）|
| 方式 | **Git LFS**（`.gitattributes` 里 `Saved/Packaged/UserBuild_e6c3ded_20261011/** filter=lfs diff=lfs merge=lfs -text`）|
| 提交 | `92f06cb <build> 归档含 UiCue 的 Windows Shipping 包 e6c3ded（Git LFS：29 个文件 / 895.7 MB）` |
| 上传量 | LFS 对象 **29/29，939 MB**（已推到 `origin/develop`）|
| 另两个包 | `Windows`、`UserBuild_0b953c6_20261011`（**不含 UiCue** 的旧包）**未入库**，只在本地 |

**拉取者注意**

1. 需要安装 **git-lfs**（`git lfs install` 一次即可），否则 `Saved/Packaged/**` 只会拿到 LFS 指针文本；
2. GitHub 免费 LFS 配额（1 GB 存储）已被这次上传占掉大半，后续再归档大包请先清旧对象或改用 Release 附件；
3. **不要对 `Saved/Packaged/**` 执行 `git restore` / `git checkout`** —— 那会用指针覆盖（或触发 smudge），真实文件只留在本地 `Saved/Packaged` 里；要重来就重新出包；
4. 本机 `E:\Git`（git 2.52.0）**缺少 `git-remote-https.exe`**，用它推 https 会报 `remote helper https aborted session`；请用 **GitHub Desktop 自带的 git** 推：
   `%LOCALAPPDATA%\GitHubDesktop\app-3.6.7\resources\app\git\cmd\git.exe`（实测可正常推送 + 上传 LFS）。

> 判断方式很简单：**凡是"代码/配置里用软引用或路径字符串指向、或靠运行时扫描发现"的资产，都要写进 `DirectoriesToAlwaysCook`**；
> 硬引用（关卡/蓝图/组件属性里直接指的）不需要。自查办法就是出包后翻 `Manifest_UFSFiles_Win64.txt` 看它有没有在里面。

整包直接拷贝到目标机器即可运行（目标机需要 **VC++ 2015-2022 运行库**；要在包里附带安装器就加 `-prereqs`）。

## 3. 打包（编辑器 GUI，偶尔手动出包时用）

1. 打开 `MHY_PROJ_v0_1.uproject`（编辑器里不要有未保存的改动）；
2. 顶部菜单 **Platforms → Windows → Package Project**；
3. 选一个空目录（例如 `Saved\Packaged`）；
4. 打包期间不要用编辑器；完成后会提示 `BUILD SUCCESSFUL`。

> GUI 走的是同一套 Cook/Stage/Pak 流程，默认就是 Shipping；只是不方便指定 `-ddc=` 之类的开关，
> 所以本机更推荐 §2.2 的命令行方式。

## 4. 打包前检查清单

| 检查项 | 本工程当前状态 |
|---|---|
| 起始关卡 | `Config/DefaultEngine.ini`：`GameDefaultMap = /Game/FirstPerson/firstvision` ✓ |
| 默认 GameMode | `GlobalDefaultGameMode = BP_MHY_ARCH_GAMEGameMode` ✓ |
| 背景音乐设置 | `Config/DefaultGame.ini` 的 `[/Script/MHY_ARCH_GAME.BackgroundMusicSettings]`（音乐/音量/倍速/自动播放）会随包发布 ✓ |
| UiCue 事件表 | `/Game/MHY_ARCH_GAME/UI/DA_UiCueSet`（9 条正式 cue，字幕偏移 30）✓ |
| UiCue 触发 id 能进包 | 被 cue 引用的 8 个物件都带 `UiCue:xxx` 标签；出包前跑 `Scripts/verify_uicue_ids_shipping.py` 应为 **9/9** ✓ |
| 关卡里的临时状态 | 已还原：PlayerStart 在 `(0,0,3182)`、隐形墙 `yinxingqiang1/2` 在场、传送器 3/4 双向正常 ✓ |
| 已知缺失资产 | `/Game/texture/concrete_wall_009_nor_gl_4k`（`M_wall3` 依赖的**法线**）仍缺失 —— 2026-10-11 20:57 已补 `_diff_4k` / `_disp_4k` / `_rough_4k` 及各自 `_Mat` 并已进包，但 cook 仍报 `M_wall3 has a dependency on /Game/texture/concrete_wall_009_nor_gl_4k which does not exist`，游戏里 `M_wall3` 用默认材质。补回这张法线贴图即可消除 |
| 可选瘦身 | `PythonScriptPlugin` 的 `PythonScriptPluginPreload` 是 **Runtime** 模块会被打进包（很小）。想彻底排除，在 `.uproject` 里给它加 `"TargetAllowList": ["Editor"]`（不影响编辑器/命令行 Python 工具）|

## 5. 出包后的验收（拿成品跑一遍）

0. **先跑发行版自检**（几秒出结论，不用通关）：
   `…\Release_20261011\Windows\MHY_PROJ_v0_1\Binaries\Win64\MHY_PROJ_v0_1-Win64-Shipping.exe -UiCueSelfTest -nullrhi -unattended`
   → 同目录写出 `UiCueSelfTest.txt`，**9 条 cue 全部 `OK`** 才算过（`UNRESOLVED` 的那条在发行版里永远不会触发；看完整理掉这个报告文件，别留在包里）；
1. 双击 `MHY_PROJ_v0_1.exe` → 应**直接进入 firstvision**（不弹编辑器、不弹控制台）；
2. **开场配音 + 字幕**：进入游戏即触发 `ui_opening`（3 句，字幕在**屏幕底端**）；
3. **背景音乐**循环播放，音量/倍速与 `DefaultGame.ini` 一致；
4. 走到机关墙附近：播动画 → 播完显形 + 开碰撞；隐形墙始终挡路（穿不过去）；
5. 传送器 **4→3** 与 **3→4** 双向各按一次 E：落点分别约 `(-8935, 580, 1300)` 与 `(-8935, 580, -3700)`，不穿模；
6. 交互提示/开合门/机关灯/UiCue 各处触发与 PIE 里一致；
7. `Esc` 退出、窗口模式/分辨率正常。

## 6. 常见问题

| 现象 | 原因 / 处理 |
|---|---|
| `Unable to build while Live Coding is active` | 编辑器（或 PIE）还开着 → 完全退出编辑器再打包 |
| 卡在 `Unable to use default cache graph … no writable nodes` | 本机 DDC/Zen 有问题 → 命令里加 `-ddc=InstalledNoZenLocalFallback` |
| cook 阶段很慢/反复重试 `zen.exe service status` | 同上；本机 Zen 服务起不来，等它超时（约 30 秒）后会继续 |
| 打包报错 `MHY_PROJ_v0_1-Win64-Shipping.exe 不存在` | 先用 §2.1 单独编一次 Shipping，把编译错误看清楚 |
| 运行成品没有字/没有声音 | 检查是否漏 cook（用了 `-map=` 只烘一张时，其它关卡/资产不会进包）|
| 想让现场也能看日志 | 在 Target.cs 定义 `USE_LOGGING_IN_SHIPPING=1`（正式包不要）|
| 发行版里 UiCue **只触发第一条**（其余全没反应）| `TriggerId` 用了编辑器标签，打包后不存在。按 `Docs/UiCue.md` §11 给物件补 `UiCue:xxx` 标签，再用 `-UiCueSelfTest` 复核 |
| 发行版"看不到日志"、没法排查 | Shipping 默认 `USE_LOGGING_IN_SHIPPING=0`，`UE_LOG` 全被编掉。用 UiCue 内置的 `-UiCueSelfTest` 这类"命令行开关 + 写文件"的方式自检 |
| 要上架/关闭控制台 | 可加 `-distribution`（会去掉调试相关文件；自行确认无副作用后再用）|
| 想带 VC++ 运行库安装器 | 加 `-prereqs` |
| UAT 报 `UnauthorizedAccessException: …\Programs\AutomationTool\Saved\Logs` | AutomationTool 要往 **工作区之外**（`%LOCALAPPDATA%\UnrealEngine\Programs\AutomationTool\Saved\Logs`）建日志目录；在受限沙箱/只读目录里跑会被拦。在自己机器的普通命令行里一般不会遇到 |

## 7. 与开发期命令的区别（别混用）

| 场景 | 命令要点 |
|---|---|
| 开发中编编辑器 | `Build.bat MHY_PROJ_v0_1Editor Win64 Development …`（见 `Docs/EditorImplementationGuide.md`）|
| 无界面跑逻辑验证 | `UnrealEditor-Cmd.exe <uproject> -run=PythonScript …` / `-game -nullrhi -DDC-ForceMemoryCache …` |
| **出用户包** | 本文 §2 的 `BuildCookRun -clientconfig=Shipping …` |

## 8. 跨机交付（把 `Windows` 目录整个拷到别的 PC）

**包是自包含的**（实测 2026-10-11，`Saved\Packaged\Release_20261011`，整目录 28 个文件）：
含 Shipping exe、`Content\Paks\*.pak/.ucas/.utoc`，以及引擎运行期 DLL（D3D12Core、XAudio2_9 redist、vorbis/ogg、DirectML、onnxruntime…）。
目标机**不需要**引擎安装，也不需要工程源码。

| 检查项 | 结论 |
|---|---|
| 配置随包 | `Config/*.ini`（66 条）都在 pak 里 → `r.CustomDepth=3`、背景音乐、`DirectoriesToAlwaysCook` 等不依赖目标机设置 ✓ |
| 中文字幕字体 | `Engine/Content/Slate/Fonts/DroidSansFallback.ttf` + `EngineFonts/Faces/DroidSansFallback.ufont` 已进包（字幕用的是 Slate 默认复合字体，不依赖目标机装了哪些字体）✓ |
| 本机路径 | `Config/` 里 0 个绝对路径；exe / pak 里也搜不到 `BaiduNetdisk`、`UE5_engine`、本机用户名 ✓ |
| 异地 + 全新用户态实跑 | 整包拷到 `E:\relocate_test\`、并把 `%LOCALAPPDATA%\MHY_PROJ_v0_1` 挪走后启动：32 秒无崩溃，`-UiCueSelfTest` **9/9 OK**，用户目录被自动重建 ✓ |

### 目标机必须有的两样东西

1. **VC++ 2015-2022 x64 运行库** —— exe 动态链接 `VCRUNTIME140.dll` / `MSVCP140.dll`（已用 findstr 确认），**包里不自带**。
   目标机没装就双击直接报缺 DLL。要么让目标机先装运行库，要么出包时加 **`-prereqs`**（会把 `UEPrereqSetup_x64.exe` 一起打进包）；
2. **能跑 DX12 / SM6 的显卡 + Win10 1909+ / Win11** —— UE5.7 Windows 默认 DX12 + Lumen，内容里还有 Nanite 网格
   （已开 `bGenerateNaniteFallbackMeshes`：不支持的硬件会退化到 fallback mesh，但太老的卡仍可能起不来或画面差异明显）。

### 会"看起来不一样"、但不影响玩法与 UiCue 的

- `%LOCALAPPDATA%\MHY_PROJ_v0_1\Saved\Config\Windows\GameUserSettings.ini` 与 PSO pipeline cache 是**每台机器各自**的
  → 新机器按它自己的默认分辨率 / 画质档起步，且第一次跑会有着色器 / PSO 编译造成的卡顿；
- 音频走系统默认设备（目标机没声音设备当然听不到配音）；
- 已知缺失贴图 `concrete_wall_009_nor_gl_4k`（`M_wall3` 依赖）→ 任何机器都用默认材质，是**统一的瑕疵**而不是跨机差异；
- 启动时会在 `%LOCALAPPDATA%\<Project>\Saved\Config\CrashReportClient\` 建一个 `UECC-<GUID>\` 目录 ——
  那是崩溃上报器**每次启动**都会建的工件，不代表崩过（本机 8 次启动对应 8 个目录，时间戳全是启动瞬间）。
