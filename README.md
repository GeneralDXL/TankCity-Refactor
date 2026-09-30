# TankCity · Refactor

> 把一个 2025 年的学生课设坦克游戏，系统性 **重构 / 翻新 / 维护** 为「架构清晰、配置驱动、手感在线」的工程作品。

**状态**：🔄 重构进行中（已完成 **M0**、**M2**，正在做 **M1.1 引擎分层**）

---

## 项目背景

2025 年，5 名武汉大学学生完成课程作业《DevOps 初级项目实践》，用 **Qt / C++** 从零搭建了一个坦克大战游戏，包含 **TCP 客户端 / 服务端** 两部分。

受课程重心（Jenkins / SonarQube / Docker 流水线）影响，代码本身留下了不少技术债：

- 关卡地图**硬编码**在 C++ 源码里，扩展关卡 = 改代码重编译；
- 资源没有 `.qrc`，靠 `../../assets` 相对路径加载，换个工作目录就失效；
- **音频功能从未实现**（有素材、无代码）；
- 构建脚本**硬编码了作者本机的 Qt 路径**，他人无法编译；
- 无单元测试、无 CI。

2026 年，由我主导对其进行**系统性重构**。

---

## 重构目标

| 维度 | 目标 |
|---|---|
| 架构 | 分层：`engine` / `game` / `client` / `server` / `shared`，依赖单向 |
| 数据 | **配置驱动** —— 关卡 / 实体 / 物块用 JSON 描述，改数据不改代码 |
| 玩法 | **双摇杆操作** + 平滑八向移动 + 鼠标独立瞄准 |
| 机制 | `wall` / `block` 双体系：反弹、可破坏、隐身、惯性…… |
| 创新 | 地形即资源 / 反弹技巧 / 物块词条 |
| 网络 | 单机 = 「本地联机」，与联机共用同一套逻辑 |
| 工程 | 可移植构建、单元测试、GitHub Actions |

### 路线图

- [x] **M-1** 工程规范（Git 工作流 / clang-format / 测试记录制度）
- [x] **M0** 可移植构建、清理冗余、GoogleTest 骨架
- [ ] **M1** 引擎骨架抽取（engine / game 分层）
  - [x] **M1.0** 目录与构建结构（`client` / `server` / `tests` 提升到顶层）
  - [ ] **M1.1** 逐个抽取 engine 子模块
- [x] **M2** 配置驱动（关卡 / 实体 / 物块 JSON 化）
- [ ] **M3** 世界与玩法重构（tile 世界、双摇杆、A*）
- [ ] **M4** 玩法创新落地（地形即资源 / 反弹 / 物块词条）
- [ ] **M5** 网络统一（单机 = 本地联机）
- [ ] **M6** 音画表现（音频、贴图、资产优化）
- [ ] **M7** 打击感 / Juice
- [ ] **M8** 工程收尾（测试、CI、文档、发布）

---

## 技术栈

- **C++17**
- **Qt 6**（Widgets / Network）—— 渲染后端设计为**可插拔**：QPainter 先行，OpenGL 预留
- **CMake** 构建
- **GoogleTest**（计划）
- **GitHub Actions**（计划）

## 目录结构

```
.
├── client/     # Qt 表现层
├── server/     # 权威游戏逻辑
├── tests/      # 单元测试（GoogleTest + CTest）
├── tools/      # 开发工具（服务端监控）
├── docs/       # 文档
└── .github/    # CI（GitHub Actions）
```

> `engine/`（引擎层）、`game/`（玩法层）、`shared/`（共享协议与配置模型）将在 **M1.1** 抽取时建立。

## 构建与运行

需要 **CMake ≥ 3.16** 与 **Qt 6**（MinGW 或 MSVC 工具链）。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<你的 Qt 安装前缀>"
cmake --build build
ctest --test-dir build --output-on-failure   # 运行单元测试
```

- 构建产物：`build/bin/tankcity_client.exe`、`build/bin/tankcity_server.exe`
- 首次配置会通过 FetchContent 下载 GoogleTest；离线时可用 `-DBUILD_TESTING=OFF` 跳过测试。

**启动**（先服务端，再客户端）：

```bash
./build/bin/tankcity_server                    # 默认监听 0.0.0.0:12345
./build/bin/tankcity_server 127.0.0.1 12345    # 也可显式指定 <IP> <port>
./build/bin/tankcity_client                    # 客户端连 127.0.0.1:12345
```

- 服务端是**控制台程序**，`qInfo()` / `qCritical()` 直接打在终端上：配置写错时会报出
  「文件 + 字段路径 + 原因」，例如 `entities.json: bullets.bulletBasic —— 穿甲与弹射互斥：…`。
- 数值与关卡都是数据文件（`assets/config/*.json`、`assets/levels/level_NN.json`）：
  新增一张关卡只需把 `level_11.json` 放进 `assets/levels/`，**不必改代码**。

## 操作方式

- **原始版本**：`W/S` 前进 / 后退，`A/D` 原地旋转
- **目标版本**：`WASD` 平滑八向移动 + 鼠标独立瞄准 + 左键开火（双摇杆）

---

## 项目沿革与署名

**2025 · 原始团队（武汉大学课程作业）**

| 成员 | 负责 |
|---|---|
| 段绪礼 (GeneralDXL) | 后端类设计、客户端 GUI 与贴图 |
| 张昶宇 (todayair) | 核心玩法、DevOps / CI、测试 |
| 周梓欣 (zzxzdzx) | 敌人 AI（A\*）、关卡、双人模式、文档 |
| 赵天星 (x1a0qiya) | TCP Socket 网络通信 |
| 李志豪 (lzh) | 参与开发 |

**2026 · 重构**：段绪礼 (GeneralDXL) 主导。

---

## 许可证

待定（TBD）—— 正式公开前将与原始团队成员确认。
