# AudioScope by BerryStudio — 0.5-alpha (Repair Measurement Suite)

> **AudioScope by BerryStudio** · `0.5.0-alpha.1` · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 0.4-alpha 保留在 tag `v0.4.0-alpha.1`；0.3-alpha 保留在 tag `v0.3.0-alpha.1`。

## 0.5-alpha 是什么

在 0.4-alpha 的维修测量之上，补齐 **失真与频谱测量**：

- **THD / THD+N / SNR / SINAD / SFDR / ENOB**：`thd`，单音 FFT 分析。
- **IMD**：`imd`，SMPTE（60 Hz + 7 kHz）与 DIN（250 Hz + 8 kHz）。
- **THD vs 电平扫描**：`sweep`，输出表格与 CSV。
- 保留 wow/flutter、串扰、阻抗、方位角、前后对比、多通道、XY、频谱热力图、SafetyGuard、校准、JSON/HTML 报告。

## 当前命令

| 命令 | 说明 |
|---|---|
| `thd [秒]` | THD/THD+N/SNR/SINAD/SFDR/ENOB；`--synthetic --freq 1000 --level -6 --thd 0.1` |
| `imd [秒]` | SMPTE/DIN IMD；`--synthetic --standard smpte|din` |
| `sweep` | THD vs 电平；`--start -40 --end 0 --step 6 --csv thd.csv` |
| `wow` | wow/flutter RMS + 带速 ppm |
| `crosstalk` | CH1→CH2 串扰 dB |
| `impedance` | 串联电阻法阻抗 |
| `azimuth` | 10 kHz L/R 相位 |
| `compare` | 两份报告 JSON 前后差值 |
| `measure --channels all` | 多通道测量 + JSON/HTML |
| `xy` / `drift` / `spectrogram` | 相位/漂移/热力图 |
| `calibrate` / `modes` / `safety` / `check` | 校准、输入模式、安全、信号检查 |
| `selftest` | 44 项核心自检 |

### 示例

```bash
# THD：合成 1 kHz，注入约 0.1% 谐波
./build/audioscope-alpha thd 2 --synthetic --freq 1000 --level -6 --thd 0.1

# IMD：SMPTE 60 Hz + 7 kHz
./build/audioscope-alpha imd 2 --synthetic --standard smpte

# THD vs 电平
./build/audioscope-alpha sweep --start -40 --end 0 --step 6 --csv thd.csv
```

## 构建与测试

```bash
cd alpha
make mac      # macOS
make win      # Windows cross-build
make test     # 44 项 selftest + ringtest
```

macOS 12+ / Xcode CLT；Windows 交叉编译需要 mingw-w64。

## 测试覆盖

- 单位、输入模式、SafetyGuard、clip feed、量程建议
- 校准 pad/rate/checksum、JSON/HTML 报告、判定
- FFT、相位/增益、100 ppm 漂移、频谱矩阵
- wow/flutter、串扰、阻抗、THD 0.1%、IMD 0.1%
- ASCII canvas、ring buffer

## 0.5-alpha 的限制

- **没有原生 GUI**：仍是 ASCII TUI。
- **Windows 仍是 WinMM 2ch**：WASAPI 未实现。
- **THD/IMD 是单音/双音 FFT**：没有扫频 THD vs 频率、没有 IMD 多标准全套。
- **sweep 目前用合成信号**：真实设备需要输出→DUT→输入闭环。
- **没有维修模板/PDF**：后续版本。
- **没有硬件前端**：软件不能保护声卡。

## 后续

- 0.6-alpha：原生 Canvas + WASAPI + 维修模板 + PDF + THD vs 频率。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
