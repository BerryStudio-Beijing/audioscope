# AudioScope by BerryStudio 产品路线图 0.3-alpha → v2.0

> **定位：声卡音频维修工作台**
> 项目：AudioScope by BerryStudio
> 代码基线：AudioScope 1.1（macOS 12+ / arm64 / 纯 C / Apache-2.0）
> 目标平台：macOS 12+ arm64 + Windows 10/11 x64
> 语言：C11 实时核心 + C++17 应用/平台层
> 文档版本：1.2 · 2026-11-07 · 状态：Draft for build
> 版本命名：**0.3-alpha** 是路线图 v1.3 “Multi-channel & Phase” 的核心子集；0.2-alpha 是 v1.2 核心，见 [`V0.2.md`](V0.2.md) / [`V0.3.md`](V0.3.md)。
> 配套文档：[`ALPHA-0.1.md`](ALPHA-0.1.md) · [`AGENTS.md`](AGENTS.md) · [`BACKLOG.md`](BACKLOG.md)

---

## 0. 一页结论

### 0.1 产品一句话

**AudioScope 是一台用声卡做音频段测量的维修工作台：在受保护的前端和逐档校准的前提下，让初学者和独立维修者完成“注入已知信号 → 定位故障 → 验证修复 → 输出可复现报告”的闭环。**

它不是“免费示波器”，也不是“通用音频分析仪”。它的价值是**把一次维修变成可执行、可验证、可交付的流程**。

### 0.2 三个赌注

1. **安全是入口，不是免责声明。** 用开源硬件前端 + 软件防呆，解决“初学者怕烧声卡”的核心恐惧。
2. **可信是差异。** dBFS 是事实，电压是推论；每一个电压数字都必须带校准来源和置信度。
3. **工作流是护城河。** 通用 FFT 谁都能做；把“吉他效果器 / 磁带机 / 调音台通道 / 耳放”的测试步骤、判据、报告模板做深，才是维修用户愿意留下的理由。

### 0.3 四个必须先纠正的技术前提

| # | 错误前提 | 正确做法 |
|---|---|---|
| 1 | 软件可以防止超大信号损坏声卡 | 软件只能**预警、锁存、静音**；保护必须发生在**模拟前端** |
| 2 | 爬厂商参数就能得到声卡电压量级 | 厂商参数不是校准；必须**逐设备、逐输入、逐增益档实测** |
| 3 | 靠波形能自动识别 Inst / Line | 它是**源阻抗与接口模式**问题；必须显式选择 + 辅助判断，绝不静默切换 |
| 4 | 同一台声卡的 CH1/CH2 能做时钟漂移测试 | 同一 ADC 共享时钟；Lissajous 测相位/极性/skew，**跨设备**才测 ppm 漂移 |

### 0.4 路线图一句话

| 版本 | 主题 | 一句话 |
|---|---|---|
| **0.1-alpha** | Dual Spike | 先证明“同一 C 核心 + C++ 应用层能同时跑 macOS/Windows”：2ch 示波器/FFT/单点校准/安全最小集 |
| **v1.2** | Safe & Calibrated | 先让测量“不出事、可相信”：安全前端、校准、输入模式、坐标交互、事件日志与报告 |
| **v1.3** | Multi-channel & Phase | 让测量“看得全、看得准”：N 通道、XY/Lissajous、通道匹配、跨设备漂移、真热力图 |
| **v1.4** | Repair Workflow | 让测量“能干活”：维修测试模板、wow & flutter、方位角、串扰、阻抗、前后对比 |
| **v2.0** | Scale | 让产品“能规模化”：通用二进制、签名公证、安装包、国际化、校准数据库 |

### 0.5 成功标准（6 个月内）

- **激活**：新用户从安装到完成第一次“已校准测量”≤ 10 分钟。
- **安全**：0 起可归因于软件的声卡损坏；100% 超量程事件被记录并阻断/警告。
- **可信**：100% 电压读数带置信度标签；重复 3 次 THD 离散 < 0.5 dB。
- **效率**：标准效果器测试 ≤ 10 分钟；报告生成 ≤ 5 秒。
- **社区**：20 台外部设备完成校准；10 份外部维修报告；5 名外部贡献者。
- **质量**：crash-free sessions > 99.5%；发布时 P0 缺陷为 0。

---

## 1. 产品定义

### 1.1 问题与机会

音频维修的学习门槛高：一台能用的示波器 + 信号发生器 + 失真仪，成本通常在数千到数万元；而几乎每个音频爱好者都有一块 24-bit 声卡。声卡的带宽（20 Hz–20 kHz）恰好覆盖音乐硬件维修的绝大多数需求。

现有替代品各有缺口：

- **REW / ARTA / AudioTester**：测量能力强，但不是维修工作流，缺少“接哪里、测什么、正常值是多少”的引导。
- **QA401 / Analog Discovery**：精度和带宽好，但价格高，面向工程师而非初学者。
- **声卡示波器小工具**：便宜，但普遍没有校准、没有安全前端、没有报告，读数不可信。
- **万用表 + 真示波器**：可信，但贵、笨重、不自动出报告。

机会在于：**把“安全 + 校准 + 维修流程 + 报告”做成一个低成本的垂直工具**，而不是再做一个 FFT 绘图器。

### 1.2 目标用户

| 画像 | 场景 | 痛点 | 想要的结果 |
|---|---|---|---|
| **初学者 / 爱好者（小林）** | 有一块 Scarlett / UMC，想修自己的效果器 | 怕烧声卡；不知道先测什么；看不懂相位图 | 跟着步骤做，别让我弄坏设备，告诉我正不正常 |
| **独立维修技师（老周）** | 工作台有 DMM 和老示波器，接活修踏板/磁带机/调音台 | 重复测量慢；报告靠手写；多通道设备测不全 | 快速、可复现、能交给客户的报告 |
| **音频硬件开发者（Aya）** | 打样前级/合成器，需要 loopback 验证 | 缺少自动化回归；多通道/相位问题难定位 | 批量测试、前后对比、异常可追溯 |
| **教学者 / 内容创作者（Lan）** | 拍维修教学、做课程演示 | 真机演示危险、成本高；学生看不到“坏”的样子 | 安全的仿真训练 + 真实案例 |

### 1.3 非目标用户

- 需要测市电、电源纹波、DC 偏置、RF、超声、母线/高压的电源工程师。
- 需要 100 MHz 以上带宽、协议解码的通用示波器用户。
- 需要声学测量、房间校准、SPL 计权的音响工程师。
- 需要多轨录音、混音、母带处理的 DAW 用户。

### 1.4 Jobs To Be Done

1. **安全作业**：当我接入一个未知来源时，我想在损坏声卡之前被拦住。
2. **故障定位**：当我怀疑设备有问题时，我想注入已知信号并看到它在哪一级失真/丢失。
3. **修复验证**：当我修完之后，我想证明它真的变好了，而不是“听起来还行”。
4. **交付报告**：当我把设备交还客户时，我想给出一份有数据、有结论、可复现的报告。
5. **学习诊断**：当我不确定相位/时钟/串扰的区别时，我想在仿真里看到“健康”和“故障”的差别。

### 1.5 价值主张

> 对需要维修音频设备、但不想先买一台昂贵示波器的初学者和小型维修者，**AudioScope** 是一台基于声卡的音频维修工作台。它用**受保护的前端、逐档校准、引导式测试和前后对比报告**，把“能看波形”变成“能下结论”。与通用频谱分析软件不同，它**明确测量边界**，并把**防止损坏设备**作为产品的一部分。

### 1.6 竞争与替代

| 方案 | 优势 | 缺口 | AudioScope 的差异 |
|---|---|---|---|
| REW | 免费、功能强、声学/电学都能测 | 非维修流程；Java UI；无安全前端 | 引导式维修 + 报告 + 安全 |
| ARTA / AudioTester | Windows 测量成熟 | 收费；无维修模板；无跨平台 | 模板 + 前后对比 + macOS 原生 |
| QA401 | 精度高、硬件成熟 | 价格高；面向工程师 | 低成本；初学者可用 |
| Analog Discovery | 带宽高、通用 | 音频精度/噪声不专精；价格中高 | 音频段专用 + 校准 + 报告 |
| 声卡示波器小工具 | 便宜、轻量 | 无校准、无保护、无报告 | 可信度 + 安全 + 工作流 |
| DMM + 真示波器 | 可信、带宽高 | 贵、笨重、不自动 | 互补，不替代 |

**差异化排序**：安全 > 可信 > 工作流 > 报告 > 原生 macOS / 零依赖 > 价格。

### 1.7 产品原则

1. **安全先于功能。** 任何会鼓励用户直连危险信号的功能，默认不做。
2. **dBFS 是事实，电压是推论。** 没有校准来源的电压不显示，或明确标注“假设”。
3. **不静默自动切换。** 输入模式、量程、通道映射的变更必须可见、可撤销。
4. **每个数字都有出处。** 设备、校准、采样率、增益档、置信度、时间戳。
5. **工作流优先于旋钮。** 先回答“用户在修什么”，再决定提供哪些测量。
6. **本地优先。** 默认不联网、不上传、不需要账号。
7. **核心零第三方依赖。** 便携 C 核心；平台层隔离；系统框架优先。
8. **每个功能都能仿真验证。** 没有已知答案的测试，不算完成。
9. **初学者安全，专家高效。** 默认引导，但允许一键进入自由模式。
10. **双端优先，共享核心。** macOS 与 Windows 从 0.1-alpha 起同步；不为单端做无法移植的架构。

### 1.8 成功指标

| 层级 | 指标 | 目标 | 采集方式 |
|---|---|---|---|
| 北极星 | 每周完成的“有结论维修报告”数 | 持续增长 | 本地报告计数（用户自愿导出） |
| 激活 | 首次校准 + 首次测量耗时 | ≤ 10 min | 引导流程埋点（本地，匿名可选） |
| 安全 | 超量程事件阻断率 | 100% | 事件日志 |
| 可信 | 电压读数带置信度比例 | 100% | UI/报告审计 |
| 重复性 | 3 次 THD 测量离散 | < 0.5 dB | 自检/报告 |
| 双端一致 | 同一 WAV 在 macOS/Windows 的频率/幅度差 | < 0.05% / < 0.5 dB | 黄金文件/CI |
| 效率 | 标准效果器模板耗时 | ≤ 10 min | 模板计时 |
| 质量 | crash-free sessions | > 99.5% | 本地崩溃日志（可选上传） |
| 社区 | 外部校准设备 / 报告 / 贡献者 | 20 / 10 / 5 | GitHub / 问卷 |

---

## 2. 范围与边界

### 2.1 In Scope

- 音频频段：标称 20 Hz–20 kHz；高采样率下可选扩展到 40 kHz（96k）或 80 kHz（192k），但默认按音频段呈现。
- 输入类型：Line（+4 dBu / −10 dBV）、Instrument（−20 dBV）、Mic（−60 dBu）、10:1 Probe、Speaker via pad（仅配合前端）。
- 通道：v1.2 立体声；v1.3 起 4–8 通道，目标 16。
- 测量：时域、FFT、频谱热力图、Bode、THD/THD+N/SNR/SINAD/ENOB/SFDR、串扰、阻抗、IMD、wow & flutter、方位角、通道匹配、时钟漂移。
- 信号源：正弦、方波、三角、锯齿、白噪、粉噪、双音、扫频、测试文件。
- 工作流：引导式维修模板、前后对比、事件日志、报告。
- 平台：0.1-alpha 起双端；macOS 12+ arm64 + Windows 10/11 x64；v2.0 加通用二进制、签名/公证、安装包。
- 语言：C11 实时核心 + C++17 应用/平台层（见 §7.0）。
- 数据：本地文件；默认不联网。

### 2.2 Out of Scope（明确不承诺）

- DC 电压、电源纹波、偏置、母线、市电、高压。
- RF、超声、> 80 kHz 的测量。
- 通用示波器、协议解码、逻辑分析。
- 声学测量、房间校准、SPL、麦克风校准。
- DAW、录音、混音、母带。
- 云同步、账号体系、在线 AI 诊断。
- 直接连接喇叭输出、市电、未衰减的胆机输出。
- “软件保证不损坏声卡”——**永远不写这句话**。

