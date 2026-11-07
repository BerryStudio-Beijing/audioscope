# AGENTS.md — AudioScope 仓库工作协议

> 本文件是人与 AI coding agent 的共同契约。**开始任何任务前先读本文件**，再读 [`ROADMAP.md`](ROADMAP.md)、[`ALPHA-0.1.md`](ALPHA-0.1.md) 与 [`BACKLOG.md`](BACKLOG.md)。

---

## 1. 项目上下文

- **产品**：AudioScope by BerryStudio — 基于声卡的音频维修工作台。
- **不是**：通用示波器、RF/超声/DC 测量仪、DAW、声学测量工具。
- **代码基线**：AudioScope 1.1，macOS 12+ / arm64 / 纯 C11 / Apache-2.0。
- **目标平台**：macOS 12+ arm64 + Windows 10/11 x64。
- **语言**：C11 实时核心 + C++17 应用/平台层；macOS 用 Objective-C++ 桥接。
- **当前目标**：1.0 正式版（双通道独立、实际采样率、模拟/实测区分完成）；下一步 WASAPI、采样率切换、游标缩放、校准向导与 Windows 真机验证。
- **核心用户**：初学者、独立维修技师、音频硬件开发者、教学者。

产品原则：**安全 > 可信 > 工作流 > 报告 > 双端一致性 > 价格**。

---

## 2. 不可协商的约束

1. **零第三方依赖**。只使用系统框架 + C/C++ 标准库。新增依赖必须走 ADR 并经维护者批准。
2. **运行时不联网**。无遥测、无账号、无云服务；上传必须显式 opt-in。
3. **音频回调实时安全**。禁止 `malloc/free`、互斥锁、文件 IO、ObjC/COM、日志、无界循环、阻塞调用、C++ 异常、STL 容器。
4. **不承诺软件保护硬件**。UI/文档不得出现“保护你的声卡”这类表述；保护来自模拟前端 + 预检。
5. **不静默自动切换**。输入模式、量程、通道映射的变更必须可见、需确认、可撤销。
6. **电压必须带置信度**。未校准不得显示“精确电压”；只能显示 dBFS 或标注“假设”。
7. **保留许可证**。不得删除 Apache-2.0 头、LICENSE、NOTICE；新增文件带版权头。
8. **保持双端可构建**。macOS 12+ arm64 + Windows 10+ x64；`-Wall -Wextra`/`/W4` 零警告；`ctest`/`make test` 全绿。
9. **schema 版本化**。设置、校准、报告、模板变更必须升版本并提供迁移/读取兼容。
10. **C ABI 稳定**。核心接口 `extern "C"`；不传 STL 类型；异常不得穿透 C 代码。
11. **小步提交**。一个 PR 只做一个任务；≤ 400 行变更；可回滚。

---

## 3. 构建与测试

### 3.1 双端 CMake（0.1-alpha 起）

