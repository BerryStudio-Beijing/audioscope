# AudioScope 0.1-alpha — Known Issues

> 版本：`0.1.0-alpha.1` · 日期：2026-11-07
> 这是**架构验证版**，不能用于真实维修决策。

## 阻断级限制

1. **没有硬件保护前端。** 软件只能预警、锁存、静音，不能阻止过压损坏声卡。不要接市电、喇叭输出、未衰减胆机输出。
2. **没有原生 GUI。** alpha 是 ASCII TUI；CoreGraphics/Direct2D 是 v1.2 的目标。
3. **Windows 使用 WinMM，不是 WASAPI。** 延迟较高；WASAPI 共享模式是 v1.2 的目标。
4. **单点校准，不是逐增益档校准。** 电压读数只用于 alpha 验证。
5. **没有多通道/XY/漂移/热力图/模板/PDF。** 这些在 v1.3/v1.4。

## 平台限制

- macOS：首次 `monitor` 需要麦克风权限；拒绝后只能看合成波形。
- macOS：默认输入/输出设备由系统决定；`devices` 列出索引，但 `monitor`/`tone` 目前只用默认设备。
- Windows：交叉编译产物为 PE32+ x64，已在 macOS 上编译通过；**尚未在真实 Windows 上运行验证**。
- Windows：静态链接，体积约 15 MB；未签名，SmartScreen 可能警告。
- 两端：未做签名/公证/安装包；alpha 直接运行二进制。
- 两端：`devices` 的索引与 `as_audio_open` 的设备选择尚未一一映射；alpha 只用默认设备。

## 实时/架构限制

- macOS CoreAudio 回调中仍使用固定栈缓冲（约 32 KB）；设备帧数超过 4096 时会被截断。
- Windows WinMM 回调中调用 `waveOutWrite` / `waveInAddBuffer`；关闭时的并发行为未完全验证。
- 音频回调未使用 STL/异常/锁，但 Windows 关闭路径可能触发回调竞态。
- 没有 TSan/ASan 的自动化 CI；alpha 只做功能自检。

## 数据/报告限制

- 会话日志是 JSONL；没有 HTML/PDF 报告。
- 校准 JSON 是固定 schema；没有设备库/逐档向导。
- 没有 WAV 录制/回放；`scope`/`fft` 用合成信号。
- 没有自动化“同一 WAV 双端一致性”测试；该测试在 v1.2。

## 构建限制

- 本机没有 CMake；macOS `make mac` 与 Windows `make win` 已实际验证，CMake 由 CI 验证。
- Windows 交叉编译依赖 Homebrew `mingw-w64`；本机未运行 Wine，无法执行 `.exe`。
- macOS 最低 12.0；Windows 最低 10。

## 下一版必须解决

1. 原生 Canvas（CoreGraphics + Direct2D）。
2. WASAPI 共享模式后端。
3. 设备选择与通道映射。
4. 逐增益档校准 + 置信度 UI。
5. 输入模式/信号检查助手。
6. 事件日志 + HTML 报告。
7. Windows 真机运行测试。
