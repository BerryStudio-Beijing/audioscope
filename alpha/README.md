# AudioScope 0.1-alpha — Dual Spike

> **0.1.0-alpha.1** · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 原 v1.1 保留在 tag `v1.1.0-initial`；本目录是 0.1-alpha 的独立基线。

## 这是什么

0.1-alpha 不是功能完整的维修工作台，而是**双端架构验证**：

- 同一套 C11 核心，macOS 用 CoreAudio，Windows 用 WinMM。
- 同一套 C++17 应用层与 ASCII TUI。
- 能列出设备、采集输入、输出测试音、看时域/FFT、做单点校准、写会话日志、跑自检。
- 先证明“同一核心 + 双端平台层”成立，再进入 v1.2 的安全/校准/报告。

## 当前能做什么

| 命令 | 说明 | macOS | Windows |
|---|---|---|---|
| `version` | 版本与平台 | ✅ | ✅ |
| `devices` | 设备枚举 | ✅ CoreAudio | ✅ WinMM |
| `monitor [秒]` | 输入实时 ASCII 示波器 | ✅ | ✅ WinMM |
| `tone [Hz] [amp] [秒]` | 输出正弦测试音 | ✅ | ✅ WinMM |
| `scope` | 合成 1 kHz 示波器 | ✅ | ✅ |
| `fft` | 合成 1 kHz 对数频谱 | ✅ | ✅ |
| `units` | dBFS/dBu/dBV/Vrms/Vpp | ✅ | ✅ |
| `safety` | SafetyGuard 状态机演示 | ✅ | ✅ |
| `session <dir>` | JSONL 会话日志 | ✅ | ✅ |
| `calibrate <path> [FS V] [device]` | 单点校准 JSON | ✅ | ✅ |
| `selftest` | 核心自检 | ✅ | ✅ |

## 构建

### macOS

```bash
cd alpha
make mac          # build/audioscope-alpha
make test         # selftest + scope/fft smoke
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

> 本机没有 CMake，macOS 的 `make mac` 与 Windows 的 `make win` 已实际验证；CMake 路径由 CI 验证。

## 运行

```bash
./build/audioscope-alpha version
./build/audioscope-alpha devices
./build/audioscope-alpha monitor 5
./build/audioscope-alpha tone 440 0.05 2
./build/audioscope-alpha scope
./build/audioscope-alpha fft
./build/audioscope-alpha selftest
```

Windows：

```powershell
.\build\audioscope-alpha.exe devices
.\build\audioscope-alpha.exe monitor 5
```

## 架构

```
alpha/
├── src/core/          C11：FFT、窗、统计、发生器、ring、单位、安全、校准、会话
├── src/app/           C++17：CLI/TUI、命令、自检
├── src/ui/            C++17：平台无关的 ASCII Canvas
├── src/platform/
│   ├── macos/         CoreAudio（AudioDevice IOProc）
│   ├── win/           WinMM（alpha 回退；WASAPI 是 v1.2 目标）
│   └── stub/          无音频平台
├── CMakeLists.txt
└── Makefile
```

**边界**：核心是 C11，C ABI；应用/UI 是 C++17；音频回调不分配、不加锁、不用 STL、不抛异常。

## 明确限制

- **没有原生 GUI**：alpha 用 ASCII TUI；CoreGraphics/Direct2D 是 v1.2 的 UI 层。
- **没有多通道/XY/漂移/热力图/模板/PDF**：这些在 v1.3/v1.4。
- **Windows 用 WinMM，不是 WASAPI**：延迟较高；WASAPI 是 v1.2 的目标。
- **没有硬件前端**：软件不能保护声卡；不要接市电、喇叭输出、未衰减胆机。
- **单点校准**：不是逐增益档校准；电压读数只用于 alpha 验证。
- **未签名/未公证**：Windows SmartScreen / macOS Gatekeeper 可能警告。

## 安全

**软件只能预警、锁存、静音，不能阻止过压损坏声卡。** 高压/未知信号必须先经过受保护的前端（1 MΩ、±50 V、AC/DC、限流、TVS/PTC、phantom blocker）。不要接市电、喇叭输出、未衰减胆机输出。

## 下一步（v1.2）

1. 原生 macOS/Windows Canvas（CoreGraphics + Direct2D）。
2. WASAPI 共享模式后端。
3. 逐增益档校准与置信度 UI。
4. 输入模式/信号检查助手。
5. 光标锚定缩放、独立通道量程。
6. 事件日志 + HTML 报告。

详见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md) 与 [`../docs/plan/ALPHA-0.1.md`](../docs/plan/ALPHA-0.1.md)。
