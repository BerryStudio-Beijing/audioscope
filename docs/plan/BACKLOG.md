# AudioScope by BerryStudio 实施 Backlog

> 配套：[`ROADMAP.md`](ROADMAP.md) · [`ALPHA-0.1.md`](ALPHA-0.1.md) · [`AGENTS.md`](AGENTS.md)
> 用法：按批次取任务 → 复制卡片到 prompt → 按 DoD 验收 → 一个任务一个 PR。
> 任务 ID 与路线图需求 ID 对应；后缀 `a/b/c` 表示同一需求的实现切片。

---

## 0. 任务卡模板

```text
### <TASK-ID> — <标题>
- Release: 0.1-alpha / v1.x · Size: S/M · Depends: <IDs>
- Parent: <REQ-ID>
- Files: <预计改动文件>
- Goal: <一句话>
- Acceptance:
  - [ ] <可测试的验收 1>
  - [ ] <可测试的验收 2>
- Tests:
  - <单元/黄金/仿真/手工>
- Prompt seed:
  "<可直接粘贴给 coding agent 的任务描述>"
```

**Size 定义**

| Size | 变更量 | 典型 |
|---|---|---|
| S | < 150 行 / 1–3 文件 | 单位换算、schema 字段、单个测试 |
| M | 150–400 行 / 3–8 文件 | 校准向导、状态机、视图 |
| L | > 400 行 | 必须拆分 |

**状态**

`todo` · `in-progress` · `review` · `blocked` · `done`

---

## 1. 批次总览

| 批次 | 目标 | 任务 |
|---|---|---|
| **B-alpha 双端 spike** | CMake/C ABI、平台抽象、WASAPI/D2D、最小示波器/FFT/校准/安全 | LANG-01、PLAT-02/09/10/04、CH-07、PLAT-06/07、ALPHA-01…09 |
| **B0 v1.2 基础** | alpha 之后，先立校准模型与单位 | CAL-02、CAL-03a |
| **B1 v1.2 安全** | 预检、削波、量程、输出保护 | SAFE-02、SAFE-03、SAFE-04、SAFE-05、SAFE-06、SAFE-07 |
| **B2 v1.2 校准** | 单位、校准、置信度 | CAL-01、CAL-03a/b、CAL-04、CAL-05、CAL-07、CAL-09 |
| **B3 v1.2 输入** | 模式与助手 | INP-01、INP-02、INP-04、INP-05 |
| **B4 v1.2 交互** | 缩放、平移、独立量程 | UI-01、UI-02、UI-03、UI-04、UI-05、UI-06、UI-07 |
| **B5 v1.2 报告** | 事件、报告 v2、HTML、崩溃日志 | RPT-01、RPT-02、RPT-03、RPT-06、RPT-07、PLAT-08 |
| **B6 v1.3 多通道** | 双端 8ch、映射、热插拔 | CH-01a/b/c、CH-02、CH-03、CH-04、CH-05、CH-06 |
| **B7 v1.3 相位** | 双端 XY、通道匹配、漂移 | PH-01…06 |
| **B8 v1.3 热力图** | dB 矩阵、余晖、导出 | SPEC-01…05 |
| **B9 v1.4 维修** | 双端模板、wow&flutter、串扰、阻抗 | REP-01…10、PH-07/08、SPEC-06/07、RPT-04/05/08、INP-03 |
| **B10 v2.0 规模化** | 通用二进制、公证/安装包、i18n、设备库 | PLAT-01/03/05、CAL-08 |
| **H 硬件** | 前端参考设计 | FE-0.1、FE-0.2、FE-1.0 |

> **先做 B-alpha。** 没有双端 CMake/C ABI/WASAPI 最小后端，后面的 v1.2–v1.4 都会返工。

---

## 2. B-alpha 双端技术预览（0.1-alpha，先做）

> 目标：同一 C11 核心 + C++17 应用层，在 macOS/Windows 跑通 2ch/48k 示波器、FFT、单点校准、安全最小集。
> 详细验收见 [`ALPHA-0.1.md`](ALPHA-0.1.md) §9。

### LANG-01 — CMake + core 静态库 + C ABI

- **Release**: 0.1-alpha · **Size**: M · **Depends**: none
- **Parent**: PLAT-09
- **Files**: `CMakeLists.txt`, `src/core/CMakeLists.txt`, `src/core/api.h`, `src/app/CMakeLists.txt`, `tests/core/CMakeLists.txt`
- **Goal**: 先把“同一核心、双端构建”立起来，不改 DSP 行为。
- **Acceptance**:
  - [ ] `audioscope_core` 静态库可被 macOS/Windows 构建。
  - [ ] 核心头文件可被 C 与 C++ 同时包含；接口 `extern "C"`。
  - [ ] `ctest` 跑通现有 DSP 测试。
  - [ ] 旧 Makefile 仍可构建 v1.1（过渡期）。
- **Tests**:
  - 双端空壳构建；C/C++ 包含测试；CTest。
- **Prompt seed**:
  "Add a CMake build with `audioscope_core` (C11) and `audioscope_app` (C++17). Keep the existing Makefile working. Expose core APIs via `extern \"C\"` in a header includable from C and C++. Add CTest targets for existing DSP tests. Do not change DSP behavior."

### PLAT-10 — 平台抽象接口

- **Release**: 0.1-alpha · **Size**: M · **Depends**: LANG-01
- **Parent**: PLAT-10
- **Files**: `src/core/platform/audio.h`, `src/ui/canvas.hpp`, `src/ui/window.hpp`, `src/core/platform/files.h`, `src/core/platform/time.h`
- **Goal**: 让核心/UI 不感知 macOS/Windows。
- **Acceptance**:
  - [ ] `AsAudioBackend` C vtable：枚举/打开/启停/读写/延迟。
  - [ ] `Canvas`/`Window`/`Files`/`Time` C++ 接口。
  - [ ] macOS 与 Windows 各有空实现可链接。
  - [ ] 核心不包含任何系统头。
- **Tests**:
  - 编译期接口测试；mock 后端。
- **Prompt seed**:
  "Define the platform abstraction per ROADMAP §7.7: a C `AsAudioBackend` vtable and C++ `Canvas`, `Window`, `Files`, `Time` interfaces. Provide no-op/mock implementations for both platforms so the core links without OS headers. Add compile-time tests."

