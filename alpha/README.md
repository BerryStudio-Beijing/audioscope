# AudioScope by BerryStudio — 0.3-alpha (Multi-channel & Phase Core)

> **AudioScope by BerryStudio** · `0.3.0-alpha.1` · macOS + Windows · C11 core + C++17 app · no third-party runtime deps
> 0.1-alpha 保留在 tag `v0.1.0-alpha.1`；0.2-alpha 保留在 tag `v0.2.0-alpha.1`。

## 0.3-alpha 是什么

在 0.2-alpha 的“安全 + 校准 + 报告”之上，补齐路线图 **v1.3 Multi-channel & Phase** 的核心：

- **多声道采集**：macOS CoreAudio 最多 8 路输入；`measure --channels all|0,1` 逐通道测量。
- **XY / Lissajous**：`xy` 显示双通道图形，给出相位/增益/极性判断。
- **相位与漂移**：`as_phase_measure()` 测相位/增益；`drift` 用 100 ppm 仿真验证漂移估计算法。
- **频谱热力图**：`spectrogram` 生成时间 × 对数频率的 dB 矩阵 ASCII 热力图。
- 保留 0.2 的输入模式、SafetyGuard、逐档校准、JSON/HTML 报告。

## 当前命令

| 命令 | 说明 |
|---|---|
| `version` | 版本、品牌、平台 |
| `devices` | 设备枚举（含 in/out 通道数） |
| `modes` | 输入模式与阻抗/最大电平 |
| `monitor` | 实时 ASCII 示波器（`--channel N`） |
| `check` | 信号检查助手（`--synthetic` 可离线） |
| `xy` | 双通道 XY/Lissajous；`--simulate --phase --gain --drift` |
| `drift` | 漂移估算；`--simulate 100` 或 `--live` |
| `spectrogram` | 时间×频率热力图；`--synthetic` 可离线 |
| `measure` | 多通道测量；`--channels all|0,1`、限值、JSON/HTML |
| `calibrate` / `calibs` | 单点校准 profile |
| `tone` | 测试音（默认 −20 dBFS 限幅） |
| `scope` / `fft` | 合成示波器 / 频谱 |
| `units` / `safety` / `session` | 单位、安全、日志 |
| `selftest` | 39 项核心自检 |

### 示例

```bash
# 双通道 XY（仿真 45° / +3 dB）
./build/audioscope-alpha xy 2 --simulate --phase 45 --gain 3

# 漂移算法验证（注入 100 ppm）
./build/audioscope-alpha drift 5 --simulate 100

# 频谱热力图（合成 1k + 3k + 5k）
./build/audioscope-alpha spectrogram 1 --synthetic

# 多通道测量
./build/audioscope-alpha measure 2 --mode inst --channels all \
  --max-vpp 10 --repeat 3 --out reports
```

## 构建

```bash
cd alpha
make mac      # macOS: build/audioscope-alpha
make win      # Windows: build/audioscope-alpha.exe (mingw-w64)
make test     # selftest + scope/fft/ring smoke
```

要求：macOS 12+ / Xcode CLT；Windows 交叉编译需要 `x86_64-w64-mingw32-gcc/g++`。

CMake：

```bash
cmake -S alpha -B alpha/build -DAUDIOSCOPE_PLATFORM=macos
cmake --build alpha/build
ctest --test-dir alpha/build --output-on-failure
```

## 架构

```
alpha/src/core/        C11：FFT、窗、统计、ring、相位/漂移、频谱矩阵、单位、安全、校准、报告
alpha/src/app/         C++17：CLI/TUI、XY、热力图、测量、自检
alpha/src/ui/          C++17：平台无关 ASCII Canvas
alpha/src/platform/    CoreAudio / WinMM / stub
```

## 0.3-alpha 的限制

- **没有原生 GUI**：仍是 ASCII TUI；CoreGraphics/Direct2D 仍未实现。
- **Windows 仍是 WinMM 2ch**：WASAPI 与 >2ch 尚未实现。
- **漂移只在仿真中可测**：真实跨设备/聚合设备采集尚未接线；同设备 CH1/CH2 共享时钟，漂移不可观测。
- **没有 Lissajous 余晖/椭圆拟合**：当前是 ASCII 点阵 + 相位/增益读数。
- **没有多通道 XY 矩阵**：只支持任意两通道 XY。
- **没有硬件前端**：软件不能保护声卡。
- **未签名/未公证**。

## 测试

`make test` 当前覆盖 39 项，包括：

- 单位、输入模式、SafetyGuard、clip feed、量程建议
- 校准 pad/rate/checksum、JSON/HTML 报告、判定
- FFT、相位/增益（45°/+3 dB）、100 ppm 漂移、频谱矩阵
- ASCII canvas、ring buffer

## 后续

- 0.4-alpha：原生 Canvas（CoreGraphics/Direct2D）、WASAPI、Lissajous 余晖/椭圆拟合。
- 0.5-alpha：真实跨设备漂移/聚合设备、多通道 XY、频谱热力图交互。
- 路线图见 [`../docs/plan/ROADMAP.md`](../docs/plan/ROADMAP.md)。