### 2.3 安全边界

| 场景 | 允许？ | 条件 |
|---|---|---|
| Line / Inst / Mic 信号 | ✅ | 使用正确输入模式；Mic 关闭 phantom |
| 效果器 / 前级 / 合成器输出 | ✅ | 先预检；峰值不超过前端量程 |
| 喇叭输出 / 胆机输出 | ⚠️ | 必须假负载 + 衰减器 + 前端；否则禁止 |
| 市电 / 电源初级 | ❌ | 任何情况下不接 |
| 48V phantom 直入线路输入 | ❌ | 必须 phantom blocker 或确认关闭 |
| 未知电压源 | ⚠️ | 先用万用表测；否则只允许最低量程并预检 |

### 2.4 测量边界

- 声卡输入通常 **AC 耦合**；UI 里的 “DC” 只能表示“不去均值”，不能测真实 DC。
- 受 Nyquist 限制；48k 采样下有效分析上限 24 kHz。
- 声卡自身的 THD+N / 噪声底是测量下限；报告必须显示“测量系统残差”。
- 输入阻抗会加载被测电路；高阻源必须提示。
- 声卡时钟精度未知；绝对频率读数受声卡晶振影响，跨设备比较需 ppm 测试。

---

## 3. 现状基线（v1.1 审计）

### 3.1 已有能力

- 四个页签：波形/时域、频谱/FFT、频率响应、闭环测试。
- 信号源：正弦/方波/三角/锯齿/白噪/粉噪/双音/扫频 + 文件播放。
- FFT 1k–128k、6 种窗、平均、峰值保持、瀑布图。
- Bode 幅频/相频、扫频、CSV 导出。
- 闭环测试：延迟、频响、THD+N、噪声、上升沿、JSON/CSV/WAV 报告、规则式建议。
- 主题系统、快捷键、自检 30 项 + 单元测试 23 项、离屏截图。
- Apache-2.0；纯 C；无第三方依赖。

### 3.2 代码审计发现

| 发现 | 证据 | 影响 |
|---|---|---|
| 音频引擎是立体声 | `ASEngine.ring` 为 interleaved stereo，`in_peak[2]` | 无法满足多声道需求 |
| CH1/CH2 共享 V/div 与 offset | `App` 只有单个 `volt_div` / `v_off` | 无法独立缩放两通道 |
| 滚轮缩放是离散档位、全局生效 | `main.m` 的 `scrollWheel` 只分 Ctrl/非 Ctrl | 无光标锚定、无平移、无捏合 |
| 无 XY / Lissajous | 全仓库只有 Bode 相位 | 无法做相位/极性/通道匹配 |
| 无 clock drift | 无 ppm / 相位-时间逻辑 | 无法定位跨设备时钟问题 |
| 瀑布图是 RGBA 位图 | `Waterfall { unsigned char *rgba; }` | 无法做光标读数、CSV 导出、真热力图 |
| 削波只有实时标志 | `tstat[].clipped` + CLIP | 无锁存、无自动静音、无事件记录 |
| “DC” 只是去均值 | `app.c` 的 `coupling==0` 分支 | 标签误导；硬件仍 AC 耦合 |
| 音频回调内有互斥锁 | `ring_write()` / `render_output()` 使用 `pthread_mutex_lock` | 有优先级反转/掉音风险；需无锁化 |
| 校准是单值 | `volts_at_fs` + `ch_gain[2]` | 无法逐通道/逐增益档校准 |
| 报告 schema v1 | `report.h` 字段有限 | 缺 provenance、事件、对比、模板 |
| 无 CI | 仓库无 `.github/` | 回归靠本地 |
| 仅 macOS arm64 | Makefile / README | 市场覆盖受限；0.1-alpha 必须解决 |

### 3.3 技术债

1. `App` 单结构体 + `view.c` 45 KB，职责过重；新功能会继续堆高耦合。
2. 实时音频路径存在锁；多通道重构前必须先解决。
3. 设置是 key=value；校准/报告/模板需要版本化 JSON。
4. 测试以 DSP 数值为主；缺 UI 快照、报告 schema、故障注入、跨设备集成测试。
5. 无崩溃日志/会话日志；现场问题难复现。
6. 无国际化基础设施；文案硬编码中文。

### 3.4 保留 / 重构 / 新增

| 类别 | 内容 |
|---|---|
| **保留** | DSP 核心、FFT、报告规则、闭环流程、主题令牌、自绘控件、测试基线 |
| **重构** | 音频引擎（无锁、N 通道）、App 状态拆分、设置/报告 schema、view 拆分为多视图 |
| **新增** | SafetyGuard、校准模型、单位系统、输入模式、XY/Phase、Spectrogram 矩阵、事件日志、测试模板、前端硬件、C++ 应用层、双端平台抽象、Windows WASAPI/D2D |

---

## 4. 需求总表

### 4.1 你的 9 条需求 → 产品需求

| # | 你的原始需求 | 产品解读 | 需求 ID | 版本 |
|---|---|---|---|---|
| 1 | 基于声卡采样率，仅音频段 | 明确带宽边界，不做通用示波器 | LIM-01/02、UI-07 | v1.2 |
| 2 | 超大信号进声卡的前端处理 | 模拟前端 + 软件防呆，软件不承诺保护 | SAFE-01…07 | v1.2 + FE |
| 3 | 声卡参数自适应 / 爬厂商 / 自定义 | 能力探测 + 逐档校准 + 精选 DB + 置信度 | CAL-01…09 | v1.2 / v2.0 |
| 4 | DI / Inst 识别与切换 | 显式模式 + 信号检查助手 + 双阻抗测源 | INP-01…05 | v1.2 / v1.3 |
| 5 | 横纵轴滚轮缩放 | 光标锚定、轴命中、平移/捏合、独立通道量程 | UI-01…06 | v1.2 |
| 6 | 多声道输入与识别 | N 通道引擎、通道映射、聚合设备 | CH-01…06 | v1.3 |
| 7 | Lissajous / 时钟 / 相位 | 同设备通道匹配；跨设备 ppm；仿真训练 | PH-01…08 | v1.3 / v1.4 |
| 8 | 热力图与频谱 | dB 矩阵热力图、余晖、光标、THD 热图 | SPEC-01…07 | v1.3 / v1.4 |
| 9 | 日志与报告 | 事件日志、报告 v2、PDF/HTML、前后对比 | RPT-01…08 | v1.2 / v1.4 |

### 4.2 功能需求

优先级：**P0** = 安全/发布阻塞；**P1** = 差异化；**P2** = 规模化。

#### Epic A — 安全（SAFE）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| SAFE-01 | 开源硬件前端参考设计（2ch，1 MΩ，±1/±10/±50 V，AC/DC，10:1，phantom blocker，限流输出） | P0 | v1.2 + FE | 原理图/BOM/装配说明可复现；±50 V 输入不损坏；20 Hz–20 kHz 幅频 ±0.1 dB |
| SAFE-02 | SafetyGuard 状态机（IDLE→PRECHECK→ARMED→RUNNING→FAULT） | P0 | v1.2 | 未完成预检不能进入 RUNNING；FAULT 必须人工复位 |
| SAFE-03 | Clip 锁存、平顶检测、自动静音、事件记录 | P0 | v1.2 | 连续 3 样本 ≥0.999 或平顶 5 样本触发；100 ms 内静音；事件含峰值/通道/时间 |
| SAFE-04 | 量程建议与超量程阻断 | P0 | v1.2 | 预估电压超过量程时禁止 Arm；建议必须给出理由 |
| SAFE-05 | 输出限幅、启动静音、淡入淡出 | P0 | v1.2 | 默认最大 −20 dBFS；启动 200 ms 静音；切换无爆音 |
| SAFE-06 | 首次使用安全引导与免责声明 | P0 | v1.2 | 必须确认“不接市电/喇叭直连/phantom”后才可测量 |
| SAFE-07 | 安全故障注入测试 | P0 | v1.2 | 注入 10 Vpp、DC、平顶、NaN/Inf、快速增益变化，全部被捕获且不崩溃 |

#### Epic B — 校准与单位（CAL）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| CAL-01 | CoreAudio 设备能力探测（UID、传输类型、采样率、通道数、流配置、延迟） | P0 | v1.2 | 3 款接口识别正确；热插拔不崩溃 |
| CAL-02 | DeviceProfile / Calibration 数据模型（版本化 JSON） | P0 | v1.2 | schema 校验通过；旧版本可迁移 |
| CAL-03 | 校准向导（万用表 / 已知参考 / loopback） | P0 | v1.2 | 1 kHz −20 dBFS 校准后误差 < 0.1 dB |
| CAL-04 | 逐通道、逐增益档校准与失效提醒 | P0 | v1.2 | 切设备/采样率/增益档时提示失效；不得静默沿用 |
| CAL-05 | 单位系统 dBFS / dBu / dBV / Vrms / Vpp | P0 | v1.2 | 换算误差 < 0.01%；UI 同时显示 dBFS 与电压 |
| CAL-06 | 输入阻抗与加载误差提示 | P1 | v1.2 | 高阻源在 10 kΩ 输入下给出 > 1 dB 衰减警告 |
| CAL-07 | 置信度标签（实测/厂商典型/假设） | P0 | v1.2 | 100% 电压读数带标签；报告包含来源 |
| CAL-08 | 精选设备参数库导入/导出/贡献 | P2 | v2.0 | 每条记录含来源 URL、日期、置信度；无来源不入库 |
| CAL-09 | 校准完整性校验（哈希/时间/设备绑定） | P1 | v1.2 | 篡改或设备不匹配时拒绝使用 |

#### Epic C — 输入模式（INP）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| INP-01 | 输入模式预设（Line Pro / Line Consumer / DI / Inst / Mic / Probe / Speaker-via-pad） | P0 | v1.2 | 每种模式有标称电平、阻抗、最大输入、安全上限 |
| INP-02 | 信号检查助手（峰值/RMS/波峰因数/频谱倾斜/工频/噪声底） | P0 | v1.2 | 给出建议 + 理由 + 置信度；必须用户确认 |
| INP-03 | 双阻抗源阻抗测试（1 MΩ / 10 kΩ） | P1 | v1.3 | 估算误差 < 20%；高阻源建议 Inst 的准确率 > 90% |
| INP-04 | Phantom / TS-TRS / 接地环路检查清单 | P0 | v1.2 | 未确认 phantom 关闭时禁止 Mic 模式测量 |
| INP-05 | 禁止静默自动切换的不变量 | P0 | v1.2 | 代码审查 + 测试保证：无任何路径自动改输入模式 |

#### Epic D — 交互与坐标（UI）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| UI-01 | 光标锚定缩放（X/Y） | P0 | v1.2 | 缩放 10 次后锚点漂移 < 1 px |
| UI-02 | 平移、捏合、框选放大、双击自适应、右键复位 | P0 | v1.2 | 触控板/鼠标均可；不误触控件 |
| UI-03 | CH1/CH2 独立 V/div、offset、单位 | P0 | v1.2 | 可同时 0.1 V/div 与 2 V/div；可链接 |
| UI-04 | 双光标与 ΔT/ΔV/1/ΔT 读数 | P1 | v1.2 | 读数与采样点一致；可吸附过零/峰值 |
| UI-05 | 每页签视图状态持久化 | P1 | v1.2 | 重启后恢复缩放/通道/页签 |
| UI-06 | 键盘导航、焦点环、对比度、减少动态 | P1 | v1.2 | 全功能可键盘操作；WCAG AA 对比度 |
| UI-07 | 带宽/耦合/DC 边界提示 | P0 | v1.2 | UI 明示 Nyquist、AC 耦合、“DC”仅表示不去均值 |

