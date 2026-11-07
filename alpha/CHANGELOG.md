# AudioScope by BerryStudio — Changelog

## 1.0.0 — 2026-11-07

首个正式版。定位仍是“声卡音频维修工作台”，本版把 0.9.3 新增的发生器/I/O
与一组“结果可信度”修正一起交付。

### Added

- **9 种信号发生器**（测量页 GENERATOR）：正弦、方波、三角、锯齿、白噪、
  粉噪、对数扫频、单位脉冲、阶跃；PolyBLEP、固定种子噪声、200 ms 启动淡入。
- **输入/输出设备选择**：波形页 IN / OUT 循环，采集、发生器与 loopback
  都使用选中设备。
- **热插拔检测**：每 2 s 枚举；选中输入消失时停采并提示，不静默切换。
- **FFT 尺寸**：频谱页 1024 / 2048 / 4096 / 8192 / 16384。
- 安装包与 `.app` 的版本、README、release notes 统一到 `1.0.0`。

### Fixed

- **CH2 独立分析**：不再用“CH1 − 0.9 dB”推算 CH2；两通道各自计算
  peak / RMS / 频率 / Vpp / 削波，报告直接使用每通道统计。
- **实际采样率贯通**：统计、FFT bin 映射、THD、相位、时间轴与 loopback
  使用设备实际采样率，不再硬编码 48 kHz。
- **模拟结果不再冒充实测**：Tape Plan 明确标记 `SIMULATED`，屏幕显示
  `DEMO PASS/FAIL`；导出的 JSON/HTML/PDF 带模拟备注并降为 WARN。
- **Loopback 更名**：结果只称 `connectivity OK/FAIL`，不暗示 DUT 全面合格。
- **Windows WinMM 设备 ID**：按 `in%u` / `out%u` 解析真实设备编号，而不是
  把列表下标直接传给 waveIn/waveOut。

### Known limits

- Windows 仍为 WinMM 2ch；WASAPI / 独占 / >2ch 未实现。
- 设备选择不带采样率切换；macOS 最低 12.0，Windows 最低 10。
- 未校准只显示 dBFS；校准向导、游标/缩放/平移、前后对比未完成。
- Tape Plan 是固定夹具演示，不是 DUT 测量。
- 二进制未签名/未公证；Windows 真机音频未在本机构建环境验证。

## 0.9.3-beta.1 — 2026-11-07

### Added

- **9 种信号发生器**（测量页 GENERATOR）：正弦、方波、三角、锯齿、白噪、
  粉噪、对数扫频、单位脉冲、阶跃。方波/锯齿用 PolyBLEP 抑制混叠；噪声用固定
  种子 xorshift，同参数逐位可复现。
  - 新增 `AsWaveform` / `AsGenConfig` / `as_gen_render()` / `as_wave_*()`；
    旧的 `as_generator_sine()` / `as_generator_square()` 保留为薄封装。
  - 输出电平预设 `−60 / −40 / −30 / −20 dBFS`，**默认钳在 −20 dBFS**；
    `gen_hot_allowed` 是预留的显式解锁位，UI 不提供解锁入口。
  - 输出启动按 200 ms 线性淡入（逐样本，CoreAudio/WinMM 回调内）。
  - CoreAudio / WinMM 回调读取原子配置后调用 `as_gen_render()`；音频线程
    不分配、不加锁。
- **设备选择**：波形页 IN / OUT 循环选择，采集与发生器都使用选中设备；
  loopback 也改为选中输出→输入，不再固定系统默认。
- 修正 Windows 端把“列表下标”当 WinMM 设备 ID 的旧问题：现在按 `in%u` /
  `out%u` 解析真实设备编号。
- **热插拔**：每 2 s 重新枚举设备；选中输入消失时停止采集并显示
  `input device lost — capture stopped`，**不会静默切换到其它设备**。
- **FFT 尺寸**：频谱页 `1024 / 2048 / 4096 / 8192 / 16384`；历史缓冲扩到
  16384，频谱 / THD / 相位共用尾部 N 点窗，切换后立即刷新。
