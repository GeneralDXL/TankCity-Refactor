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