### PLAT-04a — Windows WASAPI 后端（2ch/48k）

- **Release**: 0.1-alpha · **Size**: M · **Depends**: PLAT-10
- **Parent**: PLAT-04
- **Files**: `src/platform/win/wasapi.cpp`, `src/platform/win/wasapi.hpp`, `src/platform/win/device_enum.cpp`
- **Goal**: Windows 能用默认/指定设备做 2ch/48k 采集与播放。
- **Acceptance**:
  - [ ] 枚举输入/输出设备；打开默认设备。
  - [ ] WASAPI 共享模式、事件驱动、float32、48k、2ch。
  - [ ] 30 分钟 xrun=0；设备拔出有错误返回。
  - [ ] 音频回调不做 COM/分配/锁。
- **Tests**:
  - 设备枚举；30 min 压力；拔插；与 macOS 同一 WAV 对比。
- **Prompt seed**:
  "Implement a WASAPI shared-mode, event-driven backend for 2ch/48k/float32 on Windows. Enumerate devices, open default device, start/stop, read/write interleaved float frames. Keep the audio callback free of COM/allocation/locks. Add a 30-minute stress test and device-removal handling."

### PLAT-04b — Windows Direct2D/Win32 窗口

- **Release**: 0.1-alpha · **Size**: M · **Depends**: PLAT-10
- **Parent**: PLAT-04
- **Files**: `src/platform/win/window.cpp`, `src/platform/win/d2d_canvas.cpp`, `src/main/main_win.cpp`
- **Goal**: Windows 端有可用的最小 UI：窗口、暗色背景、文本、波形/FFT 绘制。
- **Acceptance**:
  - [ ] Win32 窗口 + Direct2D/DirectWrite。
  - [ ] DPI 感知；窗口缩放不崩。
  - [ ] `Canvas` 实现：fill/stroke/text/image/clip。
  - [ ] 与 macOS 同一 theme token。
- **Tests**:
  - 手测 + 截图；DPI 100%/150%；窗口 resize。
- **Prompt seed**:
  "Implement a Win32 + Direct2D/DirectWrite window and a `Canvas` implementation (fill_rect, stroke, text, image, clip). Use the same theme tokens as macOS, support DPI awareness, and handle resize without crashing. Add a minimal `main_win.cpp` that opens the window and draws a test waveform."

### PLAT-02 — 双端 CI

- **Release**: 0.1-alpha · **Size**: S · **Depends**: LANG-01
- **Parent**: PLAT-02
- **Files**: `.github/workflows/ci.yml`, `CMakeLists.txt`
- **Goal**: 每个 PR 双端构建/测试，失败阻断。
- **Acceptance**:
  - [ ] macOS 13/14：CMake + CTest + ASan + shot + `.app`。
  - [ ] windows-2022：MSVC 2022 + CMake + CTest + ZIP。
  - [ ] Ubuntu：`make win-core` 烟测。
  - [ ] 上传 artifacts。
- **Tests**:
  - 故意失败一个测试，确认双端 CI 红。
- **Prompt seed**:
  "Add a GitHub Actions CI matrix for macOS 13/14 and windows-2022. Build with CMake, run ctest, upload screenshots/artifacts. Keep a Ubuntu job for `make win-core`. Fail the PR on any job failure."

### ALPHA-01 — 双端时域示波器

- **Release**: 0.1-alpha · **Size**: M · **Depends**: PLAT-04a/b, CH-07
- **Parent**: ALPHA-01
- **Files**: `src/ui/views/scope.cpp`, `src/app/state.cpp`, `src/core/dsp/trigger.c`
- **Goal**: 2 通道、触发、时基/垂直档位、峰值/频率读数。
- **Acceptance**:
  - [ ] 48k/128 帧，2ch 波形显示。
  - [ ] 边沿触发、时基 1 ms–1 s/div、垂直 0.01–5 V/div。
  - [ ] 峰值/频率读数；与已知信号误差达标。
  - [ ] 双端布局一致。
- **Tests**:
  - 1 kHz 合成信号；触发稳定性；双端截图。
- **Prompt seed**:
  "Implement a minimal cross-platform scope view: 2 channels, edge trigger, timebase 1 ms–1 s/div, vertical 0.01–5 V/div, Vpp/frequency readouts. Use the shared Canvas interface and app state. Verify with a 1 kHz synthetic signal on macOS and Windows."

### B-alpha 其余任务（紧凑表）

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| CH-07 | 音频回调无锁化（立体声，双端） | M | LANG-01 | 0 malloc/mutex/IO；30 min xrun=0；TSan |
| PLAT-06 | settings.ini → settings.json | S | LANG-01 | 旧设置迁移；损坏回退 |
| PLAT-07 | 启动/UI/回调性能基线 | S | LANG-01 | 记录基线；超预算警告 |
| ALPHA-02 | FFT 视图（4k/16k、Hann、峰值） | M | ALPHA-01 | 1 kHz ±1 bin；双端 |
| ALPHA-03 | 信号源（正弦/方波、−20 dBFS） | S | PLAT-04a | 输出可测；无爆音 |
| ALPHA-04 | 单点校准 + 置信度 | M | ALPHA-01 | 已校准 <1 dB；三态标签 |
| ALPHA-05 | 安全最小集（预检/clip/静音） | M | CH-07 | 预检阻断；clip 100 ms 静音 |
| ALPHA-06 | WAV/CSV/session.jsonl | M | PLAT-10 | 可重开/解析；字段完整 |
| ALPHA-07 | 双端打包（DMG/ZIP） | S | PLAT-04b | 可下载；解压即用 |
| ALPHA-08 | HIL/双端一致性 | M | ALPHA-01…06 | 2 设备/端；同一 WAV 差达标 |
| ALPHA-09 | 文档/已知问题/免责声明 | S | ALPHA-07 | 首启可见；反馈模板 |

---

## 3. B0 v1.2 校准基础（alpha 之后）

### CAL-02 — 校准数据模型 + JSON schema