#### Epic E — 多声道与实时性（CH）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| CH-01 | N 通道环形缓冲（4–8，目标 16） | P0 | v1.3 | 8ch/48k 连续 10 分钟无 xrun；CPU < 10% M1 |
| CH-02 | 通道元数据、映射、名称、颜色 | P0 | v1.3 | 4ch 接口顺序/标签正确；支持非交错缓冲 |
| CH-03 | 每通道 arm/solo/mute/meter | P0 | v1.3 | 任意通道可独立启用；solo 不影响采集 |
| CH-04 | 任意通道触发、math、XY 对 | P0 | v1.3 | 任意两通道可做 A+B/A−B/XY |
| CH-05 | 聚合设备识别与时钟主设备说明 | P1 | v1.3 | 识别 Aggregate；显示子设备/时钟源 |
| CH-06 | 热插拔、采样率切换、设备丢失恢复 | P0 | v1.3 | 拔插 20 次不崩溃；自动重建并提示 |
| CH-07 | 音频回调无锁化（SPSC ring + 原子配置） | P0 | v0.1-alpha | 回调内 0 malloc/0 mutex/0 文件 IO；TSan 无数据竞争 |

#### Epic F — 相位、时钟、Lissajous（PH）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| PH-01 | XY / Lissajous 视图（余晖、等量程、网格） | P0 | v1.3 | 1:1/2:1 图形正确；余晖可调 |
| PH-02 | 椭圆拟合、相位/增益/极性读数 | P0 | v1.3 | 注入 45° 读 ±2°；增益比 ±0.1 dB |
| PH-03 | 通道匹配测试（增益差/相位差/skew/串扰） | P0 | v1.3 | 同源分线；报告给出 20 Hz–20 kHz 表 |
| PH-04 | 跨设备时钟漂移测试（ppm、滑移周期） | P0 | v1.3 | 注入 100 ppm 读 ±5 ppm；明确“同设备不可测” |
| PH-05 | 相位斜率/延迟区分 | P1 | v1.3 | 固定相移与延迟可区分；延迟误差 < 1 sample |
| PH-06 | 仿真/训练模式（注入相位/漂移/噪声/串扰） | P1 | v1.3 | 已知答案可复现；训练场景 ≥ 6 个 |
| PH-07 | Wow & flutter / 带速漂移 | P1 | v1.4 | 3 kHz 测试带；IEC/DIN 加权；重复性 < 5% |
| PH-08 | 磁头方位角（L/R 高频相位差） | P1 | v1.4 | 10 kHz 相位差可读；给出调整方向 |

#### Epic G — 频谱与热力图（SPEC）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| SPEC-01 | Spectrogram dB 矩阵（时间 × 频率） | P0 | v1.3 | 1 kHz 落在 ±1 bin；60 s 历史 |
| SPEC-02 | 余晖/衰减/max hold/min hold | P0 | v1.3 | 衰减可调；冻结后可读 |
| SPEC-03 | 光标读数、缩放、平移 | P1 | v1.3 | 读数与 FFT 一致；可导出当前点 |
| SPEC-04 | 色标、gamma、色盲友好配色 | P1 | v1.3 | 单调亮度；提供 viridis/magma/grayscale |
| SPEC-05 | PNG/CSV 导出 | P1 | v1.3 | 导出含时间/频率/dB/设置元数据 |
| SPEC-06 | THD vs 频率 vs 电平热力图 | P1 | v1.4 | 可定位削波/失真起点；导出 CSV |
| SPEC-07 | 长时监控/老化测试热力图 | P1 | v1.4 | 连续 8 小时；事件标记；内存有界 |

#### Epic H — 日志与报告（RPT）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| RPT-01 | 会话事件日志 JSONL | P0 | v1.2 | 设备/采样率/xrun/clip/模式/校准/备注全记录 |
| RPT-02 | 报告 schema v2 + provenance | P0 | v1.2 | 含 app/OS/设备 UID/校准 ID/设置/限值/哈希 |
| RPT-03 | HTML 报告 + PDF 导出 | P0 | v1.2 | 离线可开；图内嵌；PDF 由系统生成 |
| RPT-04 | 维修前后对比 | P0 | v1.4 | 两次运行差值表 + 曲线叠加 + 结论 |
| RPT-05 | 测试模板引擎 | P0 | v1.4 | JSON 定义步骤/信号/判据/建议 |
| RPT-06 | 可配置通过/失败限值 | P0 | v1.2 | 限值可保存/复用；报告显示判定依据 |
| RPT-07 | 重复性与不确定度 | P1 | v1.2 | 3 次重复；显示均值/标准差/残差 |
| RPT-08 | 案例文件夹与附件 | P1 | v1.4 | 每次维修一个文件夹；支持备注/截图 |

#### Epic I — 维修工作流（REP）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| REP-01 | 模板引擎与步骤执行器 | P0 | v1.4 | 支持顺序/分支/重试；失败可定位到步骤 |
| REP-02 | 吉他效果器模板 | P0 | v1.4 | bypass/engage 对比；增益/THD/噪声/削波 |
| REP-03 | 磁带机模板 | P1 | v1.4 | 带速/wow&flutter/方位角/频响/串扰 |
| REP-04 | 调音台通道模板 | P1 | v1.4 | 增益/频响/THD/串扰/phantom 检查 |
| REP-05 | 耳放模板 | P1 | v1.4 | 负载电阻；频响；THD vs 电平；输出阻抗 |
| REP-06 | 合成器/线路输出模板 | P2 | v1.4 | 噪声/频响/削波/串扰 |
| REP-07 | 串扰/立体声分离度 | P0 | v1.4 | 20 Hz–20 kHz；−dB 读数；报告 |
| REP-08 | 阻抗测量（串联电阻法） | P0 | v1.4 | 20 Hz–20 kHz；误差 < 10% |
| REP-09 | IMD（SMPTE/DIN）与 THD vs 电平 | P0 | v1.4 | 与已知参考一致；报告 |
| REP-10 | 老化/长时测试 | P1 | v1.4 | 可设时长；事件触发；自动报告 |

#### Epic J — 平台与交付（PLAT）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| PLAT-01 | 通用二进制（arm64 + x86_64） | P1 | v2.0 | Rosetta/Intel 均可启动 |
| PLAT-02 | CI（macOS + Windows 构建/测试/截图/ASan） | P0 | v0.1-alpha | 每个 PR 双端必跑；失败阻断合并 |
| PLAT-03 | 签名/公证 DMG + Windows 安装包签名 | P1 | v2.0 | Gatekeeper/SmartScreen 无警告 |
| PLAT-04 | Windows 10/11 x64 平台层（WASAPI 共享 + Direct2D + Win32） | P0 | v0.1-alpha | alpha：2ch/48k/示波器/FFT；v1.x 与 macOS 功能对等 |
| PLAT-05 | 中英文国际化 | P1 | v2.0 | 所有 UI/报告字符串可切换 |
| PLAT-06 | 设置 schema 迁移（settings.ini → JSON） | P0 | v0.1-alpha | 旧版设置可导入；失败可回滚 |
| PLAT-07 | 性能预算与回归 | P0 | v0.1-alpha | 启动 < 0.2 s；UI 30 Hz；回调 0 分配 |
| PLAT-08 | 崩溃日志、日志轮转、隐私开关 | P1 | v1.2 | 默认本地；上传需显式同意 |
| PLAT-09 | C11 核心 + C++17 应用层 + 稳定 C ABI + CMake 双端构建 | P0 | v0.1-alpha | 双端同一核心；C ABI 无 STL/异常穿透；CI 绿 |
| PLAT-10 | 平台抽象（AsAudio/AsCanvas/AsWindow/AsFiles/AsTime） | P0 | v0.1-alpha | macOS/Windows 各一套实现；核心零平台依赖 |

#### Epic K — 测量边界与诚实呈现（LIM）

| ID | 需求 | P | 版本 | 验收标准 |
|---|---|---|---|---|
| LIM-01 | 明示 Nyquist、有效带宽、AC 耦合、无 DC/超声/RF | P0 | v1.2 | UI/帮助/README 均可见；与当前采样率联动 |
| LIM-02 | 修正“DC”标签，禁止暗示可测真实 DC | P0 | v1.2 | 选项改为“Raw（不平移）”；帮助说明硬件 AC 耦合 |

### 4.3 非功能需求

#### 精度预算

| 项目 | 目标 | 条件 |
|---|---|---|
| 频率 | ±0.01%（100 ppm） | 1 s 窗口，信噪比 > 40 dB |
| 幅度（已校准） | ±0.1 dB | 1 kHz，−20 dBFS |
| 幅度（仅 datasheet） | ±1 dB | 明确标注“典型” |
| 相位 | ±1°（>100 Hz）；±3°（20 Hz） | 同源分线，已校准 |
| THD/THD+N | ±1 dB 相对 | 高于测量系统残差 10 dB |
| 时钟漂移 | ±5 ppm | 60 s，共同参考 |
| 通道 skew | ±1 sample | 冲击/方波 |
| 电压 | ±1%（已校准）；±10%（假设 FS） | 1 kHz 正弦 |
| Spectrogram | ±1 bin | 1 kHz |

#### 性能预算

- 启动 < 0.2 s（M1，冷启动）。
- UI 30 Hz，交互到重绘 < 33 ms。
- 音频回调：0 malloc / 0 free / 0 mutex / 0 文件 IO / 0 ObjC。
- 8ch / 48k / 128 帧：CPU < 10%（M1）。
- 128k FFT：< 50 ms。
- 报告生成：< 5 s（含图）。
- 内存：稳态 < 200 MB；长时监控有界。

#### 可靠性

- 设备热插拔 20 次不崩溃。
- 采样率切换 10 次不崩溃。
- 连续运行 8 小时无内存增长（< 5%）。
- 异常输入（NaN/Inf/超大文件）不崩溃。
- 所有用户数据写入采用临时文件 + 原子重命名。

#### 隐私与安全

- 默认不联网；无账号；无遥测。
- 崩溃日志/报告上传必须显式 opt-in。
- 报告不含绝对路径/用户名，除非用户填写。
- 校准/报告文件权限 0600。
- 依赖仅系统框架；无第三方供应链。

#### 可维护性

- 核心 C11，`-Wall -Wextra` 零警告。
- 每个模块有公开头文件与单元测试。
- 报告/设置/校准 schema 版本化。
- 关键路径有注释说明实时约束。
- 新平台通过接口隔离，不污染核心。

#### 可访问性

- 全功能键盘可达；焦点环可见。
- 对比度 WCAG AA；不只靠颜色传达状态。
- 尊重“减少动态”设置。
- 字号可读；数字使用等宽。

---

## 5. 版本路线图

### 5.1 总览

| 版本 | 主题 | 周期（估） | 核心范围 | 退出标准 |
|---|---|---|---|---|
| **0.1-alpha** | Dual Spike | 6–8 周 | PLAT-09/10/04/02/06/07、CH-07、ALPHA-01…09 | 双端 2ch/48k 示波器/FFT/单点校准/安全最小集；CI 绿；可分发 |
| v1.2 | Safe & Calibrated | 6 周 | SAFE、CAL、INP、UI、LIM、RPT-01/02/03/06/07、PLAT-08 | 双端安全故障注入全过；校准误差达标；报告可离线；CI 绿 |
| v1.3 | Multi-channel & Phase | 6 周 | CH-01…06、PH-01…06、SPEC-01…05 | 双端 4/8ch 稳定；XY/漂移达标；热力图可读 |
| v1.4 | Repair Workflow | 6 周 | REP-01…10、PH-07/08、SPEC-06/07、RPT-04/05/08、INP-03 | 双端 3 个模板可用；前后对比报告；wow&flutter |
| v2.0 | Scale | 8–12 周 | PLAT-01/03/05、CAL-08、安装包 | 通用二进制；公证/签名；双语；设备库 |

> 估算假设：1 名主程 + 1 名兼职设计/测试 + AI coding 辅助；硬件前端并行。Windows 平台经验是 0.1-alpha 的关键变量。实际排期按团队规模调整。
> 0.1-alpha 的详细构想、里程碑与验收见 [`ALPHA-0.1.md`](ALPHA-0.1.md)。

### 5.2 0.1-alpha — Dual Spike

**目标**：先证明“同一 C 核心 + C++ 应用层”能同时跑 macOS/Windows，而不是先堆功能。

**范围**

