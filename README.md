# AudioScope by BerryStudio

**用声卡做音频测量的维修工作台** · Sound-card audio repair workstation

[![Version](https://img.shields.io/badge/version-1.0.0-brightgreen.svg)](https://github.com/mmr0817/audioscope/releases/latest)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%2012%2B%20arm64%20%7C%20Windows%2010%2F11%20x64-lightgrey.svg)](alpha/README.md)
[![Runtime](https://img.shields.io/badge/runtime-offline%20%C2%B7%20no%20webview%20%C2%B7%20no%20telemetry-informational.svg)](alpha/README.md)

AudioScope 把一块普通声卡变成一台七页测量仪器：波形、频谱、相位/XY、通道测量、
维修流程、报告与校准。界面是 macOS AppKit/CoreGraphics 与 Windows Win32/GDI+
原生绘制，核心是 C11，应用层是 C++17；没有 WebView、没有网络访问、没有遥测，
也没有第三方运行时依赖。

> 1.0 是第一个正式版。重点不是“再加一页”，而是让屏幕上的数字**可相信**：
> 双通道独立计算、实际采样率贯通、模拟结果明确标注、输出默认 −20 dBFS。

---

## 下载

- **macOS 12+ (arm64)**：`.dmg` / `.zip` — 解压或挂载后拖入 Applications。
- **Windows 10/11 (x64)**：`setup.exe` 安装包，或便携 `.zip`。
- 前往 [Releases](https://github.com/mmr0817/audioscope/releases/latest) 下载。
- 当前二进制未签名/未公证，macOS Gatekeeper 与 Windows SmartScreen 可能提示；
  校验值见每个 Release 的 `SHA256SUMS.txt`。

---

## 功能

### 七页原生界面

| 页面 | 你能做什么 |
|---|---|
| **波形** | 采集/合成源切换、输入/输出设备选择、HOLD 冻结、时基与 ±1.0 FS 刻度 |
| **频谱** | 1024–16384 点 FFT、Hann 窗、peak-relative 100 dB 图、THD/THD+N/SNR/SINAD/SFDR/ENOB |
| **相位 / XY** | CH1 vs CH2 李萨如图、相位差、增益差、共享时钟提示 |
| **测量** | 8 通道表（双通道独立统计）、削波/判定、发生器面板 |
| **维修** | Tape Plan（**SIMULATED** 夹具演示，不冒充实测）、loopback 连通性检查 |
| **报告** | PDF + JSON + HTML 导出、本地历史、模拟报告带 WARN 标记 |
| **校准** | 校准档案/置信度/电压可用性；未校准只显示 dBFS |

### 信号发生器

9 种波形：正弦、方波、三角、锯齿、白噪、粉噪、对数扫频、单位脉冲、阶跃。

- 方波/锯齿使用 PolyBLEP 抑制混叠；噪声固定种子，同参数逐位可复现。
- 电平预设 −60 / −40 / −30 / −20 dBFS，**默认上限 −20 dBFS**；
  输出启动 200 ms 淡入，关闭即释放设备。
- 频率预设 20 Hz – 20 kHz；扫频/脉冲/阶跃周期 50 ms – 2 s。

### 设备与采样

- 输入/输出设备循环选择，采集、发生器、loopback 都使用选中设备。
- 稳定设备 id 记忆；每 2 s 热插拔检测；选中输入消失时停采并提示，
  **不会静默切换到别的设备**。
- 统计、FFT bin、THD、相位、时间轴与 loopback 使用设备**实际采样率**。
- Windows 按 WinMM 真实设备编号打开，不再误用列表下标。

### 测量与报告

- THD、THD+N、SNR、SINAD、SFDR、ENOB、IMD、THD vs 电平/频率。
- wow/flutter、串扰、阻抗、方位角、漂移、频谱热力图（CLI 测量入口）。
- 多页 PDF：摘要 + 波形 + 频谱；JSON/HTML 同源数据与安全声明。
- 未校准不输出物理电压；模拟/夹具结果不会以“实测 PASS”发布。

### 可信与安全

- 双通道独立 peak / RMS / 频率 / Vpp / clip，报告使用每通道统计。
- clip 锁存、安全声明常驻、输出默认 −20 dBFS。
- 软件不能保护声卡：未知、高压或功率输出必须经过受保护前端。

### 键盘

`1–7` 切页 · `Space` 保持 · `C` 采集 · `R` 跑维修计划 · `E` 导出 · `T` 切主题。

---

## 截图

![波形页](docs/screenshots/1.0/0_scope.png)

![频谱页](docs/screenshots/1.0/1_spectrum.png)

![通道测量与发生器](docs/screenshots/1.0/3_measure.png)

---

## 快速开始

```bash
cd alpha
make app          # macOS: build/AudioScope.app
open build/AudioScope.app

make test         # 63 项 selftest + ringtest + 12 项 navtest + 10 项 modeltest + 七页截图
make mac          # CLI: build/audioscope-alpha
make win          # 需要 mingw-w64 的 Windows CLI 交叉编译
```

Windows 原生 GUI 与安装包由 CI 的 MSVC + Inno Setup 构建。完整说明见
[`alpha/README.md`](alpha/README.md) 与 [`docs/plan/V1.0.md`](docs/plan/V1.0.md)。

---

## 架构

```text
core/        C11：DSP、FFT、统计、相位、安全、校准、报告、PDF
app/         C++17：NativeModel / NativeEngine / 状态与测量管线
platform/    CoreAudio / WinMM、AppKit / Win32 外壳
ui/          CoreGraphics + CoreText 自绘控件与七页
```

- 零第三方运行时；只使用系统框架与 C/C++ 标准库。
- 音频回调实时安全：不分配、不加锁、不做文件 IO、不调用 UI。
- macOS 最低 12.0；Windows 最低 10；当前 Windows 后端为 WinMM 2ch。

---

## 已知限制

- **Windows 仍是 WinMM 2ch**；WASAPI、独占模式、>2ch 未实现。
- Windows 真机音频尚未在本机构建环境验证；CI 只覆盖编译、CTest 与打包。
- 设备选择是基础版：没有采样率切换、设备详情或通道映射。
- 没有校准向导、游标/缩放/平移、修复前后对比。
- Tape Plan 是固定夹具演示，不是被测设备结果。
- 二进制未签名、未公证。

详细清单见 [`alpha/KNOWN-ISSUES.md`](alpha/KNOWN-ISSUES.md)。

---

## 文档

- [`alpha/README.md`](alpha/README.md) — 构建、运行、版本说明
- [`alpha/CHANGELOG.md`](alpha/CHANGELOG.md) — 版本变更
- [`docs/plan/V1.0.md`](docs/plan/V1.0.md) — 1.0 实现与验收
- [`docs/plan/ROADMAP.md`](docs/plan/ROADMAP.md) — 产品路线
- [`AGENTS.md`](AGENTS.md) — 代码与安全硬约束

## English

AudioScope by BerryStudio is a sound-card audio repair workstation for macOS and
Windows. It provides seven native pages (scope, spectrum, phase/XY, channel
measurements, repair, reports, calibration), a nine-waveform generator, device
selection, 1k–16k FFT, THD/IMD/wow/flutter measurements, and PDF/JSON/HTML
reports. Version 1.0 makes the results trustworthy: independent per-channel
statistics, actual sample rates, explicit `SIMULATED` labelling for fixtures,
and a −20 dBFS default output ceiling. No WebView, network access, telemetry or
third-party runtime.

## License

Apache-2.0. See [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
| 音频 | sounddevice / PortAudio | CoreAudio HAL（IOProc 直连） |
| FFT | numpy（Accelerate） | 自己的 radix-2 FFT |
| 文件解码 | soundfile + afconvert | CoreAudio ExtAudioFile（WAV/MP3/FLAC/M4A/AAC/ALAC…） |

---

## 1. 编译与运行

不想自己编译：从 [Releases](https://github.com/mmr0817/audioscope/releases/latest)
下载 `AudioScope-1.1-macos-arm64.dmg`，拖进 Applications 即可。

```bash
make            # 生成 build/AudioScope
make app        # 打包成 build/AudioScope.app
make dmg        # 打包成 build/AudioScope-1.1.dmg
make release    # dist/：带架构名的 DMG + ZIP + SHA256SUMS（发布用）
make run        # 直接启动
make test       # DSP 自检（30 项）+ 单元测试（23 项）
make shot       # 离屏渲染 6 张界面图到 build/shots
make win-core   # （可选）用 mingw-w64 交叉编译纯 C 核心为 Windows PE
```

运行要求：Apple Silicon（arm64），macOS 12.0 或更新（二进制 `minos=12.0`）。
0.9 起 Windows 有独立的 Win32 + GDI+ 图形版和安装包；音频仍是 WinMM 2ch，
WASAPI 尚未实现。`win/core_main.c` 仍保留为纯 C 测量内核的交叉编译脚手架。

只依赖 Xcode Command Line Tools（clang + macOS SDK），**没有任何第三方库**。
链接的系统框架：Cocoa、CoreAudio、AudioToolbox、AudioUnit、CoreGraphics、CoreText、ImageIO。

首次运行会请求**麦克风权限**，必须允许，否则没有输入信号。

---

## 2. 界面（1.1 设计语言）

| 频谱 / FFT + 瀑布 | 闭环测试（幅频 / 相频） |
|---|---|
| ![频谱与瀑布](docs/screenshots/1_fft.png) | ![闭环测试](docs/screenshots/3_loop.png) |
| **幅频 / 相频（Bode）** | **明色主题** |
| ![Bode](docs/screenshots/2_bode.png) | ![明色主题](docs/screenshots/4_light.png) |

规范落地的几条硬规则（都能在代码里找到对应实现）：

| 规则 | 实现位置 |
|---|---|
| 语义颜色令牌，只有一处定义；暗色为主，明色不是反相 | `src/theme.h` / `src/theme.c` |
| 单一重点色（鼠尾草绿）只用于当前/选中/正常；琥珀=注意、砖红=超限 | `Palette` 里的 `accent / warn / bad` |
| 三种排印角色：Label 14（uppercase + tracking）/ Value 16（等宽数字）/ Critical 26 | `ui.c` 的 `FONT_LABEL / FONT_VALUE / FONT_CRIT`（`tnum` 特性，刷新不抖动） |
| 每屏最多两个“关键大数字”，其余行式读数 | `big_number()` / `kv()` |
| 网格 4px；组内 8、区块 16、大分组 24、图表安全区 32 | `theme.h` 的 `GAP1..GAP8` |
| 材质只有两种：raised（左上提亮/右下压暗 + 接触阴影）、recessed（顶部内阴影） | `mat_raised()` / `mat_recessed()` |
| 图表：主曲线最亮且最粗，填充只向下，网格低对比 | `polyline()` / `fill_down()` |
| 状态：hover / pressed / selected / focus / disabled / warning 都要能区分 | `ui_button/ui_toggle/ui_cycle/ui_field` |
| 无持续装饰动画；过渡 140ms，系统“减少动态”时立即 | `g_reduce_motion`（读系统设置） |
| 数字用真减号 −，缺失用 —，范围用 – | `pretty_minus()` |
| 瀑布不用彩虹色 | `as_cmap()`：鼠尾草/石墨/冷灰/靛青，单调亮度 |

页眉右侧是低频操作（帮助、主题切换）；页脚是技术状态行（设备、采样率、缓冲、溢出、消息）。

---

## 3. 功能

**主窗口四个页签**

- **波形 / 时域**：触发源/边沿/电平、时基、垂直、AC/DC、A+B/A−B；右侧 Vpp、频率两个大数字 + 行式读数
- **频谱 / FFT**：1k–128k 点、6 种窗、平均、峰值保持、对数频率范围；下方瀑布图（时间×频率）
- **频率响应**：起止频率/点数扫频，幅频 + 相频上下两图，可导出 CSV
- **闭环测试**（1.1 新增，见第 4 节）

**信号源窗口**：正弦/方波/三角/锯齿/白噪/粉噪/双音/扫频，电平 −80…0 dBFS，
播放任意系统支持的音频文件（CoreAudio 解码 + 自动重采样）。

**AUTO**（F6）：自动选通道、测频率、定时基与垂直档位并居中。

---

## 4. 闭环测试（1.1 新增）

链路：**声卡输出 → 被测电路 → 声卡输入**，全程本地，不联网、不上传。

### 4.1 模块

```
src/wavegen.c/h   测试信号发生器：相位累加器 + PolyBLEP 带限 + 固定种子噪声
src/calib.c/h     校准数据：逐频点幅相 + 延迟，JSON 读写，对数轴插值
src/analyze.c/h   采集分析：互相关对齐（亚采样）、单频点幅相、同步平均、噪声底、上升沿
src/report.c/h    报告：JSON / CSV / 24-bit WAV + 规则式建议
src/loopback.c/h  流程状态机：延迟 → 频响 → THD+N → 噪声 → 上升沿 → 报告
tests/unit.c      23 项单元测试 + 虚拟 DUT 端到端验证
scripts/run_loopback.sh  可重复测试脚本（不需要硬件）
```

### 4.2 波形精度（可量化）

| 波形 | 频率误差 | 幅度误差 | 非谐波杂散 |
|---|---|---|---|
| 正弦 | 0 ppm（20 Hz–15 kHz） | 0.000 dB | −122 dBc |
| 三角 | — | 0.000 dB | −43 dBc |
| 方波（PolyBLEP） | 0 dB 峰值 | 0.000 dB | −25 dBc（奈奎斯特附近谐波折叠） |
| 白噪/粉噪 | 固定种子，同参数逐位一致 | — | — |

这些数字是 `--unittest` 每次实测算出来的，不是写在文档里的话。

### 4.3 流程

1. **直连校准**：输出直接回接输入，扫描 20 Hz–20 kHz，记录声卡本身的幅相误差与往返延迟，
   存 `~/Documents/AudioScope/calibration.json`；
2. **接入被测电路**，勾选“扣除校准”，点“开始测试”；
3. 每点先稳定（≥40 个周期）再采 8192 点，用互相关对齐后做单频点 DFT，得到增益与相位；
4. 1 kHz 测 THD/THD+N/SNR/SINAD，静音测噪声底，1 kHz 方波测 10–90% 上升沿，扫频测延迟；
5. 输出 `report-YYYYmmdd-HHMMSS.json`、`report-*.csv`、`capture-*.wav` 到 `~/Documents/AudioScope`。

### 4.4 报告与建议

报告里有：频响 24 点（增益/相位）、峰谷差、最大偏差及频点、THD、THD+N、SNR、SINAD、
噪声底、往返延迟、上升沿、削波点数、判定。设置哈希写在 JSON 里，**同一设置重复运行逐字节一致**
（`scripts/run_loopback.sh` 会真的比对两次输出）。

建议是规则推理，不是“AI 生成”：

- 高频滚降 → 指出截止频点并给出 `f = 1/(2πRC)` 的调整方向；
- 低频衰减 → 耦合电容/变压器的量化建议；
- THD+N 偏高 → 先降 6 dB 电平复测以区分失真与噪声；
- 削波 → 自动给出下一轮电平；
- 每份报告最后一行是**下一轮参数**（电平、点数、频段加密方式）。

---

## 5. 快捷键

| 键 | 作用 |
|---|---|
| ⌘R | 开始 / 停止采集 |
| ⌘⇧A 或 F6 | AUTO 自适应 |
| ⌘G | 信号源窗口 |
| ⌘O | 打开音频文件 |
| ⌘S / ⌘⇧S / ⌘B | 保存 WAV / CSV / 频响 CSV |
| ⌘P | 截图 |
| ⌘0 | 复位缩放 |
| T | 切换明/暗主题 |
| 空格 | 冻结 |
| ← / → | 时基缩放 |
| ⌘Q | 退出 |

信号源窗口（⌘G）：内置正弦 / 方波 / 三角 / 扫频发生器，也能直接播放本地音频文件。

<img src="docs/screenshots/5_source.png" width="340" alt="信号源窗口">

---

## 6. 架构

```
src/
├── theme.h/.c        设计令牌（颜色/间距/圆角/排印），暗色与明色两套
├── dsp.h/.c          发生器、触发、时域测量、FFT、THD/SNR/SINAD/ENOB/SFDR、对数分箱、格式化
├── audio.h/.c        CoreAudio 双设备 IOProc、环形缓冲、重采样、ExtAudioFile 解码、帧计数
├── ui.h/.c           立即模式控件 + 材质 + 字体（含 tnum）+ 全部状态
├── view.c            四个页签的图表与面板、页眉/导航/页脚、信号源窗口
├── app.h/.c          状态、设置持久化、30 Hz 主循环、AUTO、扫频、闭环测试调度、导出
├── wavegen/calib/analyze/report/loopback   1.1 新增测量链
├── main.m            AppKit 外壳、菜单、事件、文件对话框、CLI 入口
└── tests/            selftest.c（30 项）、unit.c（23 项 + 虚拟 DUT）
```

音频回调在 CoreAudio 线程；UI 在 30 Hz 定时器里读环形缓冲；生成器配置用互斥锁整体替换
（1.1 修掉了 1.0 里无锁写 `gen_cfg` 的潜在撕裂读）。

---

## 7. 自检与验证

```bash
make test                          # --selftest + --unittest
./build/AudioScope --simulate DIR  # 用已知传递函数的虚拟 DUT 跑完整分析链
./scripts/run_loopback.sh          # 上面全部 + 两遍结果逐字节比对
./build/AudioScope --devices       # 列出 CoreAudio 设备
./build/AudioScope --audiotest     # 真开设备跑 2 s，打印回调/电平/溢出
./build/AudioScope --shot build/shots
```

本机（MacBook Pro，48 kHz，内置麦克风 + 耳机）实测：
`--selftest` 30/30、`--unittest` 23/23、`--audiotest` `in_cb=198 out_cb=204 xrun=0`、
`--simulate` 频响误差 0.000 dB（24 点全频段）。

---

## 8. 常见问题

**没有波形 / 峰值 −180 dBFS**：麦克风权限没给，或输入设备选错、输入增益为 0。

**闭环测试没有读数**：先确认“开始采集”是运行状态；再确认输出确实接到了输入（可先做直连校准，
校准时应看到 0 dB 附近的一条平线）。

**相位看起来在跳**：相位是相对值，超过 ±180° 会折叠；跨频点比较时看趋势，不要看绝对值。

**打包后提示「无法验证开发者」**：`make app` 已做 ad-hoc 签名；右键 → 打开，或
`xattr -dr com.apple.quarantine build/AudioScope.app`。

**读数不准**：在左栏“满量程”里填输入口 0 dBFS 对应的峰值电压（万用表标定）。

---

## 9. 许可证与贡献

本项目以 **Apache License 2.0** 开源：全文见 [LICENSE](LICENSE)，版权与归属声明见 [NOTICE](NOTICE)。

- 可自由商用、修改、再分发（包括闭源产品），需保留版权、许可证与 NOTICE 声明；
- 包含明确的专利授权条款，贡献者的专利授权不会因诉讼而随意撤回；
- 软件按“现状”提供，不含任何明示或暗示的担保。

欢迎提 Issue / PR。提交前请保持本项目的两个硬约束：

1. **零第三方依赖**——只调用 macOS 系统框架（Cocoa / CoreAudio / CoreText 等）；
2. **可复现**——`make test` 全绿，`./scripts/run_loopback.sh` 两遍报告逐字节一致。

提交代码即表示同意以 Apache-2.0 授权你的贡献（无需额外签署 CLA）。


