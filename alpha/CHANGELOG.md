# AudioScope by BerryStudio — Changelog

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
