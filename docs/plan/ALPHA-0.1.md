# AudioScope by BerryStudio 0.1-alpha — 双端技术预览构想

> 代号：**Dual Spike**
> 目标平台：macOS 12+（arm64）+ Windows 10/11（x64）
> 语言：**C11 实时核心 + C++17 应用/平台层**
> 周期：6–8 周（1–2 名工程师 + AI coding 辅助）
> 状态：Concept · 2026-11-07
> 关联：[`ROADMAP.md`](ROADMAP.md) §5.2 · [`BACKLOG.md`](BACKLOG.md) B-alpha · [`AGENTS.md`](AGENTS.md)

---

## 1. 一句话

**0.1-alpha 不是功能版，而是双端可行性证明：同一套 C 核心 + C++ 应用层，在 macOS CoreAudio 和 Windows WASAPI 上跑出同样的示波器、FFT、信号源、单点校准和安全闭环。**

它的价值不是“能修多少设备”，而是回答三个问题：

1. 共享核心 + 双端平台层的架构是否成立？
2. Windows 的 WASAPI 延迟/格式/设备枚举是否足够支撑维修场景？
3. 同一段音频在两端分析，结果是否一致？

---

## 2. 为什么先做 alpha

如果直接把 Windows 留到 v2.0，会在 macOS 上堆大量无法移植的代码，最后要么重写，要么放弃 Windows。0.1-alpha 用最小功能集验证架构，把风险提前：

- **架构风险**：C/C++ 边界、平台抽象、UI 渲染接口。
- **平台风险**：WASAPI 共享模式、采样率/格式、设备热插拔。
- **产品风险**：Windows 维修用户是否真的需要这个工具。
- **工程风险**：CMake + 双 CI + 双打包是否能稳定运行。

alpha 失败的成本是 6–8 周；v2.0 才发现架构错误，成本是半年。

---

## 3. 目标与非目标

### 3.1 目标

- macOS 12+ arm64 与 Windows 10/11 x64 均可构建、启动、采集、播放。
- 2 进 2 出、48 kHz、128 帧，连续 30 分钟无 xrun。
- 时域示波器：2 通道、触发、时基、垂直档位、峰值/频率读数。
- FFT：4k/16k、Hann、峰值标记。
- 信号源：正弦/方波，−20 dBFS。
- 单点校准：手工输入 0 dBFS 对应电压；显示 `measured/datasheet/assumed`。
- 安全最小集：预检提示、clip 锁存、输出静音、启动静音。
- 保存：WAV、CSV、JSON 会话日志。
- 双端 CI：macOS 13/14 + windows-2022。
- 双端可分发：macOS `.app`/DMG（ad-hoc），Windows 便携 ZIP。

### 3.2 非目标（alpha 明确不做）

- 4/8 通道、通道映射、聚合设备。
- XY/Lissajous、通道匹配、clock drift、wow & flutter、方位角。
- Spectrogram 热力图、余晖、THD 热图。
- 校准向导、逐增益档、输入阻抗、设备库。
- 维修模板、前后对比、HTML/PDF 报告。
- 硬件前端、签名/公证、安装器、自动更新。
- 中英文国际化、App Store、Linux。
- ASIO、WASAPI 独占模式、多设备聚合。

> **alpha 不能用于真实维修决策。** 它只验证“能不能跑通、跑得准不准、架构是否可持续”。UI 与文档必须显著标注 alpha。

---

## 4. 目标用户与场景

| 用户 | alpha 场景 | 成功标准 |
|---|---|---|
| Windows 维修爱好者 | 下载 ZIP，选接口，接效果器，看到 1 kHz 波形与 FFT | 10 分钟内跑通 |
| macOS 现有用户 | 用同一套 UI 和测量逻辑，确认没有退化 | 与 v1.1 的 1 kHz 读数一致 |
| 开发者 | 只改核心一次，两端同时受益 | 核心测试两端通过 |
| 产品/维护者 | 判断是否继续投入 Windows | 架构不需要大改 |

---

## 5. 功能范围

