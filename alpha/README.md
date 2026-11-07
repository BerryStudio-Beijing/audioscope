# AudioScope by BerryStudio — 0.7-alpha (Precision Instrument UI)

> **AudioScope by BerryStudio** · `0.7.0-alpha.1` · macOS + Windows · C11 core + C++17 app + local Web UI
> 0.6-alpha 保留在 tag `v0.6.0-alpha.1`；0.5-alpha 保留在 tag `v0.5.0-alpha.1`。

## 0.7-alpha 是什么

按同工作区的 **BerryStudio UI / MasterLens 3.0 复刻设计标准** 重做前端视觉：

- **Precision Instrument**：暖灰石墨面板、单一鼠尾草绿 accent、琥珀/陶土红仅表达语义。
- **1120×700** 逻辑画布、4 px 网格、Geist 字体、暗/明双主题。
- **三种材质**：Flat 数据、Raised 面板、Recessed 图表/输入。
- **七个页面**：波形、频谱、相位、测量、维修、报告、校准。
- **本地 Web UI**：`audioscope-alpha ui` 把 C++ 核心生成的快照注入页面，不依赖网络/云。

![主界面](web/docs/screens/main.png)

![明色主题](web/docs/screens/light.png)

## 启动 UI

```bash
cd alpha

# 1) 只预览设计（内置合成快照）
python3 -m http.server 8124 --bind 127.0.0.1 --directory web
# 打开 http://127.0.0.1:8124/

# 2) 注入 C++ 核心生成的快照
./build/audioscope-alpha ui --out build/ui
python3 -m http.server 8125 --bind 127.0.0.1 --directory build/ui
# 打开 http://127.0.0.1:8125/
```

`ui` 会复制 `web/`，并把 `<!--DATA-->` 替换为 `window.AUDIOSCOPE_DATA`，包含波形、频谱、THD、XY、通道、维修计划和校准快照。

## UI 页面

| 页面 | 内容 |
|---|---|
| 波形 | Peak / RMS / 频率 / 波峰因数 / Vpp / clip，主波形 + 触发线 |
| 频谱 | THD / THD+N / SNR / SINAD / SFDR / ENOB，对数频谱 + 谐波标记 |
| 相位 | 相位 / 增益 / 极性，XY 密度图 + 拟合椭圆 |
| 测量 | 8 通道表、限值、判定、未校准电压警告 |
| 维修 | tape 计划步骤、阈值、PASS/FAIL、PDF/JSON |
| 报告 | JSON/HTML/PDF 最近报告 |
| 校准 | profile 字段、置信度、电压不可用提示 |

## 设计基线

- 颜色/材质/排印取 `berrystudio-ui/design-system/berry-tokens.css`，不散落十六进制。
- 字体：Geist Regular/Medium/Bold + Geist Mono（SIL OFL 1.1，见 `web/NOTICE.md`）。
- 数字 tabular；负号 U+2212、范围 U+2013、缺失 U+2014。
- 图表在明色主题下保持深石墨测量面。
- 快捷键：`1–7` 切页、`Esc` 关闭弹窗、`⌘/Ctrl+E` 导出快照。
- 主题/弹窗支持深链接：`?theme=light#scope`、`#about`、`#settings`。

## 构建与测试

```bash
cd alpha
make mac
make win
make test     # 47 项 selftest + ringtest + ui smoke
```

UI 截图（需要 Chrome）：

```bash
./web/scripts/shoot.sh
# -> web/docs/screens/*.png (1120×700 @ 2×)
```

## 0.7-alpha 的限制

- **Web UI 是本地只读快照**：不是实时 WebSocket 音频流；实时交互仍走 CLI。
- **没有原生 GUI**：CoreGraphics/Direct2D 未实现；Web UI 是当前前端。
- **Windows 仍是 WinMM 2ch**：WASAPI 未实现。
- **维修计划仍只跑合成验证**：真实 DUT 闭环未接线。
- **PDF 单页摘要**：无多页/图表/曲线叠加。
- **没有硬件前端**：软件不能保护声卡。

## 后续

- 0.8-alpha：原生 Canvas / WASAPI / 实时 WebSocket 桥 / 真实 DUT 闭环。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