- **Release**: v1.2 · **Size**: M · **Depends**: none
- **Parent**: CAL-02
- **Files**: `src/calib.h`, `src/calib.c`, `tests/unit.c`, `docs/schema/calibration.md`
- **Goal**: 用版本化 `Calibration` 替代单值 `CalibData` 的电压假设，支持逐设备/通道/增益档。
- **字段**: `schema_version`, `id`, `device_id`, `device_name`, `input_channel`, `gain_db`, `pad_db`, `sample_rate`, `fs_volts_rms`, `fs_volts_peak`, `input_impedance_ohm`, `coupling_hz`, `method`, `confidence`, `created_at`, `app_version`, `points[]`, `checksum`。
- **Acceptance**:
  - [ ] JSON 读写；schema 校验；未知字段保留或报错策略明确。
  - [ ] 旧 `calibration.json` 可导入，标记 `confidence: assumed/datasheet`。
  - [ ] 原子写入（tmp + rename）；损坏文件不崩溃。
  - [ ] `calib_gain_at` 等旧接口保留兼容或提供适配层。
- **Tests**:
  - round-trip；损坏/缺字段/未来版本；旧格式迁移。
- **Prompt seed**:
  "Introduce a versioned Calibration model per ROADMAP §6.2.1. Keep backward compatibility with the existing `CalibData` / `calibration.json`. Write a minimal JSON parser/writer in C (no third-party deps), atomic save, checksum, and tests for round-trip, corruption, and migration."

### CAL-03a — 单位换算核心

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-02
- **Parent**: CAL-03
- **Files**: `src/core/calib/units.c/h`（或 `src/units.c/h` 先落地）、`tests/unit.c`
- **Goal**: 集中实现 dBFS/dBu/dBV/Vrms/Vpp，UI 不重复造轮子。
- **Acceptance**:
  - [ ] `dBu = 20log10(Vrms/0.775)`；`dBV = 20log10(Vrms/1.0)`。
  - [ ] 正弦 `Vpp = 2√2·Vrms`；非正弦要求显式 crest factor。
  - [ ] 换算误差 < 0.01%。
  - [ ] 未校准输入返回“不可换算”而不是猜测。
- **Tests**:
  - 已知点表：−20 dBFS @ 1 V FS → 0.1 Vrms → −17.78 dBu 等。
- **Prompt seed**:
  "Add a pure C units module with conversions between dBFS, dBu, dBV, Vrms, Vpp. Require an explicit crest factor for non-sine. Return an error when calibration is missing. Add table-driven unit tests with < 0.01% error."

---

## 4. B1 v1.2 安全

### SAFE-02 — SafetyGuard 状态机

- **Release**: v1.2 · **Size**: M · **Depends**: CH-07, CAL-02
- **Parent**: SAFE-02
- **Files**: `src/safety.h`, `src/safety.c`, `src/app.c`, `src/view.c`, `tests/unit.c`
- **Goal**: 用显式状态机约束采集流程：`IDLE → PRECHECK → ARMED → RUNNING → FAULT`。
- **Acceptance**:
  - [ ] 未通过预检不能进入 `RUNNING`。
  - [ ] `FAULT` 必须人工复位；不得自动恢复。
  - [ ] 状态变化写事件日志（依赖 RPT-01）。
  - [ ] UI 显示当前状态与原因。
  - [ ] 单测覆盖所有合法/非法迁移。
- **Tests**:
  - 状态迁移表；越级跳转；重复 reset；设备丢失。
- **Prompt seed**:
  "Implement a SafetyGuard state machine per ROADMAP §6.1.1 with IDLE/PRECHECK/ARMED/RUNNING/FAULT. Gate acquisition on precheck. Require explicit reset from FAULT. Expose state + reason to the UI. Add exhaustive transition tests."

### SAFE-03 — Clip 锁存 + 自动静音

- **Release**: v1.2 · **Size**: M · **Depends**: SAFE-02
- **Parent**: SAFE-03
- **Files**: `src/safety.c/h`, `src/app.c`, `src/view.c`, `tests/unit.c`
- **Goal**: 检测削波/平顶，锁存事件，100 ms 内静音输出，冻结采集。
- **Acceptance**:
  - [ ] `|x| ≥ 0.999` 连续 3 样本触发。
  - [ ] 平顶：`|x| > 0.9` 且差分近 0 连续 5 样本触发。
  - [ ] 记录峰值、通道、时间、样本索引。
  - [ ] 10 ms 淡出 + 静音；必须人工复位。
  - [ ] 事件写入 JSONL。
- **Tests**:
  - 合成削波/平顶/正常信号；边界样本；重复触发只记一次。
- **Prompt seed**:
  "Add clip latching and auto-mute per ROADMAP §6.1.3. Detect hard clip (3 samples ≥ 0.999) and flat-top (5 samples). Record peak/channel/time/index. Fade out over 10 ms, mute, freeze, and require manual reset. Add synthetic tests."

### SAFE-04 — 量程建议与超量程阻断

- **Release**: v1.2 · **Size**: S · **Depends**: SAFE-02, INP-01
- **Parent**: SAFE-04
- **Files**: `src/safety.c/h`, `src/view.c`, `tests/unit.c`
- **Goal**: 根据输入模式与预估电压给出量程，超出则禁止 Arm。
- **Acceptance**:
  - [ ] 量程表：<2 V → ±1 V；2–20 V → ±10 V；20–100 V → ±50 V；>100 V → 禁止。
  - [ ] 无前端时提示“最大输入未知”，不假装有保护。
  - [ ] 阻断必须给出理由与下一步。
- **Tests**:
  - 边界值 1.99/2.0/20/100；无前端；未知模式。
- **Prompt seed**:
  "Implement range recommendation and over-range blocking per ROADMAP §6.1.4. Use input mode + estimated Vpp. Block Arm when >100 V or when no front end and the estimate exceeds the sound card's unknown limit. Show a clear reason."

### SAFE-05 — 输出限幅、启动静音、淡入淡出

- **Release**: v1.2 · **Size**: S · **Depends**: CH-07
- **Parent**: SAFE-05
- **Files**: `src/audio.c`, `src/app.c`, `src/wavegen.c`, `tests/unit.c`
- **Goal**: 保护被测设备与耳朵。
- **Acceptance**:
  - [ ] 默认最大 −20 dBFS；提高需二次确认。
  - [ ] 启动 200 ms 静音；停止/切设备淡出。
  - [ ] 文件播放前峰值扫描；超限拒绝或衰减。
  - [ ] 输出 DC 偏移 > 10 mV 警告。
