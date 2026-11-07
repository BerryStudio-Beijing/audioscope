# AudioScope by BerryStudio — 0.9.2-beta (Native)

> **AudioScope by BerryStudio** · `0.9.2-beta.1` · macOS 12+ arm64 · Windows 10/11 x64
> C11 core + C++17 app · native AppKit/CoreGraphics + Win32/GDI+ · no Web UI

## 0.9.2-beta 是什么

0.9.2 把 0.9.1 在 macOS 上做的排版修正补到 Windows，功能范围不变：

- **真减号**：Windows 读数使用 `−`（U+2212），不再出现 ASCII 连字符。
- **轴刻度**：示波器 `+1.0 / 0 / −1.0` 与 `0 / 7.5 / 15 ms`；频谱
  `0 … −100 dB` 与 `20 Hz / 1 kHz / 20 kHz`，刻在图表安全区外。
- **空状态与常驻声明**：报告页三行空状态；示波器页左下安全声明。
- **表格与页眉页脚**：报告表列宽按卡片比例；页眉品牌/版本/状态分三行对齐，
  页脚三段统一等宽 Mono。

> 限制：GDI+ 的 `DrawString` 不支持 letter-spacing，逐字字距只在 macOS 生效，
> 已记录在 `KNOWN-ISSUES.md`。

## 0.9.1-beta 是什么

0.9.1 是 0.9 原生界面的**体验与性能修正版**，功能范围不变：

- **导航指示线**：改成时间驱动（140 ms ease-out），从当前位置滑向目标；
  首帧和"减少动态"时直接落位，不再从左边飞过来。
- **窗口自适应**：任意窗口比例都按 1120×700 等比缩放并居中，resize 时
  立即重绘，鼠标命中与绘制共用同一套几何。
- **性能**：刷新率 30 → 60 Hz；波形/频谱/XY 改成单条路径一次描边；
  颜色、字体行、凸起渐变全部缓存；FFT/THD/相位降到 20 Hz。
- **浅色模式**：示波器、频谱、XY 的测量面固定为深石墨（#283133），
  不再跟着主题变亮。
- **排印与表格**：Brand 角色 + 按钮 Label 角色；ASCII 字距 0.02–0.06em，
  中文不加宽；Label 与 Value 共用基线；三张表改成固定列坐标；数字用真减号 −。
- **图表刻度**：示波器 `+1.0 / 0 / −1.0`、`0 / 7.5 / 15 ms`；频谱
  `0 … −100 dB`、`20 Hz / 1 kHz / 20 kHz`。
- **Windows 同步**：深石墨测量面、同一套导航缓动、16 ms 定时器、
  XY 单折线。

完整清单见 [`CHANGELOG.md`](CHANGELOG.md)。

## 0.9-alpha 是什么

0.9 把 0.7/0.8 的 Web 前端全部移除，改成双端**本地原生界面**：

- **macOS**：`AudioScope.app`，AppKit + CoreGraphics + CoreText，内嵌 Geist 字体。
- **Windows**：`AudioScope.exe` + Inno Setup 安装包，Win32 + GDI+。
- 七个页面：波形、频谱、相位（XY）、测量、维修、报告、校准。
- 多页 PDF：摘要 + 波形图 + 频谱图。
- Loopback：−20 dBFS / 1 kHz 输出→输入，读回电平与 THD（需手动确认）。
- 采集是显式 opt-in；默认合成数据，界面明确标注。
- 无 WebView、无 HTTP、无 WebSocket、无遥测。

## 构建

```bash
cd alpha

# macOS 原生 app + CLI + 测试
make app          # build/AudioScope.app
make mac          # build/audioscope-alpha (CLI)
make test         # 52 项 selftest + ringtest + 导航动画回归 + native --shot

# 离屏渲染七页（调试/截图）
./build/AudioScope.app/Contents/MacOS/AudioScope --shot build/shots

# Windows 交叉编译（需要 mingw-w64）
make win          # CLI 与控制台核心
# 原生 Windows GUI 由 CI 的 MSVC 构建；本机无 MSVC。
```

## 运行

```bash
open build/AudioScope.app
```

- `1–7` 切页，`Space` 保持，`C` 采集，`R` 跑维修计划，`E` 导出报告，`T` 切主题。
- 采集/回环都会先弹原生确认框；输入模式不会自动切换。

## 设计基线

- 颜色/材质/排印取 BerryStudio / MasterLens Precision Instrument token：
  暖灰石墨、单一鼠尾草绿 accent、琥珀警告、砖红错误；4 px 网格。
- 暗色为主，明色独立设计；数字等宽；无霓虹/发光。
- macOS 字体：Geist Regular/Medium/Bold + Geist Mono（SIL OFL 1.1，
  `assets/fonts/OFL.txt`）。Windows：Microsoft YaHei UI / Consolas。

## 0.9-alpha 的限制

- **Windows 仍是 WinMM 2ch**；WASAPI 与 >2ch 未实现。
- Windows 未在真实音频设备上运行验证（CI 只保证编译/测试/打包）。
- 二进制未签名；macOS Gatekeeper / Windows SmartScreen 可能提示。
- 未校准只显示 dBFS；没有校准向导 UI。
- 没有设备选择 UI，采集/回环使用系统默认设备。
- Loopback 需要手动接线；不是校准过的 DUT 测量。
- 没有硬件保护前端：软件不能保护声卡。

## 后续

- WASAPI、真机验证、设备选择、游标/缩放、校准向导、签名/公证。
- 记录见 [`../docs/plan/V0.9.md`](../docs/plan/V0.9.md)。