```bash
# macOS
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=macos
cmake --build build
ctest --test-dir build --output-on-failure

# Windows
cmake -S . -B build -DAUDIOSCOPE_PLATFORM=windows -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### 3.2 旧 Makefile（v1.1 兼容）

```bash
make            # build/AudioScope
make test       # --selftest + --unittest
make asan       # AddressSanitizer
make shot       # build/shots/*.png
make app        # build/AudioScope.app
make dmg        # build/AudioScope-<ver>.dmg
make release    # dist/ + SHA256SUMS
make win-core   # 纯 C 核心的 mingw 交叉编译
```

新增测试命令（按路线图逐步加入）：

```bash
make tsan       # ThreadSanitizer（音频线程）
make fuzz       # JSON/settings/report 解析模糊测试
make golden     # 黄金文件回归
```

**规则**：

- 提交前必须跑双端 `cmake build` + `ctest`；UI 变更加双端截图；音频/线程变更加 ASan/TSan。
- 不允许“测试先注释掉再说”。
- 不允许为了过测试而削弱断言。
- 测试必须能在旧代码上失败、在新代码上通过。
- 如果本机只有 macOS，Windows 结果以 CI 为准；PR 必须贴 CI 链接。

---

## 4. 仓库地图

**v1.1 现有结构（保留兼容）**

```
src/
├── dsp.c/h           FFT、窗、触发、统计、发生器
├── audio.c/h         CoreAudio 引擎（当前立体声）
├── ui.c/h            立即模式控件、材质、字体
├── theme.c/h         设计令牌
├── view.c            四个页签的绘制（当前 45KB，待拆分）
├── app.c/h           状态、设置、主循环、导出
├── wavegen.c/h       测试信号
├── calib.c/h         校准数据
├── analyze.c/h       采集分析
├── report.c/h        JSON/CSV/WAV 报告
├── loopback.c/h      闭环流程状态机
├── main.m            AppKit 外壳、菜单、事件
└── tests/            selftest.c、unit.c
```

**0.7-alpha 目标结构（增量迁移）**

```
CMakeLists.txt
src/
├── core/             C11，extern "C"，零平台依赖
├── app/              C++17，状态/设置/会话
├── ui/               C++17，Canvas + views
├── platform/macos/   Objective-C++/C，CoreAudio/CoreGraphics/AppKit
├── platform/win/     C++/Win32，WASAPI/Direct2D/DirectWrite
└── main/             main.mm / main_win.cpp
tests/
├── core/             C tests
├── app/              C++ tests
├── golden/
└── sim/
```

迁移必须增量：先 CMake/C ABI，再接口，再 Windows 后端；每步双端可构建、可回滚。详见 `ROADMAP.md` §7。

---

## 5. 实时音频规则

音频回调（`render_output` / 输入回调）只允许：

- 读写音频缓冲。
- 写无锁 ring。
- 生成波形。
- 更新原子峰值/计数。
- 读取已原子交换的配置快照。

**禁止**：

- `malloc/calloc/free/realloc`。
- `pthread_mutex_*`、`std::mutex`、信号量、条件变量。
- 文件/网络 IO。
- `printf/fprintf/syslog`。
- Objective-C / CoreFoundation / COM 调用（除非无分配且已证明实时安全）。
- C++ 异常、RTTI、`new/delete`。
- STL 容器/字符串/`std::function`（`std::atomic` 可用）。
- 长度不可控的循环。
- 等待 UI/分析线程。

**配置传递**：

- 双缓冲 + 原子指针交换，或 SPSC 命令队列。
- 不使用“音频线程加锁读配置”的方案。
- 现有 `ring_write()` / `render_output()` 中的互斥锁是已知技术债，由 **CH-07** 移除；新代码不得再引入。
- Windows：COM 在每个线程显式初始化；音频回调不做 COM 调用。

**测试**：

- 音频回调压力测试：8ch/48k/128 帧，10 分钟，xrun = 0。
- TSan：无数据竞争。
- 回调内分配检测：hook `malloc` 或使用 Instruments。
- 双端 HIL：macOS 2 款 + Windows 2 款接口。

---

## 6. 安全不变量

任何涉及采集、量程、输入模式、校准、输出的改动，必须检查：

1. 未完成预检不能进入 `RUNNING`。
2. 超出量程必须阻断或明确警告；不得只写日志。
3. Clip 必须锁存；不得只显示一帧。
4. Clip 后 100 ms 内输出静音；必须人工复位。
5. 输出默认限幅 −20 dBFS；启动静音 200 ms。
6. 输入模式不得被任何代码路径自动切换。
7. 未校准不得显示电压；只能 dBFS 或“假设”。
8. 设备丢失/采样率变化时，校准失效并提示。
9. 不得引导用户直连市电/喇叭输出/未衰减胆机。
10. 所有安全事件写入事件日志。

**故障注入测试**（必须存在）：

- 10 Vpp 输入（仿真）。
- 直流偏移。
- 平顶削波。
- NaN/Inf 样本。
- 快速增益变化。
- 设备拔出。
- 采样率切换。
- 校准文件损坏/不匹配。

---

## 7. 校准与单位

- `dBFS` 是唯一无需校准的事实。
- 电压显示必须带：`measured` / `datasheet` / `assumed` 三选一。
- 校准按 `device_uid + input_channel + gain_db + pad_db + sample_rate` 绑定。
- 设备/采样率/增益变化 → 校准失效；不得静默沿用。
- 校准文件写入原子化（临时文件 + rename）；带 checksum。
- 单位换算集中在 `core/calib/units.c`；UI 不重复实现。
- 正弦假设必须标注；非正弦用波峰因数。

**精度目标**：1 kHz、−20 dBFS、已校准，幅度误差 < 0.1 dB。

---

## 8. 输入模式

- 模式：Line Pro、Line Consumer、DI (balanced line)、Inst、Mic、Probe 10:1、Speaker-via-pad。
- 每种模式定义：标称电平、典型最大、输入阻抗、安全上限、phantom 要求。
- “信号检查助手”只给建议 + 理由 + 置信度；用户确认才切换。
- 任何自动切换都是 P0 缺陷。
- 双阻抗源测试仅在硬件前端支持时启用；否则隐藏。

---

## 9. 报告与 schema

- 报告 canonical 为 **HTML**（自包含、离线可开）；PDF 由系统生成。
- 报告必须包含：app/OS 版本、设备 UID、校准 ID/置信度、采样率、增益档、设置哈希、限值、判定、时间戳。
- 不得包含绝对路径/用户名，除非用户主动填写。
- 写入原子化；失败不得留下半个文件。
- schema 变更：升 `schema_version`；保留旧读取器；提供迁移。
- 报告数字必须可追溯到 run/session/校准。

---

## 10. UI 规则

- 沿用现有 theme token：暖灰石墨、鼠尾草绿重点色、琥珀警告、砖红错误。
- 4px 网格；图表 32px 安全区；单一重点色；无霓虹/发光。
- 热力图不用彩虹色；使用单调亮度、色盲友好色标。
- 数字等宽；真减号 −；缺失 —；范围 –。
- 所有交互可键盘到达；焦点环可见；不只靠颜色传达状态。
- UI 变更必须用 `--shot`/`make shot` 生成双端截图，并在 PR 中附上。
- 文案规则：说“配合前端可降低风险”，不说“保护声卡”；说“已校准 ±0.1 dB”，不说“精确”。

---

## 11. 代码风格

- **C11 核心**：4 空格缩进；不重排无关代码。
- **C++17 应用/平台**：4 空格缩进；RAII；`std::string_view`/`optional`/`variant` 优先；不在头文件 `using namespace`。
- 文件：C 用 `.c/.h`；C++ 用 `.cpp/.hpp`；macOS 桥接用 `.mm`；核心头文件必须可被 C 和 C++ 同时包含。
- C ABI：`extern "C"`；opaque struct；不传 STL 类型；异常在边界捕获。
- 命名：C `as_*` / `AS*`；C++ 命名空间 `audioscope::`，类型 `PascalCase`，函数 `snake_case`（与现有 C 风格一致）。
- 头文件 include guard：`AS_<MODULE>_H`；C++ 可用 `#pragma once`。
- 公共 API 在 `.h/.hpp` 中注释；实时约束必须写明。
- 错误处理：返回码/布尔/`std::optional`；禁止静默失败；用户可见错误要有文案。
- 不使用 VLA；不在栈上放超大缓冲；固定上限用宏/`std::array`。
- 浮点比较用容差；音频样本用 `float`，分析用 `double`。
- 注释语言可用中文或英文；同一文件保持一致。
- 编译：C 核心 `-std=c11 -fno-exceptions -fno-rtti`；C++ `-std=c++17`；MSVC `/std:c++17 /EHs-c- /GR-`（核心）。

