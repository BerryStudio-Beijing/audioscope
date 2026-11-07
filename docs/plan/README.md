# AudioScope by BerryStudio 产品路线图包

> 这是 **规划文档包**，不是代码实现。
> 目标：把 AudioScope 从“macOS 声卡示波器 1.1”推进到“macOS/Windows 双端音频维修工作台 v2.0”，并让 AI coding agent 能按任务安全地实现。
> 语言基线：**C11 实时核心 + C++17 应用/平台层**。

---

## 1. 文件说明

| 文件 | 作用 | 读者 |
|---|---|---|
| [`ROADMAP.md`](ROADMAP.md) | 产品定位、用户、9 条需求映射、0.1-alpha–v2.0 路线图、关键功能规格、架构、QA、风险 | 产品/设计/工程/评审 |
| [`ALPHA-0.1.md`](ALPHA-0.1.md) | 0.1-alpha 双端技术预览：目标/非目标、范围、架构、里程碑、验收、风险 | 工程/产品/试用者 |
| [`AGENTS.md`](AGENTS.md) | 仓库级硬约束、C/C++ 规则、实时音频规则、安全不变量、DoD、Review 清单 | 所有贡献者 / coding agent |
| [`BACKLOG.md`](BACKLOG.md) | 任务级 backlog：ID、依赖、验收、测试、prompt seed | 工程 / coding agent |
| [`V0.2.md`](V0.2.md) | 0.2-alpha 实现与测试记录（对应 v1.2 核心） | 产品/工程/测试 |
| [`V0.3.md`](V0.3.md) | 0.3-alpha 实现与测试记录（对应 v1.3 核心） | 产品/工程/测试 |
| [`V0.4.md`](V0.4.md) | 0.4-alpha 实现与测试记录（对应 v1.4 核心） | 产品/工程/测试 |
| [`V0.5.md`](V0.5.md) | 0.5-alpha 实现与测试记录 | 产品/工程/测试 |
| [`V0.6.md`](V0.6.md) | 0.6-alpha 实现与测试记录 | 产品/工程/测试 |
| [`V0.7.md`](V0.7.md) | 0.7-alpha 实现与测试记录 | 产品/工程/测试 |
| [`V0.8.md`](V0.8.md) | 0.8-alpha 实时桥实现与测试记录 | 产品/工程/测试 |

建议在仓库中的放置方式：

```
audioscope/
├── AGENTS.md                  # 从本包复制到仓库根目录
├── docs/
│   └── plan/
│       ├── ROADMAP.md
│       ├── ALPHA-0.1.md
│       └── BACKLOG.md
└── ...
```

`AGENTS.md` 放根目录，coding agent 才会自动读取；其余放 `docs/plan/` 便于维护。

---

## 2. 一页摘要

**定位**：用声卡做音频段测量的维修工作台，不是通用示波器。

**双端基线**

C11 实时核心 + C++17 应用/平台层；macOS 用 CoreAudio/CoreGraphics，Windows 用 WASAPI/Direct2D；`extern "C"` 隔离，实时回调不碰 STL/异常/锁。

**三个赌注**

1. 安全：开源硬件前端 + 软件防呆。
2. 可信：逐设备/逐通道/逐增益档校准 + 置信度。
3. 工作流：维修模板 + 前后对比 + 可复现报告。

**四个硬前提**

1. 软件不能保护声卡；保护在模拟前端。
2. 爬厂商参数 ≠ 校准；必须实测。
3. Inst/Line 不能靠波形自动识别；必须显式模式 + 辅助判断。
4. 同一台声卡的 CH1/CH2 共享时钟；漂移测试必须跨设备。

**版本**