- **Tests**:
  - 0 dBFS 请求被限幅；启动无爆音；文件峰值扫描。
- **Prompt seed**:
  "Add output limiting, 200 ms startup mute, fade in/out, and file peak scanning per ROADMAP §6.1.5. Default max −20 dBFS. Warn on DC offset > 10 mV. Add tests for limiter and startup behavior."

### SAFE-06 — 首次使用安全引导

- **Release**: v1.2 · **Size**: S · **Depends**: SAFE-02
- **Parent**: SAFE-06
- **Files**: `src/view.c`, `src/main.m`, `docs/safety.md`
- **Goal**: 首次测量前确认“不接市电/不直连喇叭/phantom 已关”。
- **Acceptance**:
  - [ ] 首次启动显示 3 项确认；未确认不能 Arm。
  - [ ] 可在设置中再次查看；不重复骚扰。
  - [ ] 文案不承诺“保护声卡”。
- **Tests**:
  - 首次/再次启动；拒绝确认；重置引导。
- **Prompt seed**:
  "Add a first-run safety onboarding with three confirmations: no mains, no direct speaker output, phantom off. Block Arm until confirmed. Make it reviewable in settings. Use honest copy per AGENTS.md §10."

### SAFE-07 — 安全故障注入测试

- **Release**: v1.2 · **Size**: M · **Depends**: SAFE-02/03/04/05
- **Parent**: SAFE-07
- **Files**: `tests/selftest.c`, `tests/unit.c`, `src/safety.c`
- **Goal**: 证明安全逻辑在异常输入下可靠。
- **Acceptance**:
  - [ ] 注入 10 Vpp、DC、平顶、NaN/Inf、快速增益变化、设备拔出。
  - [ ] 全部被捕获/阻断；无崩溃；事件完整。
  - [ ] 测试可在 CI 无硬件运行。
- **Tests**:
  - 每个故障一个用例；断言状态/事件/静音。
- **Prompt seed**:
  "Add fault-injection tests for 10 Vpp, DC offset, flat-top, NaN/Inf, rapid gain changes, and device removal. Assert SafetyGuard state, event log entries, and auto-mute. Must run in CI without hardware."

---

## 5. B2 v1.2 校准

### CAL-01 — CoreAudio 设备能力探测

- **Release**: v1.2 · **Size**: M · **Depends**: none
- **Parent**: CAL-01
- **Files**: `src/audio.c/h`, `src/app.c`, `tests/unit.c`
- **Goal**: 枚举设备/通道/采样率/延迟/传输类型，作为校准与提示的基础。
- **Acceptance**:
  - [ ] 设备 UID、名称、传输类型、采样率列表、输入/输出通道数、流配置、延迟。
  - [ ] 3 款接口识别正确；热插拔不崩溃。
  - [ ] 不猜测电压；只提供能力。
- **Tests**:
  - `--devices` 输出；拔插；采样率切换。
- **Prompt seed**:
  "Expand CoreAudio device probing to expose UID, transport type, sample rates, channel counts, stream configuration, and latency. Do not infer voltage. Add a --devices JSON output and hotplug tests."

### CAL-03b — 校准向导（UI + 存储）

- **Release**: v1.2 · **Size**: M · **Depends**: CAL-02, CAL-03a, CAL-01
- **Parent**: CAL-03
- **Files**: `src/view.c`, `src/app.c`, `src/calib.c`, `tests/unit.c`
- **Goal**: 让用户完成一次可信的校准，并看到置信度。
- **Acceptance**:
  - [ ] 选择设备/通道/增益档；记录档位。
  - [ ] 三种方法：DMM / 参考 / loopback。
  - [ ] −40/−20/−6 dBFS 线性验证；偏差 > 0.2 dB 警告。
  - [ ] 保存 profile；显示 `measured/datasheet/assumed`。
  - [ ] 设备/采样率/增益变化提示失效。
- **Tests**:
  - 模拟三种方法；线性偏差；失效提示。
- **Prompt seed**:
  "Build the calibration wizard UI per ROADMAP §6.2.2: device/channel/gain selection, DMM/reference/loopback methods, linearity check at −40/−20/−6 dBFS, save with confidence, and invalidation on device/rate/gain change. Add tests for each path."

### CAL-04 — 逐通道/逐增益档校准与失效

- **Release**: v1.2 · **Size**: M · **Depends**: CAL-02/03b
- **Parent**: CAL-04
- **Files**: `src/calib.c/h`, `src/app.c`, `src/view.c`, `tests/unit.c`
- **Goal**: 不再用单一 `volts_at_fs` 代表所有通道/档位。
- **Acceptance**:
  - [ ] 每个 `device+channel+gain+pad+rate` 独立 profile。
  - [ ] 切换时自动匹配；无匹配则显示“未校准”。
  - [ ] 失效提示不可忽略。
- **Tests**:
  - 同设备不同增益；不同通道；无匹配。
- **Prompt seed**:
  "Replace the single volts_at_fs assumption with per-device/channel/gain calibration lookup. Match by device UID, channel, gain_db, pad_db, sample_rate. Show uncalibrated state; never silently reuse a mismatched profile."

### CAL-05 — 单位系统接入 UI

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-03a
- **Parent**: CAL-05
- **Files**: `src/view.c`, `src/app.c`, `src/ui.c`
- **Goal**: 所有读数同时显示 dBFS 与电压（带置信度）。
- **Acceptance**:
  - [ ] 单位切换 dBFS/dBu/dBV/Vrms/Vpp。
  - [ ] 未校准只显示 dBFS 或“—”。
  - [ ] 非正弦标注 crest factor 假设。
- **Tests**:
  - 快照/字符串断言；未校准状态。
- **Prompt seed**:
  "Wire the units module into all readouts. Show dBFS plus voltage with confidence. Hide voltage when uncalibrated. Label sine/crest assumptions. Add UI snapshot tests."

### CAL-06 — 输入阻抗与加载误差提示

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-02, INP-01
- **Parent**: CAL-06
- **Files**: `src/calib.c/h`, `src/view.c`, `docs/schema/calibration.md`
- **Goal**: 高阻源接低阻输入时，明确提示测量被加载。
- **Acceptance**:
  - [ ] 校准/设备 profile 可记录输入阻抗。
  - [ ] 已知源阻抗 + 输入阻抗时估算衰减；> 1 dB 警告。
  - [ ] 未知阻抗显示“未标定”，不假装准确。
