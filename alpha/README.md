# AudioScope by BerryStudio — 0.2-alpha (Safe & Calibrated Core)

> **AudioScope by BerryStudio** · `0.2.0-alpha.1` · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 原 v1.1 保留在 tag `v1.1.0-initial`；0.1-alpha 保留在 tag `v0.1.0-alpha.1`；本目录是 0.2-alpha 基线。

## 这是什么

0.2-alpha 在 0.1-alpha 的双端架构上，补齐了路线图 **v1.2 Safe & Calibrated** 的核心闭环：

- 输入模式预设（Line Pro / Line Consumer / DI / Inst / Mic / Probe）。
- SafetyGuard：预检、量程建议、clip 锁存、输出限幅。
- 单点校准：逐设备/通道/增益/pad/采样率，带置信度与 checksum。
- 信号检查助手：只建议，不自动切换。
- 测量：采集、限值、判定、重复性。
- 报告：JSON schema v2 + 自包含 HTML。
- 会话日志：JSONL 事件流。
- 双端：macOS CoreAudio、Windows WinMM（WASAPI 仍是后续目标）。

## 当前命令

| 命令 | 说明 |
|---|---|
| `version` | 版本、品牌、平台 |
| `devices` | 设备枚举 |
| `modes` | 输入模式与阻抗/最大电平 |
| `monitor [秒]` | 实时 ASCII 示波器；支持 `--mode`/`--frontend`/`--vpp`/`--cal`/`--channel` |
| `check [秒]` | 信号检查助手；`--synthetic` 可离线演示 |
| `tone [Hz] [amp] [秒]` | 输出正弦测试音；默认限幅 −20 dBFS |
| `scope` / `fft` | 合成 1 kHz 示波器 / 对数频谱 |
| `units` | dBFS/dBu/dBV/Vrms/Vpp |
| `safety` | SafetyGuard 状态与 clip 故障注入 |
| `session <dir>` | JSONL 会话日志演示 |
| `calibrate` | 写单点校准 profile |
| `calibs [dir]` | 列出校准 profile |
| `measure [秒]` | 采集 + 限值 + 判定 + JSON/HTML 报告 |
| `selftest` | 核心自检（含报告、校准、安全） |

### 常用示例

```bash
# 1) 写一个校准 profile（-20 dBFS 输入测得 0.1 Vrms -> FS 1.0 Vrms）
./build/audioscope-alpha calibrate calibrations \
  --device scarlett-2i2 --channel 0 --gain 12 --pad -6 --rate 48000 \
  --dbfs -20 --measured-vrms 0.1 --confidence measured

# 2) 查看 profile
./build/audioscope-alpha calibs calibrations

# 3) 测量 2 秒，带限值，输出 JSON/HTML
./build/audioscope-alpha measure 2 \
  --cal calibrations/scarlett-2i2-ch0-g12.0-p-6.0-48000.json \
  --mode inst --channel 0 --repeat 3 \
  --max-vpp 10 --min-rms 0.001 --max-clip 0 \
  --out reports

# 4) 信号检查（只建议，不切换）
./build/audioscope-alpha check 1
./build/audioscope-alpha check 1 --synthetic
```

## 构建

### macOS

```bash
cd alpha
make mac          # build/audioscope-alpha
make test         # selftest + scope/fft/ring smoke
```

要求：macOS 12+、Xcode Command Line Tools（clang）。链接 CoreAudio / AudioToolbox / AudioUnit / CoreFoundation。

### Windows（在 macOS 上交叉编译）

```bash
cd alpha
make win          # build/audioscope-alpha.exe
```

要求：`x86_64-w64-mingw32-gcc/g++`（Homebrew mingw-w64）。运行时为 Windows 10/11 x64，静态链接，无需安装运行库。

### CMake（macOS / Windows CI）

```bash
cmake -S alpha -B alpha/build -DAUDIOSCOPE_PLATFORM=macos
cmake --build alpha/build
ctest --test-dir alpha/build --output-on-failure
```

Windows：

```powershell
cmake -S alpha -B alpha/build -DAUDIOSCOPE_PLATFORM=windows -G "Visual Studio 17 2022" -A x64
cmake --build alpha/build --config Release
ctest --test-dir alpha/build -C Release --output-on-failure
```

## 架构

```
alpha/
├── src/core/          C11：FFT、窗、统计、发生器、ring、单位、输入模式、安全、校准、报告、会话
├── src/app/           C++17：CLI/TUI、命令、自检
├── src/ui/            C++17：平台无关的 ASCII Canvas
├── src/platform/
│   ├── macos/         CoreAudio（AudioDevice IOProc）
│   ├── win/           WinMM（0.2 回退；WASAPI 是后续目标）
│   └── stub/          无音频平台
├── CMakeLists.txt
└── Makefile
```

**边界**：核心是 C11 + C ABI；应用/UI 是 C++17；音频回调不分配、不加锁、不用 STL、不抛异常。

## 0.2-alpha 已完成的范围

- SafetyGuard：`IDLE → PRECHECK → ARMED → RUNNING → FAULT`、clip feed 锁存、量程建议。
- 输入模式：6 种预设，含标称电平、最大 Vpp、阻抗、phantom/FE 约束。
- 校准：schema v2，含 device/channel/gain/pad/rate/FS/置信度/checksum；CLI 单点向导。
- 报告：JSON schema v2 + 自包含 HTML；通过/警告/失败判定；重复 3 次的标准差。
- 信号检查：峰值/RMS/波峰因数/50 Hz hum/HF tilt，输出建议与理由。
- 输出保护：`tone` 默认限幅 0.1（−20 dBFS），`--allow-loud` 才允许更高。
- 双端构建：macOS Mach-O + Windows PE32+ x64。

## 明确限制

- **没有原生 GUI**：当前是 ASCII TUI；CoreGraphics/Direct2D 仍未实现。
- **没有缩放/平移/双光标**：路线图 UI-01…06 尚未实现。
- **Windows 用 WinMM，不是 WASAPI**：延迟较高。
- **单通道测量**：`measure` 当前一次测一个通道；双通道同时测量待 0.3-alpha。
- **没有硬件前端**：软件不能保护声卡；不要接市电、喇叭输出、未衰减胆机。
- **没有多通道/XY/漂移/热力图/维修模板/PDF**：后续版本。
- **未签名/未公证**：Windows SmartScreen / macOS Gatekeeper 可能警告。

## 安全

**软件只能预警、锁存、静音，不能阻止过压损坏声卡。** 高压/未知信号必须先经过受保护的前端（1 MΩ、±50 V、AC/DC、限流、TVS/PTC、phantom blocker）。不要接市电、喇叭输出、未衰减胆机输出。

## 测试

```bash
make test
./build/audioscope-alpha selftest
```

`selftest` 覆盖：版本/品牌、单位换算、输入模式、SafetyGuard、clip feed、量程建议、校准读写（含 pad/rate）、会话日志、FFT、报告 JSON/HTML、判定、ASCII canvas。

## 后续

- 0.3-alpha：原生 Canvas（CoreGraphics/Direct2D）、WASAPI、双通道测量、缩放/双光标。
- 0.4-alpha：多通道、XY/Lissajous、clock drift、频谱热力图。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
