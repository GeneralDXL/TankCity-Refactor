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