- 构建：PLAT-09（C/C++ 分层 + C ABI + CMake 双端）、PLAT-02（双端 CI）。
- 平台：PLAT-10（平台抽象）、PLAT-04（Windows WASAPI/D2D 最小实现）。
- 实时：CH-07（音频回调无锁化，立体声）。
- 设置：PLAT-06（settings.ini → JSON）、PLAT-07（性能基线）。
- 功能：ALPHA-01…09（示波器、FFT、信号源、单点校准、安全最小集、保存、打包、HIL、文档）。

**用户故事**

1. 作为 Windows 维修爱好者，我下载便携 ZIP，选接口，接效果器，能看到 1 kHz 波形和 FFT。
2. 作为 macOS 用户，我用同一套 UI 和核心，确认没有退化。
3. 作为开发者，我只改一次核心，两端同时受益。
4. 作为产品负责人，我判断 Windows 是否值得继续投入。

**退出标准**

- macOS 13/14 + windows-2022 CI 绿。
- 双端 2ch/48k/128 帧，30 分钟 xrun=0（每端 2 款接口）。
- 1 kHz 正弦：频率 ±0.1%、幅度 ±0.5 dB（单点校准后）。
- 同一 WAV 双端分析：频率差 < 0.05%、幅度差 < 0.5 dB。
- clip 锁存 + 100 ms 内静音；预检阻断。
- macOS `.app`/DMG + Windows 便携 ZIP 可分发。
- 已知问题、alpha 免责声明、反馈模板齐备。

**演示脚本**

1. Windows 10/11 解压 ZIP，插入 Scarlett 2i2，启动。
2. 选择设备，看到输入波形；打开 FFT，看到 1 kHz 峰值。
3. 输入单点校准值，显示“实测/假设”。
4. 接入效果器，测 1 kHz 幅度/频率。
5. 故意削波，应用静音并记录事件。
6. 保存 WAV/CSV/JSON；在 macOS 上用同一文件复测，对比读数。

### 5.3 v1.2 — Safe & Calibrated（发布名 0.2-alpha）

**目标**：在 0.1-alpha 已验证的双端架构上，先让测量“不出事、可相信”。

**前提**：0.1-alpha 已通过；核心/应用边界稳定；两端音频 IO 可用。

**范围**

- 安全：SAFE-01…07。
- 校准：CAL-01…07、09（含输入阻抗/加载提示）。
- 输入：INP-01、02、04、05。
- 交互：UI-01…06、UI-07。
- 边界：LIM-01、LIM-02。
- 报告：RPT-01、02、03、06、07。
- 平台：PLAT-08（崩溃日志/隐私）+ 双端 CI 回归。

**用户故事**

1. 作为初学者，我接入未知设备时，应用先问我源类型和预估电压，并告诉我该用哪个量程。
2. 作为初学者，我校准一次后，之后每次看到电压都知道它是“实测”还是“假设”。
3. 作为维修者，我削波时应用自动静音并记录事件，而不是继续采集垃圾数据。
4. 作为维修者，我用滚轮放大时，光标下的波形点不会跑掉。
5. 作为维修者，我能导出带设备/校准/设置的 HTML 报告，离线发给客户。

**退出标准**

- 安全故障注入 100% 捕获；无崩溃。
- 校准后 1 kHz −20 dBFS 幅度误差 < 0.1 dB。
- 缩放锚点漂移 < 1 px。
- 报告在无网络环境可打开，所有数字有 provenance。
- CI 在 macOS 13/14 + windows-2022 上绿；ASan/UBSan 干净。
- 双端 1 kHz 读数一致（频率差 < 0.05%，幅度差 < 0.5 dB）。
- 回归检查：音频回调路径无 `pthread_mutex`。

**演示脚本**

1. 在 macOS 插入 Scarlett 2i2，选择 Inst 模式。
2. 应用提示“未检测到前端，最大输入未知；建议先用万用表确认”。
3. 完成校准，显示“实测本机”。
4. 接入效果器，跑 1 kHz，显示 0.0 dB / 0.3% THD。
5. 故意加大增益到削波，应用在 100 ms 内静音并弹出事件。
6. 导出 HTML 报告。
7. 在 Windows 上打开同一报告/同一 WAV，确认读数一致。

### 5.4 v1.3 — Multi-channel & Phase（发布名 0.3-alpha）

**目标**：在双端上让测量“看得全、看得准”。

**范围**

- 多通道：CH-01…06。
- 相位/时钟：PH-01…06。
- 频谱：SPEC-01…05。
- 校准：复用 v1.2 的 CAL-06（输入阻抗/加载提示）。

**用户故事**

1. 作为维修者，我接 4 通道接口，能同时看 4 路电平和任意两路 XY。
2. 作为维修者，我用 Y 分线测两通道增益差/相位差，判断是通道故障还是信号本身。
3. 作为开发者，我用两台设备测 clock drift，知道每 0.2 s 会滑一个采样。
4. 作为初学者，我在仿真里看到“反相”“90° 相移”“100 ppm 漂移”的 Lissajous 差别。

**退出标准**

- 8ch/48k 连续 10 分钟无 xrun。
- 注入 45° 读 ±2°；注入 100 ppm 读 ±5 ppm。
- 4ch 标签/顺序正确；热插拔 20 次不崩溃。
- Spectrogram 1 kHz ±1 bin；60 s 历史。

**演示脚本**

1. 4 通道接口，CH1/CH2 接同一信号，CH3 接被测，CH4 接噪声参考。
2. 通道匹配：增益差 0.02 dB，相位差 0.5°，skew 0 sample。
3. 切到 XY，显示 1:1 椭圆；反相后变成另一条对角线。
4. 两台设备测漂移：显示 98 ppm，每 0.21 s 滑一个采样。
5. 热力图显示 1 kHz 与谐波。

### 5.5 v1.4 — Repair Workflow

**目标**：在双端上让测量“能干活”。

**范围**

- 模板：REP-01…10。
- 磁带：PH-07、PH-08。
- 频谱：SPEC-06、07。
- 报告：RPT-04、05、08。
- 输入：INP-03。

**用户故事**

1. 作为维修者，我选“吉他效果器”模板，按步骤测 bypass/engage，最后得到前后对比。
2. 作为维修者，我测磁带机 wow & flutter，判断是否需要换皮带/压带轮。
3. 作为维修者，我校准耳放输出阻抗，判断是否适合低阻耳机。
4. 作为店主，我把两次报告并排给客户看，证明修复有效。

**退出标准**

- 3 个模板端到端可用；每步有判据/建议。
- wow & flutter 重复性 < 5%；方位角可读。
- 前后对比报告含差值表 + 曲线叠加。
- 长时测试 8 小时内存有界。

### 5.6 v2.0 — Scale

**目标**：让产品“能规模化”。

**范围**

- PLAT-01（通用二进制）、PLAT-03（签名/公证/Windows 安装包）、PLAT-05（i18n）。
- CAL-08（设备库）。
- 文档/教程/示例报告。

**退出标准**

- macOS 通用二进制可运行；DMG 公证；Gatekeeper 无警告。
- Windows 安装包（MSI/EXE）可用；SmartScreen 无警告或文档说明。
- 中英文切换完整。
- 设备库 ≥ 30 条，每条有来源。

### 5.7 硬件工作流 FE-1.0（与软件并行）

| 阶段 | 内容 | 周期 | 退出标准 |
|---|---|---|---|
| FE-0.1 | 面包板验证：输入保护、量程、缓冲、phantom blocker | 2 周 | ±50 V 不损坏；20 Hz–20 kHz ±0.1 dB |
| FE-0.2 | PCB + 外壳 + BOM | 4 周 | 2 台样机；噪声/THD 达标 |
| FE-1.0 | 小批量套件 + 装配说明 + 测试报告 | 4 周 | 3 款声卡验证；文档可复现 |

**前端关键指标**

- 输入阻抗：1 MΩ // ~20 pF。
- 量程：±1 V / ±10 V / ±50 V（可扩展 10:1 探头）。
- 带宽：20 Hz–20 kHz（−0.1 dB）；可选 80 kHz。
- THD+N：< −100 dB（1 kHz，2 Vrms，BW 20 kHz）。
- 噪声：< 10 µVrms（20 Hz–20 kHz，输入短路）。
- 保护：TVS + PTC + 限流 + 继电器衰减 + phantom blocker。
- 输出：600 Ω 限流，最大 +10 dBu，可衰减。

---

## 6. 关键功能规格

### 6.1 SafetyGuard 与模拟前端

#### 6.1.1 状态机

```
IDLE
  └─ 用户点击“开始” → PRECHECK
PRECHECK
  ├─ 源类型 / 预估电压 / phantom / 前端确认
  ├─ 不通过 → 显示原因，停留
  └─ 通过 → ARMED
ARMED
  ├─ 输出静音、输入最低量程、等待稳定
  └─ 用户确认 → RUNNING
RUNNING
  ├─ clip / over-range / device lost → FAULT
  └─ 用户停止 → IDLE
FAULT
  ├─ 输出静音、冻结、记录事件
  └─ 用户排除故障并复位 → IDLE
```

#### 6.1.2 预检项

1. 源类型：Line / Inst / Mic / Probe / Speaker-via-pad / Unknown。
2. 预估最大电压（Vpp 或 Vrms）。
3. 是否使用 AudioScope 前端；若否，显示“最大输入未知”。
4. Phantom 是否关闭；TS/TRS 是否匹配。
5. 量程建议；超出则禁止 Arm。
6. 用户勾选“我已确认不接市电/不直连喇叭输出”。

#### 6.1.3 Clip 检测

- 条件 A：`|x| ≥ 0.999` 连续 3 个样本。
- 条件 B：平顶检测——相邻样本差分接近 0 且 `|x| > 0.9` 连续 5 个样本。
- 动作：锁存峰值/通道/时间；10 ms 淡出；静音输出；冻结采集；写事件；显示红色横幅。
- 复位：用户确认后清除锁存；不得自动恢复。

#### 6.1.4 量程建议

| 预估 Vpp | 建议量程 | 备注 |
|---|---|---|
| < 2 V | ±1 V | 需要前端 |
| 2–20 V | ±10 V | 需要前端 |
| 20–100 V | ±50 V | 需要前端 + 10:1 |
| > 100 V | 禁止 | 不在产品范围 |

#### 6.1.5 输出保护

- 默认最大输出 −20 dBFS；用户可在设置中提高，但需二次确认。
- 启动静音 200 ms；停止/切换设备时淡出。
- 输出直流偏移检测；> 10 mV 警告。
- 文件播放前做峰值扫描；超过限幅则拒绝或衰减。

#### 6.1.6 前端硬件要点

- 输入：1 MΩ 对地，AC 耦合（10 µF/100 V 薄膜），串联 1 kΩ，TVS + PTC。
- 量程：继电器切换 π 型衰减；不要只靠钳位二极管（会在削波前失真）。
- 缓冲：低噪声 JFET/CMOS 运放；差分输入用于平衡源。
- Phantom blocker：47 µF/100 V 或 1:1 变压器；优先继电器断开。
- 输出：600 Ω 串联，最大 +10 dBu，可开关衰减。
- 电源：USB 隔离或低噪声 LDO；模拟/数字分开。
- 外壳：金属屏蔽；BNC/TRS 组合；明确标注最大输入。

### 6.2 校准与单位

#### 6.2.0 为什么不做厂商爬虫（产品决策）

“爬主流厂商声卡参数”在 demo 里很诱人，但不能作为测量依据：

- **页面没有统一 API**：字段非结构化、命名不一致、随时改版；维护成本高。
- **厂商参数是典型值，不是校准值**：同一个接口在不同增益、pad、输入阻抗下，0 dBFS 对应的电压完全不同。
- **关键信息通常缺失**：模拟增益旋钮位置、单元公差、驱动/系统音量、输入阻抗随频率变化。
- **法律与合规**：ToS/robots、版权、数据库权利；商业产品不应把爬虫当基础设施。
- **用户会过度信任**：把“SNR 110 dB”误当成“电压准确”。

**正确路径**：CoreAudio 能力探测（能拿到什么就显示什么）→ 人工校准向导 → 精选设备库（人工审核、来源/日期/置信度）→ 用户自定义参数。爬虫最多做内部种子数据，必须人工复核并标注来源。

