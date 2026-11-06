# AudioScope by BerryStudio — Changelog

## 0.5.0-alpha.1 — 2026-11-07

### Added

- THD / THD+N / SNR / SINAD / SFDR / ENOB single-tone analysis.
- IMD SMPTE (60 Hz + 7 kHz) and DIN (250 Hz + 8 kHz).
- THD vs level sweep with CSV export.
- `thd`, `imd`, `sweep` commands.
- Selftest expanded to 44 checks (THD 0.1%, IMD 0.1%).

### Changed

- Version 0.5.0-alpha.1.
- Repair measurement suite focus; native GUI/WASAPI remain future work.

## 0.4.0-alpha.1 — 2026-11-07

### Added

- Wow & flutter measurement from a 3 kHz tone/test tape; RMS wow/flutter % and speed ppm.
- Crosstalk measurement CH1 -> CH2 in dB.
- Series-resistor impedance measurement.
- Azimuth measurement from 10 kHz L/R phase.
- `compare` command for before/after report JSON deltas.
- Selftest expanded to 42 checks (wow/flutter, crosstalk, impedance).

### Changed

- Version 0.4.0-alpha.1.
- Repair workflow focused release; native GUI/WASAPI remain future work.

## 0.3.0-alpha.1 — 2026-11-07

### Added

- Multi-channel capture: macOS CoreAudio backend now exposes up to 8 input channels; ring/report support 8 channels.
- `measure --channels all|0,1` multi-channel measurement with per-channel stats, limits and report rows.
- `xy` command: ASCII Lissajous + phase/gain/polarity readout; `--simulate` with phase/gain/drift.
- `drift` command: 100 ppm simulation + estimation; live same-device capture explicitly reports that drift is not observable.
- `spectrogram` command: time × log-frequency dB matrix, ASCII heatmap, peak row readout.
- Core phase measurement (`as_phase_measure`) and drift estimation (`as_drift_estimate`).
- Core spectrogram matrix (`AsSpectrogram`) with log-frequency rows.
- Selftest expanded to 39 checks (phase/gain, 100 ppm drift, spectrogram, multi-channel report).

### Changed

- Version 0.3.0-alpha.1.
- Report channel array expanded from 2 to 8.
- `devices` shows input/output channel counts.

## 0.2.0-alpha.1 — 2026-11-07

### Added

- Input mode presets (Line Pro / Line Consumer / DI / Inst / Mic / Probe) with nominal level, max Vpp, impedance, phantom and front-end requirements.
- SafetyGuard feed path: clip detection/latch over interleaved frames; range recommendation.
- Calibration schema v2 extended with `pad_db` and `sample_rate`; per-device/channel/gain/pad/rate profile; CLI calibration wizard.
- Report schema v2: JSON + self-contained HTML; limits, verdict, repeatability, provenance.
- `measure` command: capture, limits, verdict, JSON/HTML, session event.
- `check` command: signal check assistant (peak/RMS/crest/hum/tilt) with advisory suggestion only.
- `calibrate`/`calibs` commands.
- Expanded selftest: input modes, clip feed, calibration pad/rate, report JSON/HTML, verdict.

### Changed

- Version 0.2.0-alpha.1.
- `tone` defaults to −20 dBFS output limit; `--allow-loud` required for higher.
- Branding: AudioScope by BerryStudio.

## 0.1.0-alpha.1 — 2026-11-07

### Added

- 独立的 `alpha/` 双端基线：C11 核心 + C++17 应用/UI。
- C ABI：版本、单位换算、SafetyGuard、单点校准、JSONL 会话日志。
- 便携核心：radix-2 FFT、Hann/Blackman、时域统计、正弦/方波发生器、SPSC ring。
- macOS CoreAudio 后端：设备枚举、输入采集、输出测试音（2ch float32）。
- Windows WinMM 后端：设备枚举、输入采集、输出测试音（2ch int16）。
- ASCII TUI：实时示波器、合成示波器、对数 FFT。
- CLI：`version`、`devices`、`monitor`、`tone`、`scope`、`fft`、`units`、`safety`、`session`、`calibrate`、`selftest`。
- 构建：`alpha/Makefile`（macOS + mingw 交叉编译）、`alpha/CMakeLists.txt`、GitHub Actions 双端 CI。
- 文档：alpha README、Known Issues。

### Changed

- 原 v1.1 保留为 tag `v1.1.0-initial`；alpha 分支不覆盖原实现。

### Security

- 明确“软件不能保护声卡”；SafetyGuard 只做预检、锁存、静音。