---

## 12. Git / PR

- 分支：`feat/<req-id>-<slug>`、`fix/<req-id>-<slug>`、`docs/<slug>`。
- 提交：`<type>(<scope>): <summary>`，例如 `feat(calib): add per-gain calibration profile`。
- PR 标题：`[CAL-03] Calibration wizard core`。
- 一个 PR 一个任务；≤ 400 行；超过则拆分。
- PR 描述必须包含：改了什么、为什么、测试证据、截图（UI）、风险、回滚。
- 不直接 push main；不 force push 共享分支。
- 不提交构建产物、用户数据、校准文件、日志。

---

## 13. Agent 工作循环

```
1. 读 AGENTS.md → ROADMAP.md / ALPHA-0.1.md 相关章节 → BACKLOG.md 任务卡
2. 复述任务与验收标准（写在 PR 描述里）
3. 找现有模式；不发明新架构
4. 写失败测试（单元/黄金/仿真）
5. 最小实现
6. cmake build + ctest（双端）→ ASan → shot（UI）
7. 自审：实时/线程/安全/schema/隐私/C ABI
8. 更新文档与 CHANGELOG
9. 提 PR
```

**遇到不确定**：

- 产品决策（是否自动切换、是否显示电压、是否支持某设备）→ 停下来问，不猜。
- 技术实现有多种方案 → 选最小、可回滚、与现有模式一致的。
- 发现范围蔓延 → 拆任务，不扩 PR。
- 发现旧代码违反本协议 → 记录 issue；除非任务要求，不在同一 PR 大改。

---

## 14. Definition of Done

