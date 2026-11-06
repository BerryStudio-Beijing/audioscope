# AudioScope by BerryStudio — Changelog

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
