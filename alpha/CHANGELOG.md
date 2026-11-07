# AudioScope by BerryStudio — Changelog

## 0.9.0-alpha.1 — 2026-11-07

### Added

- **Native macOS application** (`AudioScope.app`): AppKit window, CoreGraphics
  canvas, CoreText, Geist Regular/Medium/Bold and Geist Mono bundled from
  `assets/fonts`. Seven pages: scope, spectrum, XY, measure, repair, reports,
  calibration. Keyboard `1–7`, space to hold, `C` capture, `R` run plan,
  `E` export, `T` theme.
- **Native Windows application** (`AudioScope.exe`): Win32 window + GDI+
  rendering, same seven pages and design tokens, plus an Inno Setup
  **installer** (`AudioScope-0.9.0-alpha.1-windows-x64-setup.exe`).
- **Multi-page PDF**: report page 1 keeps the summary; page 2 adds the
  waveform chart; page 3 adds the spectrum chart (`as_pdf_new_page`,
  `as_pdf_polyline`, `as_report_write_pdf_ex`).
- **Loopback measurement** in the native app: −20 dBFS 1 kHz tone on the
  default output, capture from the default input, level/THD verdict. Requires
  explicit confirmation.
- `AudioScope --shot DIR` renders all seven pages offscreen at 2× for visual
  regression and CI.

### Changed

- Version 0.9.0-alpha.1.
- **Web UI removed**: `alpha/web/`, the live HTTP/WebSocket bridge, the
  `AUDIOSCOPE_DATA` injection and `ui --live` are gone. All rendering is now
  local AppKit/CoreGraphics on macOS and Win32/GDI+ on Windows.
- Geist fonts moved to `alpha/assets/fonts/` and are embedded in the `.app`.
- Capture remains opt-in and visible; synthetic data is the default.

### Known limits

- Windows audio is still WinMM 2ch; WASAPI shared mode and >2ch are not in this
  alpha.
- Windows binaries are unsigned (SmartScreen) and were compile/tested on CI,
  not on a Windows audio machine.
- Loopback needs a physical or virtual output→input route; it is not a
  calibrated DUT measurement.
- Voltage stays hidden until a measured calibration profile exists.

## 0.8.0-alpha.1 — 2026-11-07

See tag `v0.8.0-alpha.1`.