| 模块 | alpha 行为 | 验收 |
|---|---|---|
| 设备枚举 | macOS CoreAudio / Windows WASAPI；输入/输出设备、默认设备、采样率 | 两端各识别 2 款接口 |
| 音频 I/O | 2ch、48k、128 帧、float32；输入 ring + 输出生成 | 30 min xrun=0 |
| 时域 | 2ch、边沿触发、时基 1 ms–1 s/div、垂直 0.01–5 V/div、峰值/频率 | 1 kHz 正弦频率 ±0.1%、幅度 ±0.5 dB |
| FFT | 4k/16k、Hann、dBFS、峰值标记 | 1 kHz 峰值 ±1 bin |
| 信号源 | 正弦/方波、−20 dBFS、单通道/双通道 | 输出频率/幅度可测 |
| 校准 | 单点：输入 0 dBFS 电压；置信度 | 已校准误差 < 1 dB |
| 安全 | 预检弹窗、clip 锁存、输出静音、启动静音 | clip 后 100 ms 内静音 |
| 保存 | WAV 24-bit、CSV、session.jsonl | 可重新打开/解析 |
| 设置 | JSON；设备/采样率/窗口位置 | 重启恢复 |
| UI | 暗色主题、30 Hz、键盘基本操作 | 两端布局一致 |
| 报告 | 仅 JSON/CSV；无 HTML/PDF | 字段可追溯 |

---

## 6. 架构与语言

### 6.1 分层

```
┌──────────────────────────────────────────────┐
│ app/ + ui/            C++17                   │
│ 状态、控制器、设置、会话、视图模型、Canvas     │
├──────────────────────────────────────────────┤
│ platform/macos/       ObjC++ / C              │
│ CoreAudio + CoreGraphics + AppKit             │
│ platform/win/         C++ / Win32             │
│ WASAPI + Direct2D + DirectWrite               │
├──────────────────────────────────────────────┤
│ core/                 C11（extern "C"）        │
│ DSP / FFT / ring / safety / units / calib     │
└──────────────────────────────────────────────┘
```

### 6.2 语言决策

| 层 | 语言 | 理由 |
|---|---|---|
| 实时核心 | **C11** | 无隐藏分配/异常/RTTI；ABI 简单；易审计；复用 v1.1 |
| 应用层 | **C++17** | RAII、容器、字符串、`optional/variant/filesystem`；复杂状态与 schema |
| macOS 平台 | **Objective-C++** | 直接桥接 AppKit/CoreGraphics 与 C++ 应用层 |
| Windows 平台 | **C++17 + Win32/COM** | WASAPI/Direct2D 的天然语言；MSVC 工具链 |
| 构建 | **CMake 3.24+** | 双端构建、CTest、CI；构建工具不是运行时依赖 |

**硬规则**

- C++ 不跨 C ABI：所有核心接口 `extern "C"`，不传 STL 类型。
- 实时回调禁用：`new/delete`、`std::vector/string/function`、异常、锁、文件 IO。
- 核心编译 `-fno-exceptions -fno-rtti`（MSVC `/EHs-c- /GR-`）。
- 异常只在应用层使用，且在 C ABI 边界捕获，不允许穿透 C 代码。
- 不使用 Qt/ImGui/Boost 等第三方 UI/基础库。
- C++17 起步；C++20 只在双端工具链都稳定后评估。

### 6.3 目录

```
audioscope/
├── CMakeLists.txt
├── src/
│   ├── core/                 # C11
│   │   ├── dsp/
│   │   ├── audio/            # ring, format, device model
│   │   ├── safety/
│   │   ├── calib/
│   │   └── units/
│   ├── app/                  # C++17
│   │   ├── state/
│   │   ├── settings/
│   │   └── session/
│   ├── ui/                   # C++17
│   │   ├── canvas.h          # 平台无关绘制接口
│   │   ├── theme/
│   │   └── views/
│   ├── platform/
│   │   ├── macos/            # CoreAudio, CoreGraphics, AppKit
│   │   └── win/              # WASAPI, Direct2D, Win32
│   └── main/
│       ├── main.mm           # macOS
│       └── main_win.cpp      # Windows
├── tests/
│   ├── core/                 # C tests
│   ├── app/                  # C++ tests
│   └── golden/
└── packaging/
    ├── macos/
    └── windows/
```

### 6.4 CMake 目标

```cmake
audioscope_core        # static lib, C11, no platform deps
audioscope_app         # static lib, C++17, links core
audioscope_macos       # app bundle, links app + platform/macos
audioscope_win         # exe, links app + platform/win
audioscope_tests       # CTest: core + app + golden
```

构建命令：

