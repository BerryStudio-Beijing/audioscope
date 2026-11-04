# AudioScope 1.1 — macOS arm64

## 下载
| 文件 | 用途 |
|---|---|
| `AudioScope-1.1-macos-arm64.dmg` | 拖拽安装（推荐）|
| `AudioScope-1.1-macos-arm64.zip` | 解压即用，适合脚本/CI |

要求：Apple Silicon（arm64），macOS 12.0 或更新。未做公证（ad-hoc 签名），
首次打开请右键 → 打开，或 `xattr -dr com.apple.quarantine /Applications/AudioScope.app`。

## 本版内容
- **界面整体重绘**：暖灰石墨 + 单一鼠尾草绿重点色、三种排印角色（14/16/26）、
  4px 网格、图表 32px 安全区、仅向下填充、明暗双主题（T 键切换）。
- **新增闭环测试页**：直连校准 → 频响/相位/THD+N/噪声底/上升沿/延迟 → JSON+CSV+WAV 报告
  + 规则式电路调整建议与下一轮参数。全程本地。
- 自写波形算法（PolyBLEP 带限、固定种子噪声），带 23 项单元测试与虚拟 DUT 端到端验证。
- 修复：峰值保持不上升、THD+N 恒为 0、生成器配置撕裂读、FFT/Bode 刻度与填充、
  以及部署目标（此前二进制 minos=27.0，现在正确声明为 12.0）。

## 校验
```bash
shasum -a 256 -c SHA256SUMS.txt
```

## 自检
```bash
/Applications/AudioScope.app/Contents/MacOS/AudioScope --selftest   # 30/30
/Applications/AudioScope.app/Contents/MacOS/AudioScope --unittest   # 23/23
```

Windows 版本尚未提供：本程序使用 Cocoa / CoreAudio / CoreText，Windows 需要重写平台层
（Win32 + Direct2D/DirectWrite + WASAPI），不是重新编译即可得到。
