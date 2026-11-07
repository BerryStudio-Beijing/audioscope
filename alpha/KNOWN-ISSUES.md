# AudioScope by BerryStudio — 0.8-alpha Known Issues

> 版本：`0.8.0-alpha.1` · 日期：2026-11-07
> 这是 **Live Bridge 预览版**，不能替代完整维修工作台。

## 阻断级限制

1. **没有硬件保护前端。** 软件只能预警、锁存、静音，不能阻止过压损坏声卡。不要接市电、喇叭输出、未衰减胆机输出。
2. **实时桥默认是合成数据。** `ui --live` 推送的是 `live · synthetic` 快照；`--capture` 才打开
   真实输入，且没有实时 DSP/多设备聚合。没有原生 CoreGraphics/Direct2D 界面。
3. **Windows 仍是 WinMM 2ch。** 不是 WASAPI，也不支持 >2ch；延迟较高。
4. **真实跨设备漂移未接线。** `drift --live` 只说明同设备共享时钟；跨设备/聚合设备采集待后续版本。
5. **wow/flutter 是零交叉瞬时频率法。** 需要干净 3 kHz 测试音/测试带；RMS 与峰值差 √2。
6. **阻抗是手动串联电阻法。** 需要用户提供 R、Vsource、Vdut。
7. **THD/IMD 是单音/双音 FFT。** 没有 THD vs 频率、没有完整 IMD 标准套件。
8. **sweep / freqsweep 目前用合成信号。** 真实设备需要输出→DUT→输入闭环。
9. **维修计划目前只跑合成验证。** `repair --live` 明确未接线。
10. **PDF 是单页摘要。** 无图表、多页、曲线叠加。
11. **静态 UI 仍依赖本地 HTTP。** `file://` 打开会受字体/脚本限制；用 `ui --live`（自带服务）
    或 `python3 -m http.server`。
12. **Live 是控制级刷新，不是采样级流。** 每 500 ms 推送一次聚合快照（约 40 KB），
    足够看波形/频谱移动，但不是音频流；不设置 `--interval` 时不要期待毫秒级更新。

## 平台限制

- macOS：CoreAudio 输入最多取 8 路；设备原生通道数可能更多。
- macOS：首次 `monitor`/`measure`/`xy`/`spectrogram`/`wow` 需要麦克风权限。
- Windows：交叉编译 PE32+ x64；**未在真实 Windows 上运行验证**。
- Windows：静态链接，约 15 MB；未签名，SmartScreen 可能警告。
- 两端：`devices` 索引与 `as_audio_open` 的设备选择尚未一一映射；目前只用默认设备。
- 两端：未做签名/公证/安装包。

## 实时/架构限制

- macOS CoreAudio 输入回调使用固定栈缓冲（`AS_MAX_BLOCK × 8ch`，约 128 KB）；设备帧数超过 4096 时截断。
- Windows WinMM 回调中调用 `waveOutWrite` / `waveInAddBuffer`；关闭时并发行为未完全验证。
- 采集使用 ring 的“最新帧快照”，不是严格 FIFO；`measure` 的帧可能有少量重叠。
- 没有 TSan/ASan 自动化 CI；当前以 selftest + 双端编译/CTest 为主。

## 数据/报告限制

- 报告 JSON/HTML 支持最多 8 通道；没有 PDF、图表、前后对比（`compare` 只读 JSON 数值）。
- 校准 profile 是单点；没有设备库/逐增益档向导 UI。
- 没有 WAV 录制/回放；`scope`/`fft`/`xy`/`spectrogram`/`wow` 可用合成信号。
- 没有自动化“同一 WAV 双端一致性”测试。

## 构建限制

- 本机没有 CMake；macOS `make mac` 与 Windows `make win` 已实际验证，CMake 由 CI 验证。
- Windows 交叉编译依赖 Homebrew `mingw-w64`；本机未运行 Wine，无法执行 `.exe`。
- macOS 最低 12.0；Windows 最低 10。

## 下一版必须解决

1. 实时桥接真实输入（权限、设备选择、标定）并支持多客户端背压。
2. 原生 Canvas（CoreGraphics + Direct2D）。
3. WASAPI 共享模式 + Windows >2ch。
4. 真实跨设备漂移/聚合设备。
5. 真实 DUT 闭环、多页 PDF/曲线叠加。
6. 硬件前端参考设计。
7. Windows 真机运行测试（含 live bridge）。