- **Tests**:
  - 1 MΩ 源 / 10 kΩ 输入；1 kΩ 源；未知阻抗。
- **Prompt seed**:
  "Add input impedance to the calibration profile and warn when a high-impedance source is loaded by a low-impedance input. Estimate attenuation when both impedances are known; show 'uncalibrated' otherwise. Add tests."

### CAL-07 — 置信度标签与报告来源

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-02
- **Parent**: CAL-07
- **Files**: `src/view.c`, `src/report.c`, `src/calib.c`
- **Goal**: 每个电压数字都能追溯来源。
- **Acceptance**:
  - [ ] UI 绿/琥珀/砖红三态标签。
  - [ ] 报告含校准 ID、方法、置信度、时间。
  - [ ] 无来源不显示电压。
- **Tests**:
  - 三态渲染；报告字段。
- **Prompt seed**:
  "Add confidence badges (measured/datasheet/assumed) to voltage readouts and report provenance. Hide voltage when no calibration exists. Add tests for the three states."

### CAL-09 — 校准完整性校验

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-02
- **Parent**: CAL-09
- **Files**: `src/calib.c`, `tests/unit.c`
- **Goal**: 防止损坏/伪造/错配的校准被使用。
- **Acceptance**:
  - [ ] checksum 校验；设备 UID 绑定；时间戳。
  - [ ] 失败时拒绝使用并提示重新校准。
- **Tests**:
  - 篡改字段；错设备；损坏文件。
- **Prompt seed**:
  "Add checksum and device binding to calibration profiles. Reject mismatched or corrupted files with a clear message. Add tests for tampering and wrong-device use."

---

## 6. B3 v1.2 输入

### INP-01 — 输入模式预设

- **Release**: v1.2 · **Size**: S · **Depends**: CAL-03a
- **Parent**: INP-01
- **Files**: `src/app.h`, `src/app.c`, `src/view.c`
- **Goal**: Line Pro / Line Consumer / DI (balanced line) / Inst / Mic / Probe / Speaker-via-pad。
- **Acceptance**:
  - [ ] 每种模式：标称电平、典型最大、阻抗、安全上限、phantom 要求。
  - [ ] 切换需用户确认；显示影响。
  - [ ] 与 SafetyGuard 联动。
- **Tests**:
  - 模式表；切换确认；非法组合（Mic + phantom 未知）。
- **Prompt seed**:
  "Implement input mode presets with nominal level, max input, impedance, safety limit, and phantom requirements. Require confirmation on switch. Integrate with SafetyGuard. Add tests for each mode."

### INP-02 — 信号检查助手

- **Release**: v1.2 · **Size**: M · **Depends**: INP-01, CAL-03b
- **Parent**: INP-02
- **Files**: `src/analyze.c/h`, `src/view.c`, `src/app.c`, `tests/unit.c`
- **Goal**: 给出“建议模式 + 理由 + 置信度”，但不自动切换。
- **Acceptance**:
  - [ ] 采集 2 s，计算峰值/RMS/波峰因数/工频/频谱倾斜/噪声底。
  - [ ] 输出建议与理由。
  - [ ] 用户确认才切换；无自动路径。
- **Tests**:
  - 合成高阻源/线路源/静音/工频；断言建议与不切换。
- **Prompt seed**:
  "Add a signal-check assistant that captures 2 s and computes peak, RMS, crest factor, hum, spectral tilt, and noise floor. Recommend an input mode with reasoning and confidence. Never auto-switch. Add tests with synthetic high-Z and line sources."

### INP-04 — Phantom / TS-TRS / 接地环路检查

- **Release**: v1.2 · **Size**: S · **Depends**: INP-01
- **Parent**: INP-04
- **Files**: `src/view.c`, `docs/safety.md`
- **Goal**: 在 Mic/线路测量前给出接线检查清单。
- **Acceptance**:
  - [ ] phantom 未确认关闭时禁止 Mic 测量。
  - [ ] TS/TRS 不匹配警告。
  - [ ] 工频 hum 时提示接地环路排查。
- **Tests**:
  - 清单状态；阻断；文案。
- **Prompt seed**:
  "Add a wiring checklist for phantom, TS/TRS, and ground loops. Block Mic measurement until phantom-off is confirmed. Warn on 50/60 Hz hum. Add UI tests."

### INP-05 — “禁止静默自动切换”不变量测试

- **Release**: v1.2 · **Size**: S · **Depends**: INP-01/02
- **Parent**: INP-05
- **Files**: `tests/unit.c`, `src/app.c`
- **Goal**: 用测试锁死行为。
- **Acceptance**:
  - [ ] 扫描所有模式切换路径，只有用户确认路径可用。
  - [ ] 新增自动切换即测试失败。
- **Tests**:
  - 枚举事件/命令；断言需要确认 token。
- **Prompt seed**:
  "Add an invariant test that input mode can only change through an explicit user-confirmed action. Enumerate all state transitions and fail if any path changes mode without confirmation."

---

## 7. B4 v1.2 交互

### UI-01 — 光标锚定缩放

- **Release**: v1.2 · **Size**: M · **Depends**: none
- **Parent**: UI-01
- **Files**: `src/main.m`, `src/view.c`, `src/app.c`, `src/app.h`
- **Goal**: 滚轮缩放时，光标下的时间/电压点保持不动。
- **Acceptance**:
  - [ ] X/Y 轴独立命中；绘图区滚轮 X，Ctrl/Cmd 滚轮 Y，Alt 双轴。
  - [ ] 锚点漂移 < 1 px。
  - [ ] 1-2-5 刻度；连续缩放。
- **Tests**:
  - 数学单测；UI 手测；截图。
- **Prompt seed**:
  "Implement cursor-anchored zoom per ROADMAP §6.5. Add plot hit regions and axis-specific wheel handling. Keep the data point under the cursor fixed within 1 px. Use continuous zoom internally with 1-2-5 tick labels. Add unit tests for the anchor math."

### UI-02 — 平移、捏合、框选、复位