- [ ] 验收标准全部满足。
- [ ] 新增/更新测试；`ctest`/`make test` 双端绿。
- [ ] ASan/UBSan 干净；音频/线程变更 TSan 干净。
- [ ] UI 变更双端截图，`--shot` 无回归。
- [ ] 无新编译警告（`-Wall -Wextra -Werror` / `/W4 /WX`）。
- [ ] 文档/CHANGELOG 更新。
- [ ] 报告/设置/校准 schema 版本正确，旧数据可读。
- [ ] 安全不变量自查通过。
- [ ] C ABI：无 STL 类型、无异常穿透。
- [ ] 隐私：无网络、无 PII、无绝对路径。
- [ ] 性能预算未退化（启动、UI、回调）。
- [ ] PR 描述含双端测试证据、风险与回滚。

---

## 15. Review 清单

- [ ] 音频回调是否 0 分配/0 锁/0 IO/0 STL/0 异常？
- [ ] C ABI 是否只传 C 类型？异常是否在边界捕获？
- [ ] macOS 与 Windows 是否都能构建/测试？
- [ ] 配置是否原子交换？命令是否 SPSC？
- [ ] 设备热插拔/采样率切换是否处理？
- [ ] 校准失效是否提示？电压是否带置信度？
- [ ] 输入模式是否可能被静默切换？
- [ ] 超量程/削波是否阻断/锁存/静音？
- [ ] 报告 schema 是否兼容？写入是否原子？
- [ ] 错误路径是否有用户可读文案？
- [ ] 是否引入依赖/网络/遥测？
- [ ] 是否影响 Apache-2.0/NOTICE？
- [ ] 是否只改了任务相关代码？
- [ ] 是否有测试能在旧代码上失败？

---

## 16. 禁止事项

- 禁止新增第三方库、包管理器、网络请求。
- 禁止在音频回调中分配内存/加锁/写文件/用 STL/抛异常。
- 禁止让 C++ 异常或 STL 类型穿过 C ABI。
- 禁止删除或弱化安全预检、限幅、削波锁存。
- 禁止静默自动切换输入模式/量程/通道。
- 禁止在未校准时显示“精确电压”。
- 禁止声称“软件保护声卡”。
- 禁止把用户数据上传。
- 禁止大范围格式化/重命名无关文件。
- 禁止伪造测试结果或跳过失败测试。
- 禁止在 PR 中混入多个不相关任务。

---

## 17. 文档更新

| 变更 | 更新 |
|---|---|
| 新功能 | README、CHANGELOG、相关 docs |
| 新 schema | `docs/schema/*.md` + 迁移说明 |
| 新快捷键 | README 快捷键表 |
| 新模板 | `docs/templates/*.md` + 示例报告 |
| 安全相关 | `docs/safety.md` + 首次引导文案 |
| 平台支持 | README 平台表 + 构建说明 |
| alpha 相关 | `ALPHA-0.1.md` + `KNOWN-ISSUES.md` + README 下载说明 |

---

## 18. 性能预算

| 项目 | 预算 |
|---|---|
| 启动 | < 0.2 s（M1 冷启动）；Windows < 0.5 s |
| UI | 30 Hz；交互到重绘 < 33 ms |
| 8ch/48k/128 帧 CPU | < 10%（M1）；Windows 同等机器 < 15% |
| 128k FFT | < 50 ms |
| 报告生成 | < 5 s |
| 稳态内存 | < 200 MB |
| 长时监控 | 8 h 内存增长 < 5% |
| 双端音频延迟 | alpha 共享模式 < 30 ms（往返，文档标注） |

超预算必须说明原因并给出后续优化 issue。

---

## 19. 安全与隐私

- 默认无网络；macOS 用 Little Snitch/`fs_usage`，Windows 用 Resource Monitor/防火墙验证。
- 崩溃日志本地；上传需显式同意。
- 报告默认不含用户名/绝对路径。
- 设备 UID 哈希后存储。
- 校准/报告文件权限：macOS 0600；Windows 用户目录 ACL。
- 依赖仅系统框架 + C/C++ 标准库；无供应链风险。
- Windows alpha 不签名，但必须在 README/首启说明 SmartScreen 警告。

---

## 20. 发布

- 版本号：`MAJOR.MINOR.PATCH`；alpha 用 `0.7.0-alpha`；schema 独立版本。
- 发布前跑完整门禁（见 `ROADMAP.md` §8.3），双端 CI 必须绿。
- tag → macOS/Windows 构建 → 签名/公证（v2.0）→ release notes。
- macOS：`.app`/DMG；Windows：便携 ZIP（alpha）→ MSI/EXE（v2.0）。
- release notes 必须包含：新功能、修复、已知问题、安全提示、升级/迁移说明、双端限制。
