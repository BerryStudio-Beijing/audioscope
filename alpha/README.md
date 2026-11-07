# AudioScope by BerryStudio — 0.8-alpha (Live Bridge)

> **AudioScope by BerryStudio** · `0.8.0-alpha.1` · macOS + Windows · C11 core + C++17 app + local Web UI
> 0.7-alpha 保留在 tag `v0.7.0-alpha.1`；0.6-alpha 保留在 tag `v0.6.0-alpha.1`。

## 0.8-alpha 新增了什么

0.7 的 Web UI 是**只读快照**：`ui` 生成一次 `AUDIOSCOPE_DATA`，刷新永远是同一份。
0.8 加了一条**本地实时桥**，仍然零第三方依赖、只监听 `127.0.0.1`：

- `audioscope-alpha ui --live` 复制 Web UI 后，自己起一个极小的 HTTP/1.1 + WebSocket 服务，
  不再需要 `python3 -m http.server`。
- 浏览器连 `ws://127.0.0.1:<port>/ws`，服务端每 500 ms 推送一次同一个快照对象
  （波形、频谱、THD/SNR、通道表），页面原地刷新，顶部出现 `LIVE` 状态。
- 快照生成与静态 `ui` 共用 `ui_snapshot_json()`，不会出现两套字段。
- 默认数据源是**合成信号**（明确标注 `live · synthetic`），因为采集需要麦克风权限；
  `--capture` 才打开默认/指定输入，打开失败会回退并打印原因。
- `--smoke` 在进程内跑一遍 HTTP GET + WebSocket 握手 + 收帧，`make test` / CTest 用它做回归。

0.8-alpha.1 仍**不是**完整 0.8：WASAPI、Windows >2ch、真实 DUT 闭环、多页 PDF 还没做。

## 启动 UI

```bash
cd alpha
make mac

# 1) 静态快照（一次性注入）
./build/audioscope-alpha ui --out build/ui
python3 -m http.server 8124 --bind 127.0.0.1 --directory build/ui

# 2) 实时桥（自带 HTTP 服务，默认合成数据源）
./build/audioscope-alpha ui --live --port 8125
# 打开 http://127.0.0.1:8125/

# 3) 实时桥 + 真实输入（会触发 macOS 麦克风权限）
./build/audioscope-alpha ui --live --port 8125 --capture --device 2

# 4) 只跑传输层自检（CI 用）
./build/audioscope-alpha ui --live --smoke
```

`Ctrl-C` 停止服务。`--interval MS` 调整推送间隔（最小 100 ms），`--duration SEC` 用于自动退出。

![主界面](web/docs/screens/main.png)

![明色主题](web/docs/screens/light.png)

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
make test     # 47 项 selftest + ringtest + 静态 ui smoke + live bridge smoke
```

UI 截图（需要 Chrome）：

```bash
./web/scripts/shoot.sh
# -> web/docs/screens/*.png (1120×700 @ 2×)
```

## 0.8-alpha 的限制

- **实时桥默认是合成信号**：`--capture` 才接真实输入；没有实时 DSP 重采样/多设备聚合。
- **没有原生 GUI**：CoreGraphics/Direct2D 未实现；Web UI 是当前前端。
- **Windows 仍是 WinMM 2ch**：WASAPI 未实现。Windows 的实时桥代码已交叉编译进 PE，
  但本轮没有 Windows 真机可验证。
- **维修计划仍只跑合成验证**：真实 DUT 闭环未接线。
- **PDF 单页摘要**：无多页/图表/曲线叠加。
- **没有硬件前端**：软件不能保护声卡。

## 后续

- 0.8-alpha 剩余：WASAPI 共享模式 + >2ch、真实 DUT 闭环、多页 PDF/曲线叠加、
  Windows 真机验证、硬件前端参考设计。
- 机会清单见 [`KNOWN-ISSUES.md`](KNOWN-ISSUES.md) 的「下一版必须解决」。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