- **Release**: v1.2 · **Size**: M · **Depends**: UI-01
- **Parent**: UI-02
- **Files**: `src/main.m`, `src/view.c`, `src/app.c`
- **Goal**: 完整的触控板/鼠标导航。
- **Acceptance**:
  - [ ] 拖拽平移；Shift+拖拽框选；双击自适应；右键复位。
  - [ ] 捏合缩放；双指滚动平移。
  - [ ] 不误触控件。
- **Tests**:
  - 手测矩阵；截图。
- **Prompt seed**:
  "Add pan, pinch, rubber-band zoom, double-click autoscale, and right-click reset. Ensure gestures only apply inside plot regions. Add a manual test matrix and screenshots."

### UI-03 — CH1/CH2 独立量程

- **Release**: v1.2 · **Size**: M · **Depends**: UI-01
- **Parent**: UI-03
- **Files**: `src/app.h`, `src/app.c`, `src/view.c`
- **Goal**: 每通道独立 V/div、offset、单位，可链接。
- **Acceptance**:
  - [ ] 独立设置；链接开关；轴标签显示通道色。
  - [ ] 自动量程只作用于当前通道。
  - [ ] 设置持久化。
- **Tests**:
  - 两通道不同量程截图；持久化。
- **Prompt seed**:
  "Replace shared volt_div/v_off with per-channel v_div[i], v_offset[i], unit[i]. Add a link toggle. Autoscale only the active channel. Persist per-channel settings. Add screenshots and persistence tests."

### UI-04 — 双光标与 Δ 读数

- **Release**: v1.2 · **Size**: S · **Depends**: UI-01
- **Parent**: UI-04
- **Files**: `src/view.c`, `src/app.c`
- **Goal**: 测时间差/电压差/频率。
- **Acceptance**:
  - [ ] 两个光标可拖动；ΔT、ΔV、1/ΔT。
  - [ ] 吸附过零/峰值（可关）。
  - [ ] 读数进入报告截图。
- **Tests**:
  - 已知波形断言；截图。
- **Prompt seed**:
  "Add dual cursors with ΔT, ΔV, and 1/ΔT readouts. Support snapping to zero crossings/peaks with a toggle. Add tests with a known sine and a screenshot."

### UI-05 — 每页签视图状态持久化

- **Release**: v1.2 · **Size**: S · **Depends**: PLAT-06
- **Parent**: UI-05
- **Files**: `src/app.c`, `src/app.h`
- **Goal**: 重启后恢复缩放/通道/页签。
- **Acceptance**:
  - [ ] 每 tab 独立状态；版本化；损坏回退默认。
- **Tests**:
  - 保存/加载/损坏。
- **Prompt seed**:
  "Persist per-tab view state (timebase, per-channel scales, cursors, tab) in settings.json. Version the block and fall back to defaults on corruption."

### UI-06 — 键盘与可访问性

- **Release**: v1.2 · **Size**: S · **Depends**: UI-01/03
- **Parent**: UI-06
- **Files**: `src/main.m`, `src/ui.c`, `src/view.c`
- **Goal**: 全功能键盘可达，焦点可见。
- **Acceptance**:
  - [ ] Tab 顺序；焦点环；↑↓/←→ 调整；Esc 取消。
  - [ ] 对比度 AA；减少动态。
- **Tests**:
  - 键盘走查清单；截图。
- **Prompt seed**:
  "Add keyboard navigation and visible focus rings for all controls. Support arrow-key adjustments for timebase/vertical. Respect reduce-motion. Add an accessibility checklist and screenshots."

### UI-07 / LIM-01 / LIM-02 — 带宽/耦合/DC 边界提示

- **Release**: v1.2 · **Size**: S · **Depends**: none
- **Parent**: UI-07、LIM-01、LIM-02
- **Files**: `src/view.c`, `src/app.c`, `README.md`
- **Goal**: 修正“DC”标签误导，明示测量边界。
- **Acceptance**:
  - [ ] “DC”改为“DC（不平移）”或“Raw”，并提示硬件 AC 耦合。
  - [ ] 显示 Nyquist 频率与有效带宽。
  - [ ] 首次使用/帮助中写明“不能测 DC/超声”。
- **Tests**:
  - 文案/截图。
- **Prompt seed**:
  "Rename the misleading DC coupling option to 'Raw (no mean removal)' and add a note that the sound card input is AC-coupled. Show Nyquist and effective bandwidth. Update help/README. Add screenshots."

---

## 8. B5 v1.2 报告

### RPT-01 — 会话事件日志 JSONL

- **Release**: v1.2 · **Size**: M · **Depends**: PLAT-06
- **Parent**: RPT-01
- **Files**: `src/session.c/h`（或 `src/report.c` 扩展）、`src/app.c`, `tests/unit.c`
- **Goal**: 记录设备/采样率/xrun/clip/模式/校准/备注。
- **Acceptance**:
  - [ ] 事件类型齐全；时间戳带时区；追加写；崩溃安全。
  - [ ] 日志轮转；磁盘上限。
  - [ ] 报告可引用事件。
- **Tests**:
  - 事件序列；轮转；损坏行跳过。
- **Prompt seed**:
  "Add a JSONL session event log with timestamp, type, and data for device changes, xrate changes, xruns, clips, input mode changes, calibration changes, run start/end, notes, and errors. Append-only, crash-safe, rotate by size. Add tests."

### RPT-02 — 报告 schema v2 + provenance

- **Release**: v1.2 · **Size**: M · **Depends**: CAL-02, RPT-01
- **Parent**: RPT-02
- **Files**: `src/report.h`, `src/report.c`, `tests/unit.c`, `docs/schema/report-v2.md`
- **Goal**: 报告含完整来源，可追溯。
- **Acceptance**:
  - [ ] 字段：app/OS/设备 UID/校准 ID/置信度/采样率/增益/设置哈希/限值/判定/时间戳。
  - [ ] 旧 v1 可读；新写 v2。
  - [ ] 原子写入；schema 校验。
- **Tests**:
  - round-trip；v1 读取；缺字段；哈希。
- **Prompt seed**:
  "Introduce report schema v2 with provenance fields per ROADMAP §6.8.2. Keep reading v1 reports. Write atomically, validate schema, and add round-trip/migration tests."

### RPT-03 — HTML 报告 + PDF 导出