#### 6.2.1 数据模型

```json
{
  "schema_version": 2,
  "id": "cal_01H...",
  "device_id": "dev_uid_hash",
  "device_name": "Scarlett 2i2 4th Gen",
  "input_channel": 0,
  "gain_db": 0,
  "pad_db": 0,
  "sample_rate": 48000,
  "fs_volts_rms": 1.0,
  "fs_volts_peak": 1.414,
  "input_impedance_ohm": 10000,
  "coupling_hz": 1.5,
  "method": "dmm",
  "confidence": "measured",
  "created_at": "2026-11-07T10:00:00+08:00",
  "app_version": "1.2.0",
  "points": [
    {"dbfs": -40, "volts_rms": 0.01},
    {"dbfs": -20, "volts_rms": 0.1},
    {"dbfs": -6, "volts_rms": 0.501}
  ],
  "checksum": "sha256:..."
}
```

#### 6.2.2 校准向导

1. 选择设备与输入通道。
2. 把增益旋钮固定到标记位置；记录档位。
3. 选择方法：
   - **万用表**：输出 1 kHz −20 dBFS，测输入 AC Vrms，填入。
   - **已知参考**：使用标准信号源/校准器。
   - **Loopback**：输出直回输入，用已知输出 FS 电压推导。
4. 在 −40/−20/−6 dBFS 验证线性；偏差 > 0.2 dB 警告。
5. 保存 profile；显示置信度。
6. 建议在设备/增益变化后重新校准。

#### 6.2.3 单位换算

- `dBu = 20 log10(Vrms / 0.775)`
- `dBV = 20 log10(Vrms / 1.0)`
- `Vpp = 2√2 · Vrms`（正弦）
- `dBFS = 20 log10(peak / FS_peak)`
- 非正弦信号用波峰因数修正；UI 明确标注“假设正弦”。

#### 6.2.4 置信度

| 标签 | 含义 | UI |
|---|---|---|
| `measured` | 本机本档实测 | 绿色 “实测” |
| `datasheet` | 厂商典型值 | 琥珀 “典型” |
| `assumed` | 默认假设（如 1 V） | 砖红 “假设” |

**规则**：没有置信度的电压不显示；报告必须带来源。

### 6.3 输入模式与信号检查

#### 6.3.1 模式预设

| 模式 | 标称电平 | 典型最大 | 输入阻抗 | 备注 |
|---|---|---|---|---|
| Line Pro | +4 dBu | +24 dBu | 10 kΩ | 平衡/非平衡 |
| Line Consumer | −10 dBV | +10 dBV | 10 kΩ | RCA/3.5mm |
| DI (balanced line) | +4 dBu 或 −10 dBV | +24 dBu | 10 kΩ 平衡 | DI 盒输出；非独立电平标准 |
| Instrument | −20 dBV | +10 dBV | 1 MΩ | 高阻源 |
| Mic | −60 dBu | −20 dBu | 1.5 kΩ | phantom 必须关 |
| Probe 10:1 | — | ±50 V | 10 MΩ | 需前端 |
| Speaker via pad | — | ±50 V | 8 Ω 负载 | 仅假负载 |

**关于 DI 的术语纠正**：DI 是器件/接口形态，不是一种“DI 电平”。DI 盒的输出通常是 **mic level 或 line level、平衡**。因此 UI 不设“DI 信号类型”，而是提供：

- **DI (balanced line)** 预设：等价于 Line Pro + 平衡输入提示 + 低增益起点。
- **Instrument** 预设：高阻、−20 dBV、1 MΩ；这才是吉他/贝斯直入的按钮。
- 若用户接的是“DI 盒输出”，应选 Line Pro 或 DI 预设，而不是 Instrument。

**Inst 按钮的行为**：切换输入模式 → 更新预期电平/阻抗/安全上限 → SafetyGuard 重新预检；**不会**根据波形自动切换。

#### 6.3.2 信号检查助手

采集 2 秒，计算：

- 峰值、RMS、波峰因数。
- 50/60 Hz 及谐波能量。
- 频谱倾斜（1 kHz 以上每倍频程衰减）。
- 噪声底（静音段）。
- 直流偏移（去均值前）。

输出示例：

> **建议：Instrument（置信度中）**
> 峰值 −38 dBFS，波峰因数 14 dB，10 kΩ 负载下 4 kHz 以上衰减 4 dB。看起来像高阻拾音器。请确认使用 Hi-Z 输入；若不确定，先选 Line 并降低增益。

**不变量**：助手只建议，不自动切换；用户必须点击确认。

#### 6.3.3 双阻抗源阻抗测试（需前端）

1. 切换 1 MΩ，测 V1。
2. 切换 10 kΩ，测 V2。
3. `Zout ≈ R · (V1/V2 − 1)`。
4. 判定：
   - `Zout > 10 kΩ` → 高阻乐器源，建议 Inst。
   - `1 kΩ < Zout < 10 kΩ` → 不确定，建议确认。
   - `Zout < 1 kΩ` → Line。
5. 显示估算值与误差范围；标注“需前端”。

### 6.4 多声道引擎

#### 6.4.1 数据结构

```c
#define AS_MAX_CH 16

typedef struct {
    int index;
    char name[64];
    uint32_t color;
    int enabled, solo, muted;
    double gain_db, offset_v;
    Calibration cal;
} ASChannel;

typedef struct {
    int n_ch;
    float *ring;              /* interleaved n_ch */
    uint32_t ring_cap, ring_mask;
    _Atomic uint32_t ring_w;
    _Atomic int64_t ring_filled;
    ASChannel ch[AS_MAX_CH];
    /* ... */
} ASEngine;
```

#### 6.4.2 通道映射

- 读取 `kAudioDevicePropertyStreamConfiguration`，处理交错/非交错。
- 通道名优先使用设备提供的元素名；否则 `Input 1..N`。
- 保存 `device_uid + channel_index + name`，设备变化时按 UID 恢复。
- 聚合设备：识别 `kAudioDeviceTransportTypeAggregate`；显示子设备与时钟源。

#### 6.4.3 实时规则

- 音频回调只做：读输入、写 ring、生成输出、更新原子峰值。
- 禁止：malloc/free、mutex、文件 IO、ObjC、日志、长循环。
- 配置双缓冲 + 原子指针交换；命令队列 SPSC。
- UI 以 30 Hz 读取快照；分析在后台线程。

#### 6.4.4 验收

- 4ch/8ch 同时显示；标签/颜色/顺序正确。
- 任意通道可触发、可 math、可 XY。
- 拔插 20 次不崩溃；设备丢失有提示。
- 8ch/48k 连续 10 分钟 xrun = 0。

### 6.5 坐标交互

#### 6.5.1 交互表

| 操作 | 行为 |
|---|---|
| 滚轮悬停绘图区 | X 轴缩放（时间） |
| Ctrl/Cmd + 滚轮 | Y 轴缩放（电压） |
| Alt + 滚轮 | 双轴缩放 |
| Shift + 滚轮 | X 轴平移 |
| 滚轮悬停 X 轴 | 仅 X 缩放 |
| 滚轮悬停 Y 轴 | 仅 Y 缩放 |
| 触控板捏合 | 双轴缩放 |
| 拖拽 | 平移 |
| Shift + 拖拽 | 框选放大 |
| 双击 | 自适应 |
| 右键 | 复位 |
| 双光标 | ΔT、ΔV、1/ΔT |

#### 6.5.2 锚定算法

```
anchor_ratio = (mouse_x - plot_x) / plot_w
t_anchor = view_start + anchor_ratio * view_span
new_span = view_span * factor
new_start = t_anchor - anchor_ratio * new_span
```

Y 轴同理。缩放后光标下的数据点保持在同一屏幕位置。

#### 6.5.3 独立通道量程

- 每通道：`v_div[i]`、`v_offset[i]`、`unit[i]`。
- 默认链接；用户可解锁。
- 通道颜色与标签在轴左侧显示。
- 自动量程仅作用于当前通道。

#### 6.5.4 验收

- 锚点漂移 < 1 px。
- 两通道可同时 0.1 V/div 与 2 V/div。
- 框选放大后坐标轴刻度合理（1-2-5）。
- 键盘：↑↓ 调 V/div，←→ 调时基，Shift 平移。

### 6.6 XY / Lissajous / 相位 / 时钟

#### 6.6.1 物理边界

- **同设备 CH1/CH2**：共享采样时钟；可测固定相位差、极性、通道 skew、串扰；**不可测 clock drift**。
- **两台设备/聚合设备**：各自时钟；可测相对漂移（ppm）与滑移周期。
- **数字链路**：可测采样滑移/丢帧。

#### 6.6.2 XY 视图

- X = CH1，Y = CH2；等量程；网格；可选余晖。
- 1:1、2:1、3:1 比例识别。
- 椭圆拟合：相位 `φ = asin(短轴/长轴)`（1:1 近似）；更稳健用 DFT 基频相位差。
- 读数：相位（°）、增益比（dB）、极性、频率比。
- 反相：椭圆变为另一条对角线；自动标记 “POLARITY INVERTED”。

#### 6.6.3 通道匹配测试

1. 信号源经 Y 分线进 CH1/CH2。
2. 扫频 20 Hz–20 kHz（1/12 倍频程或 24 点）。
3. 每频点测增益差、相位差。
4. 冲击/方波测 skew（采样点/µs）。
5. 单通道驱动，测另一通道串扰（−dB）。
6. 报告：表 + 曲线 + 结论（正常/通道故障/线缆问题）。

#### 6.6.4 跨设备漂移测试

1. 两台设备（或聚合设备）接同一参考信号。
2. 采集 60 s；每 0.5 s 估计相位。
3. 相位解卷绕；线性拟合斜率 `s`（度/秒）。
4. `Δf = s / 360`；`ppm = Δf / f × 1e6`。
5. 滑移周期 `T = 1 / (ppm × 1e-6 × fs)` 秒。
6. 显示 ppm、方向、滑移周期、置信区间。
7. 若同设备：显示“共享时钟，漂移不可观测”，禁用该测试。

#### 6.6.5 相位斜率与延迟

- 扫频测 `φ(f)`；若 `φ` 随 f 线性下降，斜率为延迟 `τ = −dφ/dω`。
- 固定相移：`φ` 近似常数。
- UI 显示 “Delay 1.23 ms / Phase offset 12°”。

#### 6.6.6 仿真/训练

注入参数：

- 相位偏移 0–180°。
- 增益差 0–6 dB。
- 漂移 0–500 ppm。
- 噪声/串扰/削波。
- 极性反转。

场景：健康、反相、90°、100 ppm、削波、串扰、接地环路 hum。

验收：注入值可被测量值恢复；训练模式给出“正确/错误”反馈。

### 6.7 Spectrogram 热力图

#### 6.7.1 数据

```c
typedef struct {
    float *db;        /* rows × cols, dBFS */
    int rows, cols;
    int head;         /* ring */
    double t0, dt;
    double f0, df;
    int valid;
} SpecMatrix;
```

- 每帧 FFT 后写入一行；环形覆盖。
- 与瀑布图分离：瀑布图可保留为轻量预览，热力图用矩阵。

#### 6.7.2 渲染

- dB → 颜色：单调亮度色标；viridis/magma/grayscale。
- Gamma 可调；色标带 dB 刻度。
- 余晖：`display = max(new, display - decay)`；decay 可调。
- Max hold / min hold。
- 光标：时间/频率/dB；与 Scope 时间轴联动。
- 缩放/平移：时间与频率独立。

#### 6.7.3 导出

- PNG：含色标、标题、设备/设置元数据。
- CSV：time_s, freq_hz, db。
- 报告嵌入 PNG。

#### 6.7.4 THD 热力图

- X = 频率，Y = 电平，颜色 = THD%。
- 扫频 × 扫电平；每点测 THD。
- 用于找削波/失真起点；可叠加等值线。
- 导出 CSV + PNG。

### 6.8 会话日志与报告

#### 6.8.1 事件日志

`session.jsonl`：