| 版本 | 主题 | 结果 |
|---|---|---|
| **0.1-alpha** | Dual Spike | macOS/Windows 跑通同一核心：2ch 示波器/FFT/单点校准/安全最小集 |
| **0.2-alpha** | Safe & Calibrated Core | 输入模式、SafetyGuard、逐档校准、信号检查、测量、JSON/HTML 报告 |
| **0.3-alpha** | Multi-channel & Phase Core | 多通道、XY/Lissajous、相位/增益、漂移仿真、频谱热力图 |
| **0.4-alpha** | Repair Workflow Core | wow/flutter、串扰、阻抗、方位角、前后对比 |
| **0.5-alpha** | Repair Measurement Suite | THD/THD+N/SNR/SINAD/SFDR、IMD、THD vs 电平 |
| **0.6-alpha** | Repair Plans & PDF | 维修模板、PDF 报告、THD vs 频率 |
| **0.7-alpha** | Precision Instrument UI | MasterLens 视觉标准、Web UI、7 页、暗/明主题 |
| **0.8-alpha** | Live Bridge | 本地 HTTP + WebSocket 实时桥、LIVE 状态、断线重连；WASAPI/DUT/多页 PDF 待续 |
| v1.2 | Safe & Calibrated | 双端不出事、可相信 |
| v1.3 | Multi-channel & Phase | 双端看得全、看得准 |
| v1.4 | Repair Workflow | 双端能干活、能交付 |
| v2.0 | Scale | 通用二进制、公证/安装包、国际化、设备库 |

---

## 3. 怎么用（人）

1. 先读 `ROADMAP.md` §0–§5，确认定位、双端范围和版本目标。
2. 再读 `ALPHA-0.1.md` 与 `V0.2.md`，确认 0.1-alpha 的边界与 0.2-alpha 的实现/测试记录。
3. 再读 §6/§7 中与你工作相关的规格（安全/校准/多通道/相位/热力图/报告/语言分层）。
4. 用 `BACKLOG.md` 的 B-alpha 作为 0.1-alpha 清单，`V0.2.md` 作为 0.2-alpha 验收记录，B0–B5 作为完整 v1.2 清单。
5. 每个任务按 `AGENTS.md` 的 DoD 验收；UI 变更必须双端截图。

**0.1-alpha 已完成；0.2-alpha 的测试记录见 [`V0.2.md`](V0.2.md)。**

- Day 1–2：`LANG-01` CMake/C ABI + `PLAT-02` 双端 CI。
- Day 3–4：`PLAT-10` 平台抽象 + `CH-07` 无锁化。
- Day 5：`PLAT-04a` WASAPI 设备枚举 + `PLAT-04b` Direct2D 空窗口。

**建议的前两周**

- 跑通“macOS 与 Windows 都能列出设备、打开窗口、看到输入波形”。
- 暂不做多通道、XY、热力图、报告；先把“同一核心双端可跑”立住。
- 第二周结束前必须有双端 CI 和 30 分钟 xrun=0 的烟测。

---

## 4. 怎么用（AI coding agent）

1. 把 `AGENTS.md` 复制到仓库根目录。
2. 把 `ROADMAP.md` / `ALPHA-0.1.md` / `BACKLOG.md` 放到 `docs/plan/`。
3. 每次只给一个任务卡，使用 `BACKLOG.md` 的 **Prompt seed**，加上：
   - 相关文件路径
   - 验收标准
   - 双端测试要求
   - “不要改无关代码 / 不要加依赖 / 不要削弱安全 / 不要用 STL 进音频回调”
4. Agent 必须先写失败测试，再实现；完成后跑 `cmake build` + `ctest`（双端）、ASan、双端截图。
5. 人工 review 按 `AGENTS.md` §15 清单；不通过就退回，不合并。

**不要做的事**

- 不要把 9 条需求一次性丢给 agent。
- 不要让 agent 同时改音频引擎、UI、报告。
- 不要接受“测试通过但行为变了”的 PR。
- 不要在没有验收标准的情况下开始编码。

### 第一个 prompt（可直接粘贴）