- **Release**: v1.2 · **Size**: M · **Depends**: RPT-02
- **Parent**: RPT-03
- **Files**: `src/report.c`, `src/main.m`, `tests/golden/`
- **Goal**: 离线可开、可打印/PDF。
- **Acceptance**:
  - [ ] 自包含 HTML（内嵌 CSS/PNG）；无外部资源。
  - [ ] 封面/设备/校准/结论/图表/事件/建议。
  - [ ] macOS 用 `CGPDFContext` 导出 PDF。
  - [ ] 无网络也可打开。
- **Tests**:
  - 黄金 HTML 片段；离线打开；PDF 生成。
- **Prompt seed**:
  "Generate a self-contained HTML report (inline CSS + base64 PNG) with cover, device/calibration, verdict, charts, events, and advice. Add macOS PDF export via CGPDFContext. No external resources. Add golden tests."

### RPT-06 — 限值与判定

- **Release**: v1.2 · **Size**: S · **Depends**: RPT-02
- **Parent**: RPT-06
- **Files**: `src/report.c/h`, `tests/unit.c`
- **Goal**: 可配置通过/失败，报告显示依据。
- **Acceptance**:
  - [ ] 默认限值；用户覆盖；保存/复用。
  - [ ] PASS/FAIL/WARN；每条显示实际值 vs 限值。
- **Tests**:
  - 边界值；覆盖；报告字段。
- **Prompt seed**:
  "Add configurable pass/fail limits with PASS/FAIL/WARN verdicts. Show actual vs limit for every criterion. Persist per-template limits and add boundary tests."

### RPT-07 — 重复性与不确定度

- **Release**: v1.2 · **Size**: S · **Depends**: RPT-02
- **Parent**: RPT-07
- **Files**: `src/loopback.c`, `src/report.c`, `tests/unit.c`
- **Goal**: 让用户知道“这个数字有多稳”。
- **Acceptance**:
  - [ ] 可选重复 3 次；均值/标准差/极差。
  - [ ] 报告显示测量系统残差（声卡自身 THD+N/噪声）。
- **Tests**:
  - 已知方差输入；报告字段。
- **Prompt seed**:
  "Add an optional 3-run repeatability mode with mean/std/range, and a measurement-system residual section (sound card THD+N/noise floor). Add tests with synthetic variance."

### PLAT-07 — 性能预算与回归

- **Release**: v1.2 · **Size**: S · **Depends**: CH-07
- **Parent**: PLAT-07
- **Files**: `Makefile`, `tests/`, `docs/perf.md`
- **Goal**: 防止 vibe coding 把实时性能写坏。
- **Acceptance**:
  - [ ] 启动计时；UI 帧率；回调分配检测；8ch CPU。
  - [ ] CI 记录趋势；超预算警告。
- **Tests**:
  - 基准命令；阈值断言。
- **Prompt seed**:
  "Add performance benchmarks for startup, UI frame time, audio callback allocations, and 8ch CPU. Record results in CI and fail on budget regressions. Document thresholds."

### PLAT-08 — 崩溃日志与日志轮转

- **Release**: v1.2 · **Size**: S · **Depends**: RPT-01
- **Parent**: PLAT-08
- **Files**: `src/main.m`, `src/session.c`, `docs/privacy.md`
- **Goal**: 现场问题可定位，默认不上传。
- **Acceptance**:
  - [ ] 崩溃日志本地；含版本/设备/最后事件。
  - [ ] 轮转/上限；隐私开关。
- **Tests**:
  - 模拟崩溃；日志内容；清理。
- **Prompt seed**:
  "Add local crash logging with app/OS/device/last events, rotation, and a privacy toggle. Never upload by default. Add tests for log content and cleanup."

---

## 9. B6 v1.3 多通道

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| CH-01a | ring 抽象为 N 通道（先 4ch） | M | CH-07 | 4ch 读写正确；回绕/溢出测试 |
| CH-01b | 8ch 引擎 + 压力测试 | M | CH-01a | 8ch/48k 10 min xrun=0；CPU <10% |
| CH-01c | 通道元数据/颜色/名称 | S | CH-01a | 4ch 标签正确；持久化 |
| CH-02 | 通道映射/非交错缓冲 | M | CH-01b | 4/8ch 顺序正确；聚合设备 |
| CH-03 | arm/solo/mute/meter | S | CH-01c | 任意通道独立启用；solo 不影响采集 |
| CH-04 | 任意通道触发/math/XY 对 | M | CH-02 | 任意两通道可 A+B/A−B/XY |
| CH-05 | 聚合设备识别/时钟源 | M | CH-02 | 识别 Aggregate；显示子设备 |
| CH-06 | 热插拔/采样率/设备丢失 | M | CH-02 | 拔插 20 次不崩溃；提示 |

---

## 10. B7 v1.3 相位

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| PH-01 | XY/Lissajous 视图 | M | CH-04 | 1:1/2:1 正确；余晖可调 |
| PH-02 | 椭圆拟合/相位/增益/极性 | M | PH-01 | 45°±2°；增益 ±0.1 dB |
| PH-03 | 通道匹配测试 | M | PH-02 | 同源分线；20 Hz–20 kHz 表 |
| PH-04 | 跨设备漂移测试 | M | PH-03 | 100 ppm±5 ppm；同设备禁用 |
| PH-05 | 相位斜率/延迟 | S | PH-03 | 固定相移 vs 延迟可区分 |
| PH-06 | 仿真/训练模式 | M | PH-02 | ≥6 场景；已知答案可复现 |

---

## 11. B8 v1.3 热力图

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| SPEC-01 | Spectrogram dB 矩阵 | M | CH-01a | 1 kHz±1 bin；60 s 历史 |
| SPEC-02 | 余晖/衰减/max hold | S | SPEC-01 | 可调；冻结可读 |
| SPEC-03 | 光标/缩放/平移 | M | SPEC-01 | 读数与 FFT 一致 |
| SPEC-04 | 色标/gamma/色盲友好 | S | SPEC-01 | 单调亮度；3 套配色 |
| SPEC-05 | PNG/CSV 导出 | S | SPEC-01 | 含元数据；报告可嵌入 |

---

