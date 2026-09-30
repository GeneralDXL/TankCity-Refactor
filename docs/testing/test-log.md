# 测试记录（Test Log）

> 本文件**纳入公开仓库**，记录每次阶段验收的测试结果，由**开发者本人**执行并填写。
> 内部规范见 `docs/standards/testing.md`。

## 说明

- 每完成一个阶段，**至少手动验证一次**，并在此追加记录；
- 失败也要如实记录并附上报错，**不删除失败记录**；
- "验证人"填执行者；"提交"填被测的 commit 短哈希。

## 记录

| 日期 | 阶段 | 提交 | 测试项 | 预期 | 实际 | 结果 | 验证人 | 环境 / 备注 |
|---|---|---|---|---|---|---|---|---|
| 2026-09-28 | M-1 | `531540f` | `clang-format` 可用 | 能正常输出版本号 | `clang-format version 20.1.3` | ✅ | GeneralDXL | Windows；Qt Creator 自带 |
| 2026-09-28 | M-1 | `531540f` | 规范文件就位 | `.clang-format` / `.editorconfig` / `.gitattributes` / `docs/testing/test-log.md` 均存在 | 4 个文件全部存在 | ✅ | GeneralDXL | 终端核实 |
| 2026-09-28 | M-1 | `531540f` | 分支与工作区 | 处于 `chore/m-1-standards` 且工作区干净 | 分支正确；`git status` 无输出 | ✅ | GeneralDXL | 终端核实 |
| 2026-09-29 | M0 | `ca10097` | CMake configure（干净构建目录） | 配置成功 | 成功（25.1s，含 googletest 下载） | ✅ | GeneralDXL | Windows / Qt 6.9.2 / MinGW 13.1 |
| 2026-09-29 | M0 | `ca10097` | 全量构建 | 客户端与服务端均编译链接成功 | 46/46 成功 | ✅ | GeneralDXL | 同上 |
| 2026-09-29 | M0 | `ca10097` | `ctest` | 全部通过 | 1/1 通过（100%） | ✅ | GeneralDXL | 含 3 个地图网格用例 |
| 2026-09-29 | M0 | `ca10097` | GitHub Actions | Ubuntu + Windows 均通过 | success（4m12s） | ✅ | GeneralDXL | run `36448125052` |
| 2026-09-29 | M1.0 | `6301c22` | CMake configure | 配置成功 | 成功（5.9s） | ✅ | GeneralDXL | Windows / Qt 6.9.2 / MinGW 13.1 |
| 2026-09-29 | M1.0 | `6301c22` | 全量构建 | 客户端与服务端均编译链接成功 | 46/46 成功 | ✅ | GeneralDXL | 同上 |
| 2026-09-29 | M1.0 | `6301c22` | `ctest` | 全部通过 | 1/1 通过（100%） | ✅ | GeneralDXL | 同上 |
| 2026-09-29 | M1.0 | `6301c22` | GitHub Actions | Ubuntu + Windows 均通过 | success（3m19s） | ✅ | GeneralDXL | run `36511516536` |
| 2026-09-29 | M1.0 | `6301c22` | 运行游戏（行为不变） | 界面/贴图正常；注册、登录、选关、进入游戏均正常 | 通过 | ✅ | GeneralDXL | 服务端需带 `<IP> <port>` 启动（历史遗留，转 M1.1 处理） |
| 2026-09-30 | M2 | `3ef1b77` | CMake configure（干净构建目录） | 配置成功 | 成功（4.9s） | ✅ | GeneralDXL | Windows / Qt 6.9.2 / MinGW 13.1 / Ninja / Release |
| 2026-09-30 | M2 | `3ef1b77` | 全量构建 | 客户端与服务端均编译链接成功 | 成功（19.0s） | ✅ | GeneralDXL | `tankcity_client` / `tankcity_server` / `tankcity_tests` 均产出 |
| 2026-09-30 | M2 | `3ef1b77` | `ctest` | 全部通过 | 1/1 通过（100%，0.16s） | ✅ | GeneralDXL | 内含 54 个 gtest 用例 / 12 个套件全绿 |
| 2026-09-30 | M2 | `3ef1b77` | GitHub Actions | Ubuntu + Windows 均通过 | success（ubuntu 2m46s / windows 4m49s） | ✅ | GeneralDXL | run `36612035952`（PR #5） |
| 2026-09-30 | M2 | `3ef1b77` | DoD 2：数值只在 JSON 里定义 | `200000` / `2.5f` / `shootDelay = 60` 从 C++ 消失 | 三个锚点均清除（`2.5f` 仅剩 `enemy.cpp` 的 `i * 22.5f` 16 方向寻路角） | ✅ | GeneralDXL | 等价值由 `ConfigResolve.*` 5 项钉死 |
| 2026-09-30 | M2 | `3ef1b77` | DoD 4：未知字段 / 互斥规则的报错定位 | 报出「文件 + 字段路径 + 原因」 | 两分支均报出，如 `entities.json: bullets.bulletBasic —— 穿甲与弹射互斥：…` | ✅ | GeneralDXL | 对正式 `entities.json` 做变异验证，已还原 |
| 2026-09-30 | M2 | `3ef1b77` | DoD 5：新增关卡免改 C++ | 放进 `level_11.json` 即出现在列表，序号可回传服务端 | `LevelList.NewLevelFileAppearsWithoutCppChange` 通过（列表 11 项；序号 10 → `level_11.json`） | ✅ | GeneralDXL | 菜单实际呈现见下一行 |
| 2026-10-01 | M2 | `3ef1b77` | 运行游戏（行为不变） | 界面/贴图正常；注册、登录、选关、进入游戏均正常；选关列表显示关卡文件里的 10 项名称 | 通过 | ✅ | GeneralDXL | 已知偏差：玩家血条按新尺度显示「10%」（M6 重做 UI）；服务端仍需带 `<IP> <port>` 启动（M1.1） |
| 2026-10-01 | M1.1 | `9be6b95` | 全量构建（基线） | 三个目标均产出 | 成功（38 步）；`bin/{tankcity_client,tankcity_server,tankcity_tests}.exe` | ✅ | AI（代跑） | Windows / Qt 6.9.2 / MinGW 13.1 / Ninja，Terminal 实测 |
| 2026-10-01 | M1.1 | `9be6b95` | `ctest`（基线） | 全部通过 | 1/1 通过（0.52s） | ✅ | AI（代跑） | 与 M0/M1.0/M2 记录一致 |
| 2026-10-01 | M1.1 | `16fb6c4` | 服务端无参启动 | 回退 `0.0.0.0:12345` 并正常服务 | 直接运行 `tankcity_server.exe`（无参数）→ stderr `Server started on "0.0.0.0" : 12345`；`0.0.0.0:12345` 处于 LISTEN | ✅ | AI（代跑） | `<IP> <port>` 现已可选；README 已写明启动方式 |
| 2026-10-01 | M1.1 | `3146c84` | `connect()` 误用修复 | 启动无 `signal not found` 警告 | 启动输出仅一行，无任何 `QObject::connect` 警告 | ✅ | AI（代跑） | 该连接此前把 slot 当 signal，从未生效 |
| 2026-10-01 | M1.1 | `9be6b95` | **第 0 步：基线行为清单** | 下方 §M1.1 基线 15 项全部符合 | ⏳ 待 GeneralDXL 走查确认 | ⏳ | GeneralDXL | 3 张截图待存入 `docs/testing/m1.1-baseline/` |

