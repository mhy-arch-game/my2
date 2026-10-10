# BackgroundMusic —— 全局背景音乐（循环 · 音量 / 倍速可调）

> 需求：现有背景音乐素材，在游戏进程中**循环播放**，暴露**音量**与**倍速**两个属性。

## 1. 结构

```
UBackgroundMusicSettings     Project Settings -> Game -> Background Music
   （Music / Volume / Speed / bAutoPlay / bLoop）
            │ 启动时读取（存在 Config/DefaultGame.ini，随工程走）
            v
UBackgroundMusicSubsystem    GameInstanceSubsystem + 核心 ticker
            └─ UAudioComponent（2D，bAutoDestroy=false）→ 循环播放
```

## 2. 两个被暴露的属性

| 属性 | 改初始值的位置 | 说明 |
|---|---|---|
| **Volume（音量）** | Project Settings -> **Game -> Background Music** | 音量倍率，1 = 原始音量，范围 0–4 |
| **Speed（倍速）** | 同上 | 1 = 原速，2 = 两倍速；范围 0.05–4 |

运行时也可改（蓝图 / C++）：

```
UBackgroundMusicSubsystem* Music = UBackgroundMusicSubsystem::Get(WorldContext);
Music->SetVolume(0.6f);     // 音量
Music->SetSpeed(1.25f);     // 倍速
Music->PlayMusic();         // 开始 / 继续
Music->StopMusic();         // 停止（之后不会自动重播）
Music->IsPlaying();
Music->GetBackgroundMusicDebugString();   // 一行状态，供 Print String
```

## 3. 数值存在哪（不进代码）

`Config/DefaultGame.ini`：

```
[/Script/MHY_ARCH_GAME.BackgroundMusicSettings]
Music=/Game/sound_resources/background.background
Volume=1.000000
Speed=1.000000
bAutoPlay=True
bLoop=True
```

> 本项目已导入素材：`/Game/sound_resources/background`（SoundWave，约 **3 分 34 秒**，已勾 **Loop**）。

## 4. 循环与跨关卡

- 素材已勾 **Loop**；即使没勾，模块也会在播完后自动续播（`bLoop`）；
- 模块挂在 **GameInstance** 上 ⇒ 切关卡不中断；编辑器视口（非 PIE）不播放；
- `StopMusic()` 之后不会自动重播，需要再 `PlayMusic()`。

## 5. 排查

| 现象 | 原因 / 处理 |
|---|---|
| 完全没有音乐 | `Music` 未设置（或素材路径变了）；或 `bAutoPlay` 关了（可蓝图调 `PlayMusic`）|
| 只播一次就停 | 素材没勾 Loop 且 `bLoop` 被关；或 `Volume = 0` |
| 改了 Project Settings 没反应 | 那是**初始值**；运行中的改动要用 `SetVolume` / `SetSpeed` |
| **倍速同时改变了音高** | SoundWave 的固有条目（速率=音高）。要"加快但不变调"需要 MetaSound 的 Time Stretch 类节点，不是本模块的范围 |
| 想按场景换曲子 | 调 `SetVolume(0)` + 换 `Music` 资产（或后续给本模块加一个"按关卡/状态切歌"的接口）|

## 6. 相关文件

| 文件 | 作用 |
|---|---|
| `Source/MHY_ARCH_GAME/BackgroundMusic/BackgroundMusicSettings.h` | 全局设置（Project Settings 页）|
| `Source/MHY_ARCH_GAME/BackgroundMusic/BackgroundMusicSubsystem.{h,cpp}` | 循环播放与运行时接口 |
| `Config/DefaultGame.ini` | 默认音量 / 倍速 / 素材 / 自动播放 |