# AudioScope by BerryStudio — 1.0 (Native)

> **AudioScope by BerryStudio** · `1.0.0` · macOS 12+ arm64 · Windows 10/11 x64
> C11 core + C++17 app · native AppKit/CoreGraphics + Win32/GDI+ · no Web UI

## 1.0 是什么

1.0 是第一个正式版：七页原生测量界面、发生器、设备选择、报告与安装包。
本版重点不是加页面，而是让已有结果**可相信**：

- **双通道独立**：CH1/CH2 各自计算 peak / RMS / 频率 / Vpp / 削波，报告使用
  每通道数据，不再由 CH1 推算 CH2。
- **实际采样率**：统计、FFT bin、THD、相位、时间轴与 loopback 跟随设备实际率。
- **模拟结果明确**：Tape Plan 标注 `SIMULATED`，屏幕为 `DEMO PASS/FAIL`，
  导出报告带模拟备注并降为 WARN。
- **信号发生器**：9 种波形、默认 −20 dBFS 上限、200 ms 淡入、固定种子噪声。
- **设备与 FFT**：IN/OUT 选择、热插拔检测、FFT 1024–16384。
- **Windows**：WinMM 设备编号修正；`setup.exe` + 便携 zip；macOS `.app`/`.dmg`。

## 0.9.3-beta 是什么

0.9.3 给“只读监测”补上主动输出与音频 I/O：

- **信号发生器**：正弦、方波、三角、锯齿、白噪、粉噪、对数扫频、单位脉冲、
  阶跃；方波/锯齿带 PolyBLEP，噪声固定种子可复现。
- **安全电平**：预设 −60 / −40 / −30 / −20 dBFS，默认钳在 **−20 dBFS**，
  UI 不提供解锁入口；输出启动 200 ms 淡入。
- **设备选择**：波形页 IN / OUT 循环选择，采集、发生器、loopback 都使用
  选中设备；稳定 id 在重新枚举后找回。
- **热插拔**：每 2 s 枚举；选中输入消失时停止采集并提示，不静默切换。
- **FFT 尺寸**：频谱页 1024 / 2048 / 4096 / 8192 / 16384，切换立即刷新。
- 自检 52 → **63 项**；`make test` 另含 ringtest、12 项 navtest 与七页截图。

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
make test         # 63 项 selftest + ringtest + 12 项 navtest + 10 项 modeltest + 七页截图

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
- **设备选择是基础版**：可循环选择输入/输出并记忆 id，但没有设备详情、
  采样率切换或通道映射；Windows 仍固定 2ch。
- Loopback 需要手动接线；结果只代表连通性，不是校准过的 DUT 测量。
- **Tape Plan 是固定夹具演示**（SIMULATED），不是被测设备结果。
- 没有硬件保护前端：软件不能保护声卡。

## 后续

- WASAPI、真机验证、采样率切换、游标/缩放、校准向导、签名/公证。
- 记录见 [`../docs/plan/V1.0.md`](../docs/plan/V1.0.md)。