```bash
# macOS
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=macos
cmake --build build
ctest --test-dir build --output-on-failure

# Windows
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=windows -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

---

## 7. 平台实现要点

### 7.1 macOS

- 复用现有 CoreAudio IOProc、CoreGraphics/CoreText 绘制、AppKit 窗口。
- `main.m` → `main.mm`，用 Objective-C++ 桥接 C++ 应用层。
- 部署目标 12.0；arm64；暂不做 universal（v2.0）。
- `.app` + ad-hoc 签名；DMG 可选。
- 现有 v1.1 的 DSP/校准/报告代码优先复用。

### 7.2 Windows

- **音频**：WASAPI **共享模式**、事件驱动；48 kHz、float32、2ch；默认设备 + 设备枚举。
- **格式**：若 mix format 不是 48k/float32，使用 WASAPI 自动转换或做简单重采样；alpha 只保证 48k。
- **UI**：Win32 窗口 + Direct2D/DirectWrite；DPI 感知；暗色主题。
- **线程**：WASAPI capture/render 线程 + SPSC ring；COM 每线程初始化；音频回调不做 COM 调用。
- **打包**：静态 CRT（`/MT`）或随包 VC Runtime；便携 ZIP；不做 MSI。
- **签名**：alpha 不签名；SmartScreen 警告写入文档。
- **限制**：共享模式延迟较高；不支持 ASIO/独占/聚合；alpha 明确标注。

### 7.3 平台抽象

核心侧保持 C vtable：

```c
typedef struct AsAudioBackend AsAudioBackend;
typedef struct {
    int  (*list_devices)(AsDevice *out, int max);
    int  (*open)(AsAudioBackend *b, const AsAudioConfig *cfg);
    int  (*start)(AsAudioBackend *b);
    int  (*stop)(AsAudioBackend *b);
    int  (*read)(AsAudioBackend *b, float *dst, int frames);
    int  (*write)(AsAudioBackend *b, const float *src, int frames);
    double (*latency_ms)(AsAudioBackend *b);
} AsAudioVTable;
```

应用/UI 侧用 C++ 接口：

```cpp
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void fill_rect(Rect, Color) = 0;
    virtual void stroke(Rect, Color, float width) = 0;
    virtual void text(Point, FontRole, Color, std::string_view) = 0;
    virtual void image(ImageHandle, Rect) = 0;
    virtual void clip(Rect) = 0;
};
```

`AsWindow`、`AsFiles`、`AsTime` 同理。平台层实现接口，核心不感知 OS。

---

## 8. 里程碑（6–8 周）

| 周 | 交付 | 退出标准 |
|---|---|---|
| W1 | CMake、`core` 静态库、C ABI、CI 骨架 | macOS + Windows 空壳可构建 |
| W2 | 平台接口、macOS 适配、Windows 设备枚举 | 两端能列出设备 |
| W3 | WASAPI 后端、Direct2D 窗口、时域视图 | Windows 能看到输入波形 |
| W4 | 共享 app 状态、FFT、信号源 | 两端 1 kHz 正弦/FFT 可测 |
| W5 | 单点校准、预检、clip 锁存/静音 | 校准读数与安全行为一致 |
| W6 | WAV/CSV/JSON、设置、打包 | 两端可保存/重开 |
| W7 | HIL 矩阵、双端一致性、修 bug | 2 设备/端 30 min 无 xrun |
| W8 | 文档、已知问题、alpha 发布 | 可下载、可复现、可反馈 |

---

## 9. 验收标准

### 9.1 构建与 CI

- [ ] macOS 13/14 + windows-2022 均绿。
- [ ] `ctest` 覆盖 core/app/golden。
- [ ] 无新警告（`-Wall -Wextra -Werror`；MSVC `/W4 /WX`）。
- [ ] ASan/UBSan（macOS）干净；Windows 可选 ASan。
- [ ] 产物：macOS `.app`/DMG、Windows 便携 ZIP。

### 9.2 音频

- [ ] 2ch/48k/128 帧，30 min，xrun=0（每端 2 款设备）。
- [ ] 设备热插拔不崩溃；设备丢失有提示。
- [ ] 采样率 44.1k/48k 至少各测一次（48k 为必测）。
- [ ] 输出无爆音；启动静音 200 ms。

### 9.3 测量

- [ ] 1 kHz 正弦：频率 ±0.1%，幅度 ±0.5 dB（单点校准后）。
- [ ] FFT 峰值 ±1 bin。
- [ ] 同一 WAV 在 macOS/Windows 分析：频率差 < 0.05%，幅度差 < 0.5 dB。
- [ ] 触发稳定；时基/垂直档位可调。

### 9.4 安全

- [ ] 未完成预检不能开始采集。
- [ ] clip 锁存并 100 ms 内静音。
- [ ] 输出默认限幅 −20 dBFS。
- [ ] UI 明示“无硬件前端，最大输入未知”。

### 9.5 数据

- [ ] WAV 24-bit 可被 Audacity/REAPER 打开。
- [ ] CSV 含时间/通道/电压/dBFS。
- [ ] `session.jsonl` 可解析；含设备/采样率/clip/设置。
- [ ] 设置重启恢复。

### 9.6 发布

- [ ] 两端安装/解压后 10 分钟内跑通首次测量。
- [ ] 已知问题文档完整。
- [ ] alpha 免责声明在首启与 README 可见。
- [ ] 反馈模板（Issue）可用。

---

## 10. 交付物

- macOS：`AudioScope-0.1.0-alpha-macos-arm64.dmg`（ad-hoc）或 `.app` ZIP。
- Windows：`AudioScope-0.1.0-alpha-win-x64.zip`（便携，含 exe 与必要运行库）。
- 源码：CMake 工程、CI 配置、测试。
- 文档：`BUILD.md`、`RUN.md`、`KNOWN-ISSUES.md`、`ALPHA-NOTES.md`、`TEST-MATRIX.md`。
- 演示：30–60 秒 GIF/视频，展示两端同一测量。
- 反馈：GitHub Issue 模板（Windows/macOS/音频设备）。

---

## 11. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| WASAPI 共享模式延迟/格式不匹配 | 高 | 固定 48k/float32；自动转换；alpha 只承诺 48k |
| Direct2D 文本/DPI 差异 | 中 | 统一 theme token；DPI 感知；截图对比 |
| C/C++ ABI 错误 | 高 | `extern "C"`；不传 STL；边界测试；ASan |
| CMake 双端构建复杂 | 中 | 最小 target；CI 从 W1 开始；不做插件 |
| 设备驱动差异 | 高 | 2 款/端 HIL；错误提示；已知问题 |
| alpha 被当成生产版 | 高 | 首启免责声明；版本号 `0.1.0-alpha`；文档显著标注 |
| 无硬件前端导致损坏 | 高 | 预检 + 明确“最大输入未知”；不承诺保护 |
| 范围蔓延 | 高 | 非目标清单；新需求进 v1.2 backlog |

---

## 12. 退出标准与下一步

**alpha 通过的条件**

- 双端架构不需要大改。
- Windows 音频稳定，UI 可用。
- 核心/应用边界稳定，v1.2 可以直接加功能。
- 至少 5 名 Windows 用户完成试用并给出反馈。

**alpha 不通过的处理**

- 若 WASAPI 共享模式不可接受：评估独占模式/ASIO，再决定是否继续 Windows。
- 若 C++/C 边界成本过高：退回“C 核心 + 平台 C 适配 + 更薄的 C++ 层”。
- 若 Direct2D 成本过高：评估 Skia（第三方，需重新评估零依赖）或最小 GDI 回退。

**下一步**

- 进入 v1.2：安全、校准向导、输入模式、缩放、事件日志/HTML 报告，双端同步。
- v1.3：多通道、XY/Lissajous、漂移、热力图。
- v1.4：维修模板、wow & flutter、方位角、串扰、阻抗、前后对比。
- v2.0：通用二进制、签名/公证、安装包、国际化、设备库。

---

## 13. 开放问题

1. Windows alpha 用共享模式还是直接上独占模式？
2. 便携 ZIP 还是 MSI？alpha 建议 ZIP。
3. 静态 CRT 还是随包 VC Runtime？alpha 建议静态 CRT。
4. Direct2D 还是 GDI 回退？alpha 建议 Direct2D。
5. C++17 是否足够？alpha 建议是。
6. 是否给 alpha 加简单崩溃上报？建议只本地日志。
7. Windows 测试设备选哪两款？建议 Scarlett 2i2 + UMC202HD。
8. 是否在 alpha 就做 universal macOS？建议不做，v2.0 再做。