```text
You are working in the AudioScope repo (macOS 12+ arm64 + Windows 10/11 x64,
C11 realtime core + C++17 app/platform, zero third-party runtime deps).
Read AGENTS.md, docs/plan/ROADMAP.md, docs/plan/ALPHA-0.1.md,
and docs/plan/BACKLOG.md first.

Your first task is B-alpha: LANG-01 (CMake + core static lib + C ABI) only.
- Add CMake targets: audioscope_core (C11), audioscope_app (C++17), tests.
- Keep the existing Makefile working for v1.1 compatibility.
- Expose core APIs via extern "C" in headers includable from C and C++.
- Add CTest targets for existing DSP tests.
- Do not change DSP/audio/UI behavior.
- Do not add third-party dependencies.
- Explain how macOS and Windows CI will build and test it.

Before coding, restate the acceptance criteria in the PR description.
After coding, list the commands you ran on macOS/Windows and their results.
```

---

## 5. 关键决策（需要产品/维护者确认）

| # | 决策 | 默认建议 |
|---|---|---|
| 1 | 硬件前端是参考设计还是售卖套件？ | 先参考设计 + BOM；套件后置 |
| 2 | Windows alpha 用 WASAPI 共享模式还是独占模式？ | 先共享模式，保证兼容；独占模式进 v1.3 评估 |
| 3 | 报告 canonical 用 HTML 还是 PDF？ | HTML canonical；PDF 由系统生成 |
| 4 | 校准数据库是否接受社区提交？ | 接受，但必须来源/日期/审核 |
| 5 | 是否上 App Store？ | 暂不；沙盒限制设备/文件访问 |
| 6 | 是否做 opt-in 遥测？ | 默认不做；先靠 GitHub/问卷 |
| 7 | v1.3 目标 8ch 还是 16ch？ | 8ch 先稳定，16ch 作为上限 |
| 8 | 是否支持 Linux？ | 暂不承诺；核心保持可移植 |
| 9 | Windows alpha 用便携 ZIP 还是 MSI？ | 先 ZIP；v2.0 再做安装包 |
| 10 | C++17 是否足够？ | 是；C++20 只在双端工具链稳定后评估 |

这些决策不阻塞 0.1-alpha；但会影响 v1.2/v2.0 的范围，建议在 alpha 发布前定稿。

---

## 6. 文档维护规则

- 需求变更：先改 `ROADMAP.md` 的对应需求与验收，再改 `BACKLOG.md`。
- 任务拆分：只改 `BACKLOG.md`，保持 ID 与路线图对应。
- 架构变更：在 `ROADMAP.md` §7 增加 ADR 链接；不要在代码里“先斩后奏”。
- 每次发布：更新 §5 的退出标准、§14.E 变更记录、`BACKLOG.md` 状态。
- 文档与代码冲突：以 `ROADMAP.md` 的产品意图 + `AGENTS.md` 的约束为准；代码行为需补测试与说明。

---

## 7. 交付状态

| 文档 | 状态 |
|---|---|
| `ROADMAP.md` | v1.1，覆盖 9 条需求、4 个技术前提、0.1-alpha–v2.0、双端语言决策、关键规格、架构、QA、风险、vibe coding 手册 |
| `ALPHA-0.1.md` | v1.0，0.1-alpha 双端技术预览：范围、架构、里程碑、验收、风险 |
| `AGENTS.md` | v1.1，双端硬约束/C++ 规则/实时规则/安全不变量/DoD/Review 清单 |
| `BACKLOG.md` | v1.1，B-alpha + B0–B10 + 硬件，0.1-alpha 任务卡可直接执行 |

**下一步建议**：从 `BACKLOG.md` 的 B-alpha 开始，先做 `LANG-01` + `PLAT-02` + `PLAT-10` + `CH-07`，再进入 `PLAT-04a/b` 与 `ALPHA-01`。双端基础不牢，后面的 v1.2–v1.4 都会返工。
