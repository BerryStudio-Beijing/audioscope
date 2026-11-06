# AudioScope by BerryStudio — 0.3-alpha Known Issues

> 版本：`0.3.0-alpha.1` · 日期：2026-11-07
> 这是 **Multi-channel & Phase 核心验证版**，不能替代完整维修工作台。

## 阻断级限制

1. **没有硬件保护前端。** 软件只能预警、锁存、静音，不能阻止过压损坏声卡。不要接市电、喇叭输出、未衰减胆机输出。
2. **没有原生 GUI。** 当前是 ASCII TUI；CoreGraphics/Direct2D 仍未实现。
3. **Windows 仍是 WinMM 2ch。** 不是 WASAPI，也不支持 >2ch；延迟较高。
4. **真实漂移测试未接线。** `drift --live` 只能说明同设备共享时钟；跨设备/聚合设备采集待后续版本。
5. **Lissajous 仍是点阵。** 没有余晖、椭圆拟合、自动频率比识别。
6. **单点校准。** 不是扫频/逐频点校准；电压精度依赖用户输入的 FS 值。

## 平台限制

- macOS：CoreAudio 输入最多取 8 路；设备原生通道数可能更多。
- macOS：首次 `monitor`/`measure`/`xy`/`spectrogram` 需要麦克风权限。
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

- 报告 JSON/HTML 支持最多 8 通道；没有 PDF、图表、前后对比。
- 校准 profile 是单点；没有设备库/逐增益档向导 UI。
- 没有 WAV 录制/回放；`scope`/`fft`/`xy`/`spectrogram` 可用合成信号。
- 没有自动化“同一 WAV 双端一致性”测试。

## 构建限制

- 本机没有 CMake；macOS `make mac` 与 Windows `make win` 已实际验证，CMake 由 CI 验证。
- Windows 交叉编译依赖 Homebrew `mingw-w64`；本机未运行 Wine，无法执行 `.exe`。
- macOS 最低 12.0；Windows 最低 10。

## 下一版必须解决

1. 原生 Canvas（CoreGraphics + Direct2D）+ 缩放/双光标。
2. WASAPI 共享模式 + Windows >2ch。
3. 真实跨设备漂移/聚合设备。
4. Lissajous 余晖/椭圆拟合/频率比。
5. 硬件前端参考设计。
6. Windows 真机运行测试。
