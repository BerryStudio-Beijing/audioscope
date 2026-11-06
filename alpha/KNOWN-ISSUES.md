# AudioScope by BerryStudio — 0.2-alpha Known Issues

> 版本：`0.2.0-alpha.1` · 日期：2026-11-07
> 这是 **Safe & Calibrated 核心验证版**，不能替代完整维修工作台。

## 阻断级限制

1. **没有硬件保护前端。** 软件只能预警、锁存、静音，不能阻止过压损坏声卡。不要接市电、喇叭输出、未衰减胆机输出。
2. **没有原生 GUI。** 当前是 ASCII TUI；CoreGraphics/Direct2D 仍未实现。
3. **Windows 使用 WinMM，不是 WASAPI。** 延迟较高；WASAPI 共享模式仍是后续目标。
4. **单通道测量。** `measure` 一次测一个通道；双通道同时测量待 0.3-alpha。
5. **单点校准。** 不是扫频/逐频点校准；电压精度依赖用户输入的 FS 值。
6. **没有缩放/平移/双光标。** 路线图 UI-01…06 尚未实现。

## 平台限制

- macOS：首次 `monitor`/`measure` 需要麦克风权限；拒绝后 `check` 可退回 `--synthetic`。
- macOS：默认输入/输出设备由系统决定；`devices` 列出索引，但 `monitor`/`measure` 目前只用默认设备。
- Windows：交叉编译产物为 PE32+ x64，已在 macOS 上编译通过；**尚未在真实 Windows 上运行验证**。
- Windows：静态链接，体积约 15 MB；未签名，SmartScreen 可能警告。
- 两端：未做签名/公证/安装包。
- 两端：`devices` 索引与 `as_audio_open` 的设备选择尚未一一映射。

## 实时/架构限制

- macOS CoreAudio 回调使用固定栈缓冲（约 32 KB）；设备帧数超过 4096 时会被截断。
- Windows WinMM 回调中调用 `waveOutWrite` / `waveInAddBuffer`；关闭时并发行为未完全验证。
- 采集使用 ring 的“最新帧快照”，不是严格 FIFO；`measure` 的帧可能有少量重叠。
- 没有 TSan/ASan 自动化 CI；当前以 selftest + 双端编译为主。

## 数据/报告限制

- 报告 JSON schema v2 与 HTML 已实现；没有 PDF、图表、前后对比。
- 校准 profile 是单点；没有设备库/逐增益档向导 UI。
- 没有 WAV 录制/回放；`scope`/`fft` 用合成信号。
- 没有自动化“同一 WAV 双端一致性”测试；该测试在后续版本。
- 报告中的电压只在有校准 profile 时显示；否则只显示 dBFS。

## 构建限制

- 本机没有 CMake；macOS `make mac` 与 Windows `make win` 已实际验证，CMake 由 CI 验证。
- Windows 交叉编译依赖 Homebrew `mingw-w64`；本机未运行 Wine，无法执行 `.exe`。
- macOS 最低 12.0；Windows 最低 10。

## 下一版必须解决

1. 原生 Canvas（CoreGraphics + Direct2D）+ 缩放/双光标。
2. WASAPI 共享模式后端。
3. 双通道同时测量与通道选择。
4. 逐增益档校准向导 + 输入阻抗/加载修正。
5. 硬件前端参考设计。
6. Windows 真机运行测试。