---

## M1.1 引擎分层：重构前基线（第 0 步）

> 依据 `docs/plans/M1.1-引擎分层草案.md` §6。**基线 = `9be6b95`**（`main` + tag `m2`）。
> **前置条件**：客户端必须从 `build/bin` 启动（`cd build/bin; ./tankcity_client`）——贴图走 `./../../assets` 相对路径，
> 从仓库根或 `build/` 启动会**静默丢失全部贴图**，直接污染基线截图。服务端不受影响（已用 `applicationDirPath()`）。
> M1.1 变动的是**运行时行为与画面**，无法做数据级等价对比（M2 的做法不适用），
> 因此以「**截图 + 行为清单**」作为唯一对照基准：此后每一步都要重跑本清单并**逐项**比对。
> 清单里凡是标注「基线即如此」的，都是**既有行为**——重构中一旦变化即视为回归。

### 固定截图（存 `docs/testing/m1.1-baseline/`）

| 文件 | 场景 | 进入方式 | 用途 |
|---|---|---|---|
| `01-open-level01.png` | 开阔关初始 | 选关第 1 项「地图 1」（10 个阻挡物） | 比对地图布局 / 贴图 / HUD 基线 |
| `02-maze-level04.png` | 迷宫关初始 | 选关第 4 项「砖墙迷宫」（31 个阻挡物） | 比对碰撞与敌人绕墙寻路 |
| `03-combat.png` | 交火中 | 关卡内移动 + 开火，待敌人/子弹/道具同屏时截 | 比对弹道、敌人、道具、HUD |

> 要求：同一窗口尺寸、同一关卡、尽量同一初始位置；**不要**用不同分辨率重截，否则对比失效。

### 行为清单（逐项走查确认）

| # | 检查项 | 预期（基线行为） |
|---|---|---|
| 1 | 服务端启动 | 无需参数，输出 `Server started on "0.0.0.0" : 12345`，**无** `QObject::connect` 警告 |
| 2 | 客户端启动 | 登录窗口正常显示，贴图无缺失 / 无白块 |
| 3 | 注册 / 登录 | 可注册并登录进入主菜单（旧账号亦可） |
| 4 | 选关列表 | 显示 **10 项**，名称与 `assets/levels/*.json` 的 `name` 一致：地图 1 … 环形孤岛 |
| 5 | 进入游戏 | 10 关**均可进入**，布局与 JSON 一致（第 4 关是砖墙迷宫、第 10 关是环形孤岛） |
| 6 | 操作方式（**坦克式，M3 才会改**） | **`W`/`S` 前进后退、`A`/`D` 左右转向**（`server/game.cpp:301-304`），非八向平移 |
| 7 | 瞄准 | 炮塔独立跟随鼠标指向（不随车身） |
| 8 | 开火 | **鼠标左键单击**开火一次；长按**不**连发（`sendKey()` 每次发完即清除左键状态，`gamewindow.cpp:598`） |
| 9 | 碰撞 | 子弹撞墙即消失（`game.cpp:598` 直接 erase）；子弹命中敌人扣血；坦克撞墙停下**不穿透**；敌人寻路绕墙 |
| 10 | 无敌帧 | 玩家被击中后短暂无敌期间，敌方子弹被忽略（`game.cpp:609`） |
| 11 | HUD | 血条 / 分数 / 敌人计数显示正常 |
| 12 | **已知偏差（预期如此，勿顺手修）** | 玩家血条按新尺度显示为「**10%**」（归 M6 重做 UI） |
| 13 | 道具 | 刷新出现且可拾取，效果生效 |
| 14 | 结算 | 击杀全部敌人 → 胜利结算；玩家死亡 → 失败结算 |
| 15 | 音频 | **无音频**（基线即如此，音频属 M6） |
