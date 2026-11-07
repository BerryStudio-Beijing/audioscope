# AudioScope by BerryStudio — 0.6-alpha (Repair Plans & PDF)

> **AudioScope by BerryStudio** · `0.6.0-alpha.1` · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 0.5-alpha 保留在 tag `v0.5.0-alpha.1`；0.4-alpha 保留在 tag `v0.4.0-alpha.1`。

## 0.6-alpha 是什么

在 0.5-alpha 的失真测量之上，补齐 **维修计划、PDF 报告、THD vs 频率**：

- **维修模板**：`templates` 列出 quick / pedal / tape / mixer / headphone；`repair` 运行合成验证计划。
- **PDF 报告**：`measure` 与 `repair` 生成可打印 PDF；HTML 仍是 canonical 报告。
- **THD vs 频率**：`freqsweep` 对数频率扫描，输出表格与 CSV。
- 保留 THD/IMD、wow/flutter、串扰、阻抗、方位角、多通道、XY、热力图、SafetyGuard、校准。

## 当前命令

| 命令 | 说明 |
|---|---|
| `templates` | 列出内置维修计划 |
| `repair --template tape` | 运行合成维修计划；输出 JSON + PDF |
| `freqsweep` | THD vs 频率；`--start 20 --end 20000 --points 12 --csv freq.csv` |
| `thd` / `imd` / `sweep` | 失真测量与 THD vs 电平 |
| `measure --channels all` | 多通道测量；JSON + HTML + PDF |
| `wow` / `crosstalk` / `impedance` / `azimuth` | 维修测量 |
| `compare` | 前后报告 JSON 差值 |
| `xy` / `drift` / `spectrogram` | 相位/漂移/热力图 |
| `calibrate` / `modes` / `safety` / `check` | 校准、输入模式、安全、信号检查 |
| `selftest` | 47 项核心自检 |

### 示例

```bash
# 列出并运行维修计划
./build/audioscope-alpha templates
./build/audioscope-alpha repair --template tape --out repairs

# THD vs 频率
./build/audioscope-alpha freqsweep --start 20 --end 20000 --points 12 --csv freq.csv

# 测量 + 三份报告
./build/audioscope-alpha measure 2 --mode inst --channels all --out reports
# reports/run-*.json / .html / .pdf
```

## 构建与测试

```bash
cd alpha
make mac
make win
make test     # 47 项 selftest + ringtest
```

macOS 12+ / Xcode CLT；Windows 交叉编译需要 mingw-w64。

## 报告

- JSON：机器可读，schema v2。
- HTML：自包含、离线可开，canonical。
- PDF：单页可打印摘要，Helvetica 文本；验证过 `file`/`qlmanage` 可渲染。
- 维修计划：`repair-<template>.json` + `.pdf`。

## 测试覆盖

- 单位、输入模式、SafetyGuard、clip feed、量程建议
- 校准、JSON/HTML/PDF 报告、判定
- FFT、相位/增益、漂移、频谱矩阵
- wow/flutter、串扰、阻抗、THD、IMD、THD 扫描
- 维修模板、PDF 头/文件、ASCII canvas、ring buffer

## 0.6-alpha 的限制

- **没有原生 GUI**：仍是 ASCII TUI。
- **Windows 仍是 WinMM 2ch**：WASAPI 未实现。
- **维修计划目前只跑合成验证**：`repair --live` 明确未接线；真实 DUT 闭环待后续。
- **PDF 是单页摘要**：无图表/多页/曲线叠加。
- **THD vs 频率用合成信号**：真实闭环扫频未做。
- **没有硬件前端**：软件不能保护声卡。

## 后续

- 0.7-alpha：原生 Canvas + WASAPI + 真实 DUT 闭环 + 多页 PDF/曲线。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