```json
{"ts":"2026-11-07T10:00:00.123+08:00","type":"app_start","data":{"version":"1.2.0","os":"macOS 14.5"}}
{"ts":"...","type":"device_change","data":{"in":"Scarlett 2i2","out":"Scarlett 2i2","rate":48000}}
{"ts":"...","type":"calibration_load","data":{"id":"cal_...","confidence":"measured"}}
{"ts":"...","type":"clip","data":{"ch":0,"peak":1.02,"t":12.34}}
{"ts":"...","type":"run_end","data":{"run_id":"...","verdict":"pass"}}
```

事件类型：`app_start/stop`、`device_change`、`sample_rate_change`、`xrun`、`clip`、`input_mode_change`、`calibration_change`、`run_start/end`、`note`、`error`。

#### 6.8.2 报告 schema v2

```json
{
  "schema_version": 2,
  "app": {"version": "1.2.0", "build": "..."},
  "os": {"name": "macOS", "version": "14.5", "arch": "arm64"},
  "device": {"in": {...}, "out": {...}, "uid": "..."},
  "calibration": {"id": "...", "confidence": "measured", "method": "dmm"},
  "session": {"id": "...", "started_at": "...", "operator": "..."},
  "run": {"id": "...", "template": "guitar_pedal", "settings": {...}},
  "results": {...},
  "limits": {...},
  "verdict": {"pass": true, "reasons": [...]},
  "provenance": {"settings_hash": "...", "calibration_hash": "...", "report_hash": "..."},
  "attachments": ["scope.png", "spectrum.png"],
  "notes": "..."
}
```

#### 6.8.3 HTML/PDF

- HTML 为 canonical：自包含 CSS/PNG，可离线打开。
- PDF：macOS 用 `CGPDFContext`；Windows 用系统打印到 PDF。
- 页面：封面、设备/校准、结论、频响、THD、时域、频谱、事件、建议、附录。
- 所有数字带单位/置信度；失败项红色标注。

#### 6.8.4 前后对比

- 选择两次 run（before/after）。
- 差值表：频响、THD、噪声、串扰、阻抗。
- 曲线叠加：before 灰、after 绿。
- 结论：改善/恶化/无变化；自动生成一句话。
- 可导出独立对比报告。

#### 6.8.5 限值与判定

- 模板自带默认限值；用户可覆盖。
- 限值示例：频响 ±1 dB（20 Hz–20 kHz）、THD < 0.1%、SNR > 90 dB、串扰 < −60 dB。
- 判定规则：全部通过 = PASS；关键项失败 = FAIL；其余 = WARN。
- 报告显示每条判据与实际值。

### 6.9 维修工作流

#### 6.9.1 模板引擎

```json
{
  "id": "guitar_pedal",
  "name": "吉他效果器",
  "version": 1,
  "input_mode": "inst",
  "steps": [
    {"id":"cal","type":"calibrate"},
    {"id":"bypass","type":"measure_fr","signal":{"level_dbfs":-20},"limits":{"flat_db":1.0}},
    {"id":"engage","type":"measure_fr","signal":{"level_dbfs":-20},"limits":{"flat_db":3.0}},
    {"id":"thd","type":"measure_thd","freq":1000,"limits":{"thd_pct":0.5}},
    {"id":"noise","type":"measure_noise","limits":{"snr_db":70}},
    {"id":"clip","type":"find_clip","limits":{"headroom_db":6}}
  ]
}
```

- 步骤类型：`calibrate`、`measure_fr`、`measure_thd`、`measure_noise`、`measure_crosstalk`、`measure_impedance`、`measure_wow`、`measure_azimuth`、`find_clip`、`manual_note`。
- 支持重试、跳过（需理由）、分支（bypass/engage）。
- 每步输出：值、单位、判据、通过/失败、建议。

#### 6.9.2 模板清单

| 模板 | 关键测量 | 版本 |
|---|---|---|
| 吉他效果器 | bypass/engage 频响、THD、噪声、削波余量 | v1.4 |
| 磁带机 | 带速、wow&flutter、方位角、频响、串扰、噪声 | v1.4 |
| 调音台通道 | 增益、频响、THD、串扰、phantom | v1.4 |
| 耳放 | 负载频响、THD vs 电平、输出阻抗、串扰 | v1.4 |
| 合成器/线路输出 | 噪声、频响、削波、串扰 | v1.4 |

#### 6.9.3 专用测量

- **Wow & flutter**：3 kHz 测试带；FM 解调；IEC/DIN 加权；峰值/加权 RMS；带速偏差 ppm。
- **方位角**：10 kHz 左右声道相位差；调整磁头使相位差最小、和信号最大。
- **串扰**：驱动 A，测 B；`20 log10(Vb/Va)`；20 Hz–20 kHz。
- **阻抗**：串联已知电阻 R；测 DUT 电压 Vd 与源电压 Vs；`Z = R · Vd/(Vs − Vd)`；需校准。
- **IMD**：SMPTE 60 Hz + 7 kHz（4:1）；DIN 250 Hz + 8 kHz；测边带。
- **THD vs 电平**：固定频率，扫电平；找失真拐点。

### 6.10 仿真与训练

- 复用 `--simulate` 虚拟 DUT，扩展参数：增益、频响、THD、噪声、相位、漂移、串扰、削波、hum。
- 新增 `--simulate --scenario=...`，输出已知答案 JSON。
- UI 训练模式：选择场景 → 显示“故障波形/XY/频谱” → 用户选择诊断 → 反馈。
- 验收：注入 100 ppm 读 ±5 ppm；注入 45° 读 ±2°；注入 −60 dB 串扰读 ±3 dB。

---

## 7. 技术架构

### 7.0 语言与工具链决策

**结论：C11 实时核心 + C++17 应用/平台层，`extern "C"` 隔离；macOS 用 Objective-C++ 桥接，Windows 用 C++/Win32/COM。不整体重写。**

| 层 | 语言 | 理由 |
|---|---|---|
| DSP、FFT、ring、安全状态机、单位/校准数学 | **C11** | 无隐藏分配/异常/RTTI；ABI 简单；易审计；复用 v1.1 |
| 应用状态、设置、会话、报告、模板、视图模型 | **C++17** | RAII、容器、字符串、`optional/variant/filesystem`；复杂状态与 schema |
| macOS 平台层 | **Objective-C++** | 直接桥接 AppKit/CoreGraphics 与 C++ 应用层 |
| Windows 平台层 | **C++17 + Win32/COM** | WASAPI、Direct2D/DirectWrite 的天然语言；MSVC 工具链 |
| 构建 | **CMake 3.24+** | 双端构建、CTest、CI；构建工具不是运行时依赖 |

**硬规则**

- 所有核心接口 `extern "C"`；C ABI 不出现 STL 类型、异常、虚函数。
- 实时回调禁用：`new/delete`、`std::vector/string/function`、异常、锁、文件 IO。
- 核心编译 `-fno-exceptions -fno-rtti`；MSVC 用 `/EHs-c- /GR-`。
- 异常只在应用层使用，且在 C ABI 边界捕获；不允许穿透 C 代码。
- C++17 起步；C++20 只在 macOS/Windows 工具链都稳定后评估。
- 不引入 Qt/ImGui/Boost 等第三方 UI/基础库；系统框架优先。
- 静态初始化不得在音频启动后运行；所有平台对象在 `main` 中显式初始化。

**如果必须只选一种**

- 选 **C++17**，但实时核心按“C 风格 C++”写：固定数组、自由函数、无异常、无 RTTI、无 STL 容器，接口 `extern "C"`。
- 这等价于“一个编译器写两种风格”，可行，但不如物理分层清晰。

**为什么不选**

- **纯 C**：应用层的字符串/JSON/报告/模板/多通道管理会大量手写容器与生命周期，v1.3/v1.4 会变慢且易错。
- **纯 C++ 重写**：当前 C 核心已验证，重写只引入回归；实时路径的隐藏分配/异常风险更高。
- **Qt / Dear ImGui**：违背零第三方依赖与小体积定位。
- **SwiftUI**：macOS 舒服，但 Windows 端锁死。
- **Rust**：语言很好，但全量重写 + CoreAudio/WASAPI 互操作成本不划算。

**迁移原则**

1. 先建 `core/` 静态库与 C ABI，不动 DSP 行为。
2. 再抽 `AsAudioBackend` / `AsCanvas` / `AsWindow` / `AsFiles` / `AsTime`。
3. macOS 先适配，Windows 同步实现最小后端。
4. 应用层新增模块才用 C++；旧 `app.c`/`view.c` 逐步迁移，不做大爆炸重写。

### 7.1 目标结构

```
audioscope/
├── CMakeLists.txt
├── src/
│   ├── core/                 # 便携 C11，无平台依赖
│   │   ├── dsp/              # fft, window, stats, generator
│   │   ├── audio/            # ring, format, device model
│   │   ├── measure/          # tone, fr, thd, imd, impedance, wow, crosstalk
│   │   ├── calib/            # profile, calibration, units
│   │   ├── safety/           # guard, clip, limits
│   │   ├── session/          # log, report, compare
│   │   └── testplan/         # plan, templates
│   ├── app/                  # C++17：state, settings, session
│   ├── ui/                   # C++17：canvas.h, theme, views
│   ├── platform/
│   │   ├── macos/            # CoreAudio, CoreGraphics, AppKit
│   │   └── win/              # WASAPI, Direct2D, Win32
│   └── main/
│       ├── main.mm           # macOS shell
│       └── main_win.cpp      # Windows shell
├── tests/
│   ├── core/                 # C tests
│   ├── app/                  # C++ tests
│   ├── golden/               # known-answer files
│   ├── sim/                  # virtual DUT
│   └── hw/                   # hardware checklist
└── packaging/
    ├── macos/
    └── windows/
```

### 7.2 迁移策略

不要大爆炸重构。按以下顺序：

1. **建构建骨架**（LANG-01）：CMake + `core` 静态库 + C ABI + 双端 CI；行为不变。
2. **先无锁化**（CH-07）：保持立体声，替换 ring/mutex；行为不变，测试通过。
3. **抽平台接口**（PLAT-10）：`AsAudioBackend`、`AsCanvas`、`AsWindow`、`AsFiles`、`AsTime`；macOS 先包一层。
4. **双端最小后端**（PLAT-04）：macOS 适配 + Windows WASAPI/D2D；跑通 2ch/48k 示波器。
5. **抽核心模块**：把 `dsp.c`、`calib.c`、`report.c` 中纯逻辑移入 `core/`；保留兼容头。
6. **N 通道**：改 `ASEngine` 与 `ASChannel`；同步改 UI/测试。
7. **视图拆分**：`view.c` → `ui/views/scope.cpp`、`fft.cpp`、`xy.cpp`、`report.cpp`。

每个 PR 必须双端可构建、可测试、可回滚；至少 macOS 可跑，Windows 不回归。

### 7.3 数据模型

| 对象 | 关键字段 | 存储 |
|---|---|---|
| `DeviceProfile` | uid、vendor、model、transport、rates、channels、impedance、source、confidence | `profiles/devices.json` |
| `Calibration` | device_id、channel、gain、pad、fs_volts、method、confidence、points、checksum | `calibration/<device>/<ch>-<gain>.json` |
| `Session` | id、start、end、devices、calibrations、events、runs | `sessions/<year>/<id>/session.jsonl` |
| `Run` | id、template、settings、results、verdict | `sessions/.../run-<id>.json` |
| `Report` | schema、provenance、results、limits、attachments | `sessions/.../report.html` / `.json` |
| `TestPlan` | id、version、steps、limits | `templates/*.json` |

### 7.4 线程与实时规则

| 线程 | 职责 | 禁止 |
|---|---|---|
| Audio IO（CoreAudio / WASAPI） | 读写设备、ring、生成器 | malloc/free、mutex、文件 IO、ObjC/COM、日志、C++ 异常、STL 容器 |
| Analysis | FFT、测量、校准计算 | 阻塞音频、长时间持锁 |
| UI | 30 Hz 绘制、输入 | 直接读音频缓冲；只读快照 |
| Export | 报告/CSV/WAV 写入 | 阻塞 UI；后台任务 |