- 自检新增 11 项（各波形 RMS/峰值、噪声确定性、脉冲计数、阶跃形状、扫频
  走向），selftest 52 → **63 项**。

### Changed

- `NativeModel::wave`（CH1 波形缓冲）与新增的波形类型字段分开命名
  （`gen_wave`），避免同一结构里重名。

### Known limits

- Windows 仍是 WinMM 2ch；本机无 mingw/MSVC，Windows 构建与 UI 由 CI 验证。
- 设备选择不带采样率切换（仍 48 kHz），没有 WASAPI / 独占模式。
- 热插拔只处理“选中设备消失”，不自动恢复采集；需要用户重新确认。
- 发生器是软件侧输出，不能保护声卡；未知电平请用受保护前端。

## 0.9.2-beta.1 — 2026-11-07

### Fixed (Windows parity)

- **真减号**：Windows 的 `fmt` / `fmt_signed` 输出 U+2212，不再出现 ASCII 连字符。
- **示波器轴刻度**：Y 轴 `+1.0 / 0 / −1.0`、X 轴 `0 / 7.5 / 15 ms`；绘图区左侧
  留 34 px 安全区，刻度不压曲线。
- **频谱轴刻度**：Y 轴 `0 … −100 dB`（右侧对齐）、X 轴 `20 Hz / 1 kHz / 20 kHz`
  三点定位，替换原来用空格拼出来的一行字符串。
- **报告页空状态**：56 px 图标块 + "No reports yet / 如何导出 / 报告不外传" 三行。
- **示波器页常驻安全声明**：左下角分区分隔线 + 三行文案，与 macOS 一致。
- **报告表列宽**：由绝对偏移（330 / 450）改为按卡片宽度的 50% / 78% 定位。
- **页眉 / 页脚排印**：品牌行 20 px、版本行改等宽 Mono、状态与设备右对齐到与
  macOS 相同的 272 px 边距；页眉底部加发丝线；页脚三段统一等宽 Mono 与行高。

### Corrected

- 更正 0.9.1 记录里的说法：Windows 的测量/维修/报告表**本来就是列坐标**实现，
  不是拼空格；0.9.2 补齐的是轴刻度、空状态、真减号与页眉页脚排印。

### Known limits

- GDI+ 的 `DrawString` 不支持 letter-spacing，**Windows 不做逐字字距**（macOS 的
  0.02–0.06em 只在该端生效）。见 `KNOWN-ISSUES.md`。
- 仍是 beta：WASAPI、设备选择、游标/缩放、校准向导未做；Windows 未在真机音频
  上验证，二进制未签名。

## 0.9.1-beta.1 — 2026-11-07

### Fixed

- **导航指示线动画**：改为时间驱动（`MOTION_FAST` = 140 ms，ease-out），从
  当前显示位置出发；首帧与 reduce-motion 直接落位。0.9 用的是"每帧靠近
  32%"叠加 30 fps 定时器，换页时会从上一页缓慢滑过（看起来永远从左边飞
  过来），且帧率一变动画时长就变。新增 `tests/test_nav.c` 锁住行为。
- **窗口等比自适应**：缩放/居中几何统一在 `update_metrics` 里计算，绘制与
  鼠标命中测试共用同一份；视图随窗口 resize（`autoresizingMask` +
  `setFrameSize:`），resize 时立即重绘。letterbox 外边改为主题底色。
- **浅色模式示波器不再是浅色**：波形/频谱/XY 的测量面固定为深石墨
  （`chart_bg`），曲线、网格、刻度的对比关系在两个主题里一致。
- **数字符号**：所有读数使用真减号 `−`（U+2212），不再出现 ASCII 连字符。
- **表格对齐**：测量/维修/报告三张表改为固定列坐标，比例字体表头与等宽
  数值共用基线，不再靠拼空格对齐。

### Changed

- **刷新率 30 → 60 Hz**；引擎 `tick` 使用真实 `dt`；FFT / THD / 相位降到
  20 Hz（60 fps 下每 3 帧一次），波形与读数仍每帧更新。
