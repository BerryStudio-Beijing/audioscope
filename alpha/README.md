# AudioScope by BerryStudio — 0.4-alpha (Repair Workflow Core)

> **AudioScope by BerryStudio** · `0.4.0-alpha.1` · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 0.3-alpha 保留在 tag `v0.3.0-alpha.1`；0.2-alpha 保留在 tag `v0.2.0-alpha.1`。

## 0.4-alpha 是什么

在 0.3-alpha 的“多通道 + 相位”之上，补齐路线图 **v1.4 Repair Workflow** 的核心维修测量：

- **Wow & flutter**：3 kHz 测试音/测试带，输出 wow/flutter RMS % 与带速 ppm。
- **串扰**：CH1 驱动、CH2 泄漏，输出 dB。
- **阻抗**：串联电阻法，`Z = R·Vdut/(Vsource−Vdut)`。
- **方位角**：10 kHz L/R 相位差，指导磁头调整。
- **前后对比**：读取两份报告 JSON，输出峰值/RMS/Vpp/频率/削波差值。
- 保留多通道采集、XY/Lissajous、频谱热力图、SafetyGuard、校准、JSON/HTML 报告。

## 当前命令

| 命令 | 说明 |
|---|---|
| `wow [秒]` | wow/flutter；`--synthetic --wow 0.3 --flutter 0.1` 可离线验证 |
| `crosstalk [秒]` | CH1→CH2 串扰；`--synthetic` 注入 −60 dB |
| `impedance` | `--r 1000 --vsrc 1 --vdut 0.5` 或 `--simulate 32` |
| `azimuth [秒]` | 10 kHz L/R 相位；`--synthetic --phase 10` |
| `compare before.json after.json` | 维修前后报告差值 |
| `xy` / `drift` / `spectrogram` | 0.3 的相位/漂移/热力图 |
| `measure --channels all` | 多通道测量 + JSON/HTML |
| `calibrate` / `calibs` | 单点校准 profile |
| `modes` / `safety` / `check` | 输入模式、安全、信号检查 |
| `selftest` | 42 项核心自检 |

### 示例

```bash
# wow & flutter（注入 0.3% peak + 0.1% peak）
./build/audioscope-alpha wow 10 --synthetic --wow 0.3 --flutter 0.1

# 串扰 −60 dB
./build/audioscope-alpha crosstalk 2 --synthetic

# 32 Ω 阻抗（R=1 kΩ）
./build/audioscope-alpha impedance --r 1000 --vsrc 1 --simulate 32

# 方位角 10°
./build/audioscope-alpha azimuth 2 --synthetic --phase 10

# 前后对比
./build/audioscope-alpha compare reports/before.json reports/after.json
```

## 构建与测试

```bash
cd alpha
make mac      # macOS
make win      # Windows cross-build
make test     # 42 项 selftest + ringtest
```

macOS 12+ / Xcode CLT；Windows 交叉编译需要 mingw-w64。

CMake：

```bash
cmake -S alpha -B alpha/build -DAUDIOSCOPE_PLATFORM=macos
cmake --build alpha/build
ctest --test-dir alpha/build --output-on-failure
```

## 测试覆盖

- 单位、输入模式、SafetyGuard、clip feed、量程建议
- 校准 pad/rate/checksum、JSON/HTML 报告、判定
- FFT、相位/增益、100 ppm 漂移、频谱矩阵
- wow/flutter 0.3% + 0.1%（RMS）、串扰 −60 dB、阻抗 1000 Ω
- ASCII canvas、ring buffer

## 0.4-alpha 的限制

- **没有原生 GUI**：仍是 ASCII TUI。
- **Windows 仍是 WinMM 2ch**：WASAPI 未实现。
- **真实漂移仍需双设备/聚合设备**；同设备 CH1/CH2 共享时钟。
- **wow/flutter 用零交叉瞬时频率**：测试带必须干净、3 kHz 标称；RMS 与峰值会差 √2。
- **阻抗是手动串联电阻法**：需要用户提供 R、Vsource、Vdut。
- **方位角只给相位差**：没有自动磁头调整机构。
- **没有 THD/IMD/频响模板**：后续版本。
- **没有硬件前端**：软件不能保护声卡。

## 后续

- 0.5-alpha：原生 Canvas + WASAPI + THD/IMD + 维修模板 + PDF。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