- 配置使用双缓冲 + 原子交换；命令使用 SPSC 队列。
- 峰值/电平用原子变量；UI 读取后清零由 UI 负责。
- 设备热插拔在专用线程处理；音频回调只看到新配置。
- Windows：COM 在每个线程显式初始化；音频回调不做 COM 调用。
- C++ 异常不得跨越音频回调或 C ABI；应用层在边界捕获。

### 7.5 存储布局

macOS：`~/Documents/AudioScope/`；Windows：`%USERPROFILE%\Documents\AudioScope\`。

```
~/Documents/AudioScope/
├── settings.json
├── profiles/
│   └── devices.json
├── calibration/
│   └── <device_id>/
│       ├── in0-g0.json
│       └── in1-g0.json
├── sessions/
│   └── 2026/
│       └── 20261107-100000-abc123/
│           ├── session.jsonl
│           ├── run-001.json
│           ├── capture-001.wav
│           ├── scope.png
│           ├── spectrum.png
│           ├── report.html
│           └── report.pdf
├── templates/
│   ├── guitar_pedal.json
│   └── tape_deck.json
└── logs/
    └── app-2026-11-07.log
```

### 7.6 迁移

- `settings.ini` → `settings.json`：读取旧键，写新 schema；保留备份 `settings.ini.bak`。
- `calibration.json` → 新 `Calibration`：旧数据标记 `confidence: datasheet` 或 `assumed`。
- 报告 v1 → v2：保留读取器；新报告只写 v2。
- 迁移失败：不删除旧文件；显示错误；允许手动导出。

### 7.7 平台抽象

```c
typedef struct AsAudioBackend AsAudioBackend;
typedef struct {
    int (*list_devices)(AsDevice *out, int max);
    int (*open)(AsAudioBackend *b, const AsAudioConfig *cfg);
    int (*start)(AsAudioBackend *b);
    int (*stop)(AsAudioBackend *b);
    int (*read)(AsAudioBackend *b, float *dst, int frames);
    int (*write)(AsAudioBackend *b, const float *src, int frames);
    double (*latency_ms)(AsAudioBackend *b);
} AsAudioVTable;
```

- 核心侧用 C vtable（`AsAudioBackend`），不暴露 C++ 类型。
- 应用/UI 侧用 C++ 接口（`Canvas`、`Window`、`Files`、`Time`）：

```cpp
class Canvas {
public:
    virtual ~Canvas() = default;
    virtual void fill_rect(Rect, Color) = 0;
    virtual void stroke(Rect, Color, float width) = 0;
    virtual void text(Point, FontRole, Color, std::string_view) = 0;
    virtual void image(ImageHandle, Rect) = 0;
    virtual void clip(Rect) = 0;
};
```

- 渲染接口：`fill_rect`、`stroke`、`text`、`image`、`clip`；macOS 用 CoreGraphics，Windows 用 Direct2D/DirectWrite。
- 文件接口：打开/保存对话框、文件读写、路径；UTF-8 与 `wchar_t` 在平台边界转换。
- 时间接口：单调时钟、日期时间。
- 窗口接口：创建/尺寸/DPI/事件循环；macOS AppKit，Windows Win32。

### 7.8 依赖政策

- 核心与双端平台层：仅系统框架 + C/C++ 标准库。
- C++ 标准库（libc++ / MSVC STL）视为平台工具链的一部分，不是第三方运行时依赖。
- 不引入 JSON 库：写一个小型 parser/writer（受控子集）或使用现有手写输出。
- PDF：macOS `CGPDFContext`；Windows 用系统打印/HTML。
- 构建工具：CMake 3.24+；不引入包管理器（vcpkg/Conan）。
- 不允许网络库；不允许 Qt/ImGui/Boost。
- 第三方代码进入需 Apache-2.0 兼容 + NOTICE 更新 + 法务审查。

---

## 8. 质量与验证

### 8.1 测试金字塔

| 层级 | 内容 | 工具/命令 |
|---|---|---|
| 单元 | FFT、窗、统计、单位、校准、限值、schema | `make test` / `ctest` |
| 黄金文件 | 已知 WAV → 期望 JSON/CSV；逐字节/容差 | `tests/golden` |
| 仿真 | 虚拟 DUT、故障注入、漂移/相位 | `--simulate` |
| UI 快照 | 双端界面图；布局回归 | `make shot` / `--shot` |
| 硬件在环 | 2 款接口 × 2 个 macOS 版本 + 2 款 × 2 个 Windows 版本 | 手工清单 |
| 模糊 | 设置/报告/模板 JSON；异常文件 | `--fuzz`（新增） |
| Sanitizer | ASan/UBSan；TSan（音频线程） | `make asan` / `ctest` / 新增 `make tsan` |

### 8.2 精度验收夹具

- 1 kHz 正弦，−20 dBFS，已知幅度/相位。
- 扫频 20 Hz–20 kHz，已知传递函数。
- 方波/冲击，已知延迟。
- 双设备漂移 100 ppm。
- 串扰 −60 dB。
- 阻抗 32 Ω / 300 Ω / 10 kΩ。
- Wow & flutter 0.1% / 0.3%。

### 8.3 发布门禁

- [ ] macOS 13/14 + windows-2022 双端 CI 全绿。
- [ ] `ctest` / `make test` 全绿。
- [ ] ASan/UBSan 干净；音频/线程变更 TSan 干净。
- [ ] UI 快照双端无布局回归。
- [ ] 新功能有单元/仿真/黄金测试。
- [ ] 报告 schema 校验通过。
- [ ] 安全故障注入全过。
- [ ] macOS 2 款 + Windows 2 款接口手工验证。
- [ ] 双端 1 kHz 读数一致（频率 < 0.05%，幅度 < 0.5 dB）。
- [ ] 无 P0/P1 缺陷。
- [ ] CHANGELOG / README / 文档更新。
- [ ] 版本号、schema 版本、报告版本一致。

### 8.4 硬件在环清单

- macOS 基准：Scarlett 2i2、MOTU M2（USB，2ch）。
- Windows 基准：Scarlett 2i2、Behringer UMC202HD（USB，2ch）。
- 4ch 接口（v1.3）：如 Audient iD14 / MOTU M4。
- 聚合设备（v1.3）：两台接口。
- macOS 13 / 14（v2.0 加 15）；Windows 10 22H2 / Windows 11 23H2+。
- 采样率 44.1k / 48k / 96k。
- 缓冲 32 / 64 / 128 / 256（Windows 共享模式以 128/256 为主）。

### 8.5 隐私

- 默认无网络请求；用 `fs_usage`/Little Snitch 验证。
- 崩溃日志本地；上传需明确同意。
- 报告默认不含用户名/绝对路径。
- 设备 UID 哈希后存储。

### 8.6 可访问性

- 键盘走查：所有按钮/输入/页签。
- VoiceOver 标签（v2.0）。
- 对比度检查。
- 减少动态。

---

## 9. 设计规范

### 9.1 信息架构

```
顶部：Scope | Spectrum | XY/Phase | Response | Loopback | Report
左栏：采集 / 输入模式 / 安全状态 / 校准
中央：主图 + 光标 + 图例
右栏：关键读数 / 判据 / 建议
底部：设备 / 采样率 / 缓冲 / xrun / 安全 / 置信度
```

### 9.2 两种模式

- **Guided（默认）**：维修模板、步骤条、下一步按钮、判据解释。
- **Bench（专家）**：自由测量、所有参数可见、快捷键。

### 9.3 状态设计

| 状态 | UI |
|---|---|
| 无设备 | 空状态 + “重新扫描” + 权限说明 |
| 未校准 | 黄色横幅 + “去校准”；电压显示 “—” |
| 已校准 | 绿色 “实测” 徽标 |
| 削波 | 红色横幅 + 冻结 + “复位” |
| 超量程 | 红色阻断 + 建议量程 |
| 设备丢失 | 模态 + 重连/切换 |
| 漂移 | 黄色 “98 ppm” + 滑移周期 |
| 同设备漂移测试 | 禁用 + 解释 |

### 9.4 文案规则

- 不说“保护你的声卡”；说“配合前端可降低风险”。
- 不说“精确电压”；说“已校准，±0.1 dB”。
- 不说“AI 诊断”；说“基于规则的检查”。
- 错误信息：发生了什么、为什么、下一步做什么。
- 单位：dBFS、dBu、dBV、Vrms、Vpp 始终标注。

### 9.5 视觉

- 沿用现有 theme token：暖灰石墨、鼠尾草绿重点色、琥珀警告、砖红错误。
- 4px 网格；图表 32px 安全区；单一重点色。
- 热力图不用彩虹；单调亮度。
- 数字等宽；真减号；缺失用 —。

### 9.6 可访问性

- 焦点环可见；Tab 顺序合理。
- 不只靠颜色区分通过/失败（加图标/文字）。
- 支持“减少动态”。
- 最小点击区域 32×32。

---

## 10. 发布与增长

### 10.1 打包

- macOS：`.app` + DMG；alpha 用 ad-hoc 签名；v2.0 签名/公证 + 通用二进制。
- Windows：alpha 用便携 ZIP（静态 CRT）；v2.0 提供 MSI/EXE 安装包并签名。
- `cmake --install` / CPack 生成产物；SHA256SUMS。
- 版本号语义化：`MAJOR.MINOR.PATCH`；alpha 用 `0.1.0-alpha`；schema 独立版本。

### 10.2 CI/CD

- GitHub Actions：
  - `macos-13` / `macos-14`：CMake 构建、`ctest`、ASan、截图、`.app`/DMG。
  - `windows-2022`：MSVC 2022、CMake 构建、`ctest`、便携 ZIP。
  - `ubuntu-latest`：`make win-core`（核心可移植性/交叉编译烟测）。
  - 上传 artifacts：DMG、ZIP、截图、测试日志。
- PR 必须双端绿；main 保护。
- 发布：tag → 双端构建 → 签名/公证（v2.0）→ release notes。

### 10.3 文档

- README：定位、截图、快速开始、安全边界、双端下载。
- `docs/BUILD-macos.md` / `docs/BUILD-windows.md`：双端构建步骤与依赖。
- `docs/quickstart.md`：10 分钟首次测量。
- `docs/calibration.md`：校准原理与步骤。
- `docs/safety.md`：电气安全、前端、禁止事项。
- `docs/templates/*.md`：每个维修模板的步骤与判据。
- `docs/reports/sample.html`：示例报告。
- `docs/KNOWN-ISSUES.md`：双端已知问题（alpha 必更）。
- `docs/faq.md`：常见问题。

### 10.4 社区

- Issue 模板：bug / feature / calibration data / hardware.
- 讨论区：维修案例、模板贡献。
- 校准数据库 PR 模板：设备、来源、日期、测量条件。
- 每季度发布“社区校准设备榜”。

### 10.5 商业模式（可选，不阻塞）

- 开源核心 + 硬件前端套件。
- 免费基础模板 + 付费专业模板/报告品牌（未来）。
- 不支持订阅制强制；尊重本地优先。

---

## 11. 风险登记

| 风险 | 影响 | 概率 | 缓解 |
|---|---|---|---|
| 用户误接高压导致设备损坏 | 高 | 中 | 前端 + 预检 + 阻断 + 明确免责；永不承诺“软件保护” |
| 电压读数被过度信任 | 高 | 高 | 置信度标签；未校准不显示电压；报告 provenance |
| 厂商参数不可靠/法律风险 | 中 | 高 | 不爬；人工精选；来源/日期；用户校准优先 |
| 多通道重构引入回归 | 高 | 中 | 先无锁化；分阶段；黄金文件；HIL |
| Windows WASAPI 共享模式延迟/格式不匹配 | 高 | 中 | 固定 48k/float32；自动转换；alpha 只承诺 48k；预留独占模式 |
| Direct2D 文本/DPI 与 macOS 差异 | 中 | 高 | 统一 theme token；DPI 感知；双端截图对比 |
| C/C++ ABI 或生命周期错误 | 高 | 中 | `extern "C"`；不传 STL；边界测试；ASan/TSan |
| Windows 平台经验不足 | 高 | 中 | 提前找 Windows 顾问；alpha 预留 8–10 周；先做最小后端 |
| 单一维护者/总线因子 | 高 | 中 | 文档、CI、任务拆分、社区 |
| 实时音频锁导致掉音 | 中 | 中 | CH-07 无锁化；TSan；压力测试 |
| 报告/PDF 复杂度 | 中 | 中 | HTML canonical；PDF 用系统；先简单模板 |
| 校准漂移/增益旋钮被碰 | 高 | 高 | 失效提醒；重新校准流程；报告记录档位 |
| 功能蔓延导致延期 | 高 | 高 | 版本门禁；非目标清单；P0 优先 |
| 硬件前端供应链/成本 | 中 | 中 | 参考设计先行；套件后置；BOM 目标 |
| 声卡时钟/驱动差异 | 中 | 高 | HIL 多设备；聚合设备；错误提示 |

---

## 12. 资源与排期

### 12.1 假设

- 1–2 名工程师（C11 + C++17 + macOS/Windows 平台经验），1 名兼职设计/测试，1 名兼职硬件。
- AI coding 辅助，但每个 PR 需人工 review；双端 CI 是硬门禁。
- 硬件前端与软件并行；0.1-alpha / v1.2 不依赖硬件量产。
- Windows 平台经验是 0.1-alpha 的关键变量；若缺失，alpha 预留 8–10 周。

### 12.2 里程碑

| 周 | 里程碑 |
|---|---|
| W1–2 | LANG-01 CMake/C ABI；PLAT-02 双端 CI；CH-07 无锁化；PLAT-09/10 分层与接口 |
| W3–4 | PLAT-04 macOS 适配 + WASAPI/D2D 最小后端；ALPHA-01 示波器 |
| W5–6 | ALPHA-02 FFT；ALPHA-03 信号源；ALPHA-04 单点校准 |
| W7–8 | ALPHA-05 安全最小集；ALPHA-06 保存；ALPHA-07 打包；ALPHA-08 HIL；ALPHA-09 文档；**0.1-alpha 发布** |
| W9–14 | v1.2：CAL/SAFE/INP/UI/LIM/RPT；双端回归/发布 |
| W15–20 | v1.3：CH-01…06、PH-01…06、SPEC-01…05；双端发布 |
| W21–26 | v1.4：REP-01…10、PH-07/08、SPEC-06/07、RPT-04/05/08；双端发布 |
| W27–38 | v2.0：PLAT-01/03/05、CAL-08、安装包/公证/i18n/设备库 |

### 12.3 关键路径

`LANG-01 → PLAT-09/10 → PLAT-04 → ALPHA-01/02 → ALPHA-04/05 → 0.1-alpha → CAL-02/03 → SAFE-02/03 → v1.2 → CH-01 → PH-03 → REP-02 → RPT-04 → v1.4 → PLAT-01/03`

### 12.4 并行工作流

- 软件主线：核心/平台接口 → alpha 双端 → v1.2 → v1.3 → v1.4。
- Windows 平台层：WASAPI/D2D/打包，与 macOS 并行。
- 硬件前端 FE-0.1/0.2/1.0。
- 文档/教程/示例报告。
- CI/发布工程（双端）。
- 社区/校准数据/alpha 试用反馈。

---

## 13. Vibe Coding 执行手册

> 目标：让 AI coding agent 能按任务独立完成可合并的 PR，而不是一次性生成一大坨不可审查的代码。

### 13.1 使用顺序

1. 读 [`AGENTS.md`](AGENTS.md)（硬约束）。
2. 读本文件的“版本范围”和“关键规格”；alpha 阶段读 [`ALPHA-0.1.md`](ALPHA-0.1.md)。
3. 从 [`BACKLOG.md`](BACKLOG.md) 选一个任务（alpha 从 B-alpha 开始）。
4. 按 13.4 的模板写 prompt。
5. 按 13.5 的 DoD 验收。
6. 按 13.6 自查后提 PR。

### 13.2 Agent 工作循环

```
理解任务 → 复述验收 → 找现有模式 → 写失败测试 → 最小实现
→ cmake build / ctest（双端）→ ASan → shot（UI）→ 自审 → 更新文档 → PR
```

**规则**：

- 一次只做一个任务；任务大于 1 天就拆。
- 不引入新依赖；不重写无关代码；不改变公共行为除非任务要求。
- 先写测试，再写实现；测试必须能在旧代码上失败。
- 每个 PR ≤ 400 行变更；超过则拆分。
- UI 变更必须附双端截图（`--shot`/`make shot`）。
- 涉及音频/校准/报告/安全，必须走专项自查。

### 13.3 任务切片

| 大小 | 范围 | 例子 |
|---|---|---|
| S | 1–3 文件，< 150 行 | 单位换算函数 + 测试 |
| M | 3–8 文件，< 400 行 | 校准向导 UI + 存储 |
| L | 拆分后再做 | N 通道引擎 |

### 13.4 Prompt 模板

```text
You are working in the AudioScope repo (macOS + Windows, C11 core + C++17 app,
zero third-party runtime deps).

CONTEXT
- Product: sound-card audio repair workstation.
- Current version: 0.1-alpha / v1.2. Roadmap: ALPHA-0.1.md + ROADMAP.md §5.
- Relevant docs: ROADMAP.md §6.x, §7.0, AGENTS.md.
- Existing files: <paths>.

TASK
<one backlog task ID + title>

REQUIREMENTS
- <acceptance criteria 1>
- <acceptance criteria 2>

CONSTRAINTS
- No new dependencies. No network. No allocations/locks/file IO/STL in audio callback.
- Core stays C11 with extern "C"; C++17 only in app/platform/ui.
- No C++ exceptions across the C ABI.
- Preserve existing behavior unless specified.
- Follow theme tokens and 4px grid for UI.
- Build and test on macOS and Windows; add tests first.

TESTS
- <unit/golden/sim test names>
- <manual verification steps on both platforms>

DELIVERABLE
- Minimal diff, tests, docs/changelog update.
- PR description: what changed, why, test evidence (both platforms), risks, rollback.
```

### 13.5 Definition of Ready / Done

**Ready**

- 需求 ID、版本、优先级明确。
- 验收标准可测。
- 依赖已完成或可 mock。
- 涉及的文件/接口已知。
- 安全/隐私影响已标注。

**Done**

- [ ] 验收标准全部满足。
- [ ] 新增/更新测试；`ctest` 双端绿。
- [ ] ASan/UBSan 干净；音频/线程变更 TSan 干净。
- [ ] UI 变更附双端截图；`--shot` 无回归。
- [ ] 文档/CHANGELOG 更新。
- [ ] 无新警告（`-Wall -Wextra -Werror` / `/W4 /WX`）。
- [ ] 实时/线程/安全/C ABI 自查通过。
- [ ] 报告/设置 schema 版本正确。
- [ ] PR 描述含双端测试证据、风险与回滚。

### 13.6 Review 清单

- [ ] 音频回调是否仍满足 0 malloc/0 mutex/0 IO/0 STL/0 异常？
- [ ] C ABI 是否只传 C 类型？异常是否在边界捕获？
- [ ] macOS 与 Windows 是否都能构建/测试？
- [ ] 设备热插拔/采样率切换是否处理？
- [ ] 校准失效是否提示？电压是否带置信度？
- [ ] 输入模式是否可能被静默切换？
- [ ] 报告 schema 是否兼容？
- [ ] 错误路径是否有用户可读文案？
- [ ] 是否引入未授权依赖/网络？
- [ ] 是否影响 Apache-2.0/NOTICE？
- [ ] 性能预算是否仍满足？

### 13.7 命令

```bash
# macOS
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=macos
cmake --build build
ctest --test-dir build --output-on-failure
./build/AudioScope --shot build/shots

# Windows
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=windows -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# 兼容旧 Makefile（v1.1）
make test
make asan
make win-core
```

### 13.8 护栏

- 不得删除/弱化安全预检。
- 不得让软件声称“保护声卡”。
- 不得静默自动切换输入模式/量程。
- 不得在未校准的情况下显示“精确电压”。
- 不得把用户数据上传到网络。
- 不得在音频回调中分配内存/加锁/写文件/用 STL/抛异常。
- 不得让 C++ 异常或 STL 类型穿过 C ABI。
- 不得破坏 v1.1 已有功能与报告读取。
- 不得引入第三方库（除非 ADR 批准）。
- 不得大范围格式化/重排无关代码。

---

## 14. 附录

### A. 需求追溯矩阵

| 目标 | 需求 ID | 版本 | 验收 |
|---|---|---|---|
| 0 macOS/Windows 双端 | PLAT-09/10/04/02、ALPHA-01…09 | 0.1-alpha | 双端 2ch/48k 示波器/FFT；CI 绿 |
| 1 声卡带宽内 | LIM-01/02、UI-07 | v1.2 | UI 明示 Nyquist/AC/DC 边界 |
| 2 大信号保护 | SAFE-01…07 | v1.2 + FE | 故障注入全过；前端 ±50 V |
| 3 参数自适应 | CAL-01…09 | v1.2 / v2.0 | 校准 < 0.1 dB；置信度 100% |
| 4 DI/Inst | INP-01…05 | v1.2 / v1.3 | 建议准确率 > 90%；不静默切换 |
| 5 滚轮缩放 | UI-01…06 | v1.2 | 锚点漂移 < 1 px；独立量程 |
| 6 多声道 | CH-01…06 | v1.3 | 8ch 稳定；映射正确 |
| 7 Lissajous/时钟 | PH-01…08 | v1.3 / v1.4 | 45°±2°；100 ppm±5 ppm |
| 8 热力图 | SPEC-01…07 | v1.3 / v1.4 | 1 kHz±1 bin；8h 有界 |
| 9 日志/报告 | RPT-01…08 | v1.2 / v1.4 | 离线报告；前后对比 |

### B. 术语

| 术语 | 含义 |
|---|---|
| dBFS | 相对数字满量程 |
| dBu | 相对 0.775 Vrms |
| dBV | 相对 1 Vrms |
| Inst | 高阻乐器电平 |
| DI | 直接注入盒；输出通常 mic/line 平衡 |
| Lissajous | 两通道 XY 图形 |
| Clock drift | 两设备采样时钟相对偏差（ppm） |
| Skew | 通道间采样/时间偏移 |
| Wow & flutter | 带速慢变/快变抖动 |
| Azimuth | 磁头方位角 |
| Spectrogram | 时间×频率×幅度热力图 |

### C. 参考

- 仓库：<https://github.com/mmr0817/audioscope>
- README：<https://github.com/mmr0817/audioscope/blob/main/README.md>
- Releases：<https://github.com/mmr0817/audioscope/releases>
- 相关标准（实现时查阅原文）：AES17（音频测量）、IEC 61606（音频设备测量）、IEC/DIN 磁带 wow&flutter 加权。

### D. 待决问题

1. 硬件前端是“参考设计”还是“售卖套件”？是否申请认证？
2. Windows alpha 用 WASAPI 共享模式还是直接上独占模式？建议共享模式。
3. Windows 便携 ZIP 还是 MSI？alpha 建议 ZIP，v2.0 再做安装包。
4. 报告 canonical 用 HTML 还是 PDF？PDF 是否必须跨平台一致？
5. 校准数据库是否接受社区提交？如何审核/署名？
6. 是否支持 App Store？沙盒会限制设备/文件访问。
7. 是否引入 opt-in 遥测？隐私优先，默认不做。
8. 8ch 还是 16ch 作为 v1.3 目标？
9. 是否支持 Linux（PipeWire）？暂不承诺。
10. C++17 是否足够？alpha 建议是；C++20 只在双端工具链稳定后评估。
11. macOS 通用二进制放 v2.0 是否可接受？alpha 只做 arm64。

### E. 变更记录

| 版本 | 日期 | 变更 |
|---|---|---|
| 1.0 | 2026-11-07 | 初版：产品定位、9 条需求映射、v1.2–v2.0 路线图、关键规格、vibe coding 手册 |
| 1.1 | 2026-11-07 | 加入 macOS/Windows 双端目标、C11 核心 + C++17 应用/平台层语言决策、0.1-alpha Dual Spike 构想；Windows 从 v2.0 提前到 alpha；新增 PLAT-09/10；调整排期与验收 |