- **排印**：新增 Brand 角色（17 / Geist Medium）；按钮改用 Label 角色；
  ASCII 文本按角色加 0.02–0.06em 字距，CJK 不加宽；同一行的 Label 与
  Value 共用基线；基线取整避免半像素发虚。
- **图表性能**：波形（720 点）、频谱（220 点）、XY（512 点）改为一条
  `CGPath` 一次描边，替代逐段 stroke；颜色、CTLine、凸起渐变全部缓存。
- **轴刻度**：示波器 `+1.0 / 0 / −1.0` 与 `0 / 7.5 / 15 ms`；频谱
  `0 … −100 dB` 与 `20 Hz / 1 kHz / 20 kHz`，刻度放在图表安全区外。
- **空状态与常驻声明**：报告页加入三行空状态；示波器页左下常驻安全声明。
- **Windows**：测量面同样固定深石墨；导航指示线使用同一套时间缓动；
  定时器 33 → 16 ms；XY 由 512 个独立椭圆改为单条折线。
- `make dist` 的产物文件名改用 `VERSION`，不再写死 0.8。

### Tests

- 新增 `tests/test_nav.c`（12 项断言），接入 `make test` 与 CTest：
  验证"从当前位置出发 / 半程在中间 / 时间到精确落位 / 低帧率不变时长 /
  reduce-motion 直接落位"。
- `make test`：52 项 selftest + scope/fft + ringtest + navtest + 七页
  `--shot` 全绿。

### Known limits

- 仍是 **beta 预览**：没有 WASAPI、设备选择、游标/缩放、校准向导，
  Windows 未在真机音频上验证，二进制未签名。
- Windows 的轴刻度、空状态与真减号在 0.9.2 才补齐；0.9.1 只同步了测量面、
  导航缓动与刷新率。

## 0.9.0-alpha.1 — 2026-11-07

### Added

- **Native macOS application** (`AudioScope.app`): AppKit window, CoreGraphics
  canvas, CoreText, Geist Regular/Medium/Bold and Geist Mono bundled from
  `assets/fonts`. Seven pages: scope, spectrum, XY, measure, repair, reports,
  calibration. Keyboard `1–7`, space to hold, `C` capture, `R` run plan,
  `E` export, `T` theme.
- **Native Windows application** (`AudioScope.exe`): Win32 window + GDI+
  rendering, same seven pages and design tokens, plus an Inno Setup
  **installer** (`AudioScope-0.9.0-alpha.1-windows-x64-setup.exe`).
- **Multi-page PDF**: report page 1 keeps the summary; page 2 adds the
  waveform chart; page 3 adds the spectrum chart (`as_pdf_new_page`,
  `as_pdf_polyline`, `as_report_write_pdf_ex`).
- **Loopback measurement** in the native app: −20 dBFS 1 kHz tone on the
  default output, capture from the default input, level/THD verdict. Requires
  explicit confirmation.
- `AudioScope --shot DIR` renders all seven pages offscreen at 2× for visual
  regression and CI.

### Changed

- Version 0.9.0-alpha.1.
- **Web UI removed**: `alpha/web/`, the live HTTP/WebSocket bridge, the
  `AUDIOSCOPE_DATA` injection and `ui --live` are gone. All rendering is now
  local AppKit/CoreGraphics on macOS and Win32/GDI+ on Windows.
- Geist fonts moved to `alpha/assets/fonts/` and are embedded in the `.app`.
- Capture remains opt-in and visible; synthetic data is the default.

### Known limits

- Windows audio is still WinMM 2ch; WASAPI shared mode and >2ch are not in this
  alpha.
- Windows binaries are unsigned (SmartScreen) and were compile/tested on CI,
  not on a Windows audio machine.
- Loopback needs a physical or virtual output→input route; it is not a
  calibrated DUT measurement.
- Voltage stays hidden until a measured calibration profile exists.

## 0.8.0-alpha.1 — 2026-11-07

See tag `v0.8.0-alpha.1`.