## 12. B9 v1.4 维修工作流

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| REP-01 | 模板引擎/步骤执行器 | M | RPT-02 | 顺序/分支/重试；失败定位 |
| REP-02 | 吉他效果器模板 | M | REP-01, PH-03 | bypass/engage；THD/噪声/削波 |
| REP-03 | 磁带机模板 | M | REP-01, PH-07/08 | 带速/wow/方位角/频响/串扰 |
| REP-04 | 调音台通道模板 | M | REP-01 | 增益/频响/THD/串扰/phantom |
| REP-05 | 耳放模板 | M | REP-01, REP-08 | 负载频响/THD/输出阻抗 |
| REP-06 | 合成器/线路输出模板 | S | REP-01 | 噪声/频响/削波/串扰 |
| REP-07 | 串扰/分离度 | M | CH-02 | 20 Hz–20 kHz；−dB；报告 |
| REP-08 | 阻抗测量 | M | CAL-03b | 20 Hz–20 kHz；误差 <10% |
| REP-09 | IMD + THD vs 电平 | M | CAL-03b | SMPTE/DIN；与参考一致 |
| REP-10 | 老化/长时测试 | M | RPT-01 | 8 h 内存有界；事件触发 |
| PH-07 | Wow & flutter | M | PH-03 | 3 kHz；IEC/DIN；重复性 <5% |
| PH-08 | 方位角 | S | PH-03 | 10 kHz 相位差可读 |
| SPEC-06 | THD vs 频率 vs 电平热图 | M | SPEC-01 | 定位失真起点；CSV |
| SPEC-07 | 长时监控热图 | M | SPEC-01 | 8 h 有界；事件标记 |
| RPT-04 | 前后对比 | M | RPT-02 | 差值表/曲线叠加/结论 |
| RPT-05 | 模板文档/示例 | S | REP-01 | 每个模板有步骤/判据/示例 |
| RPT-08 | 案例文件夹/附件 | S | RPT-01 | 每次维修一个文件夹 |
| INP-03 | 双阻抗源测试 | M | FE-0.2 | 误差 <20%；准确率 >90% |

---

## 13. B10 v2.0 规模化

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| PLAT-01 | 通用二进制 | S | PLAT-02 | arm64+x86_64 可启动 |
| PLAT-03 | 签名/公证 + Windows 安装包 | M | PLAT-02 | Gatekeeper/SmartScreen 无警告或文档说明 |
| PLAT-05 | 中英文 i18n | M | 无 | 所有 UI/报告可切换 |
| CAL-08 | 设备库导入/导出/贡献 | M | CAL-02 | ≥30 条；来源/日期/置信度 |

---

## 14. H 硬件前端

> **SAFE-01 的落地方式**：本节的 FE-0.1 / FE-0.2 / FE-1.0 就是 SAFE-01 的硬件任务切片；软件侧在无前端时只显示“最大输入未知”，不得假装有保护。

| ID | 任务 | Size | Depends | 验收摘要 |
|---|---|---|---|---|
| SAFE-01 | 前端参考设计交付（原理图/BOM/装配/测试报告） | L | 无 | 与 FE-0.1/0.2/1.0 对应；文档可复现 |
| FE-0.1 | 面包板验证 | M | 无 | ±50 V 不损坏；20 Hz–20 kHz ±0.1 dB |
| FE-0.2 | PCB + 外壳 + BOM | L | FE-0.1 | 2 台样机；噪声/THD 达标 |
| FE-1.0 | 小批量套件 + 文档 | L | FE-0.2 | 3 款声卡验证；装配说明可复现 |

**FE 关键指标**：1 MΩ // 20 pF；±1/±10/±50 V；THD+N < −100 dB（1 kHz，2 Vrms，20 kHz BW）；噪声 < 10 µVrms；TVS + PTC + 限流 + 继电器衰减 + phantom blocker；600 Ω 输出。

---

## 15. 建议执行顺序（关键路径）

```
LANG-01 → PLAT-09/10 → PLAT-04a/b → CH-07 → ALPHA-01
  ├─ ALPHA-02 → ALPHA-04 → ALPHA-05
  ├─ ALPHA-03
  └─ ALPHA-06 → ALPHA-07 → ALPHA-08 → ALPHA-09
                    └─ 0.1-alpha 发布
CAL-02 → CAL-03a → CAL-03b → CAL-04
  ├─ SAFE-02 → SAFE-03 → SAFE-04 → SAFE-05 → SAFE-07
  ├─ INP-01 → INP-02 → INP-05
  ├─ UI-01 → UI-02 → UI-03 → UI-04
  └─ RPT-01 → RPT-02 → RPT-03 → RPT-06/07
                └─ v1.2 发布
CH-01a → CH-01b → CH-02 → CH-04 → PH-01 → PH-02 → PH-03 → PH-04
                                      └─ SPEC-01 → SPEC-02/03/04/05
                                                    └─ v1.3 发布
REP-01 → REP-02/07/08/09 → PH-07/08 → RPT-04/05
                                      └─ v1.4 发布
PLAT-01/03/05 + CAL-08 → v2.0
```

**并行建议**

- Windows 平台层（WASAPI/D2D）与 macOS 适配并行；这是 alpha 的关键路径。
- 硬件 FE 与软件 v1.2 并行；软件在无前端时显示“最大输入未知”。
- 文档/示例报告与 alpha 并行；alpha 必须有 KNOWN-ISSUES。
- 0.1-alpha 发布后，先收集 Windows 试用反馈，再进入 v1.2。

---

## 16. 任务领取规则

1. 先做 `Depends` 已完成的 P0。
2. 不并行领取会改同一核心结构的任务（如 CH-01b 与 PH-01）。
3. 每个任务必须有验收标准与测试；没有就退回补充。
4. 任务超过 400 行或 1 天，拆成 `a/b/c`。
5. 发现需求冲突，先改 ROADMAP/本文件，再写代码。
6. 完成后更新状态、CHANGELOG、相关文档；在 PR 中引用任务 ID。

---

## 17. 完成定义（单任务）

- [ ] 代码/测试/文档三件套齐。
- [ ] `ctest`/`make test` 双端绿；ASan/UBSan 干净；UI 双端截图。
- [ ] 验收标准逐条勾选。
- [ ] 无新增依赖/网络/隐私风险。
- [ ] C ABI 无 STL/异常穿透；音频回调无分配/锁/STL。
- [ ] PR ≤ 400 行；含双端测试证据、风险与回滚。
- [ ] 更新本 backlog 状态（如仓库内维护）。
