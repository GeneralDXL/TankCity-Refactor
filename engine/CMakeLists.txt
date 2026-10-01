# 引擎层：与游戏无关的通用设施（循环 / 渲染 / 物理 / 输入 / 资源 / UI 容器）。
#
# 硬性纪律（见 docs/plans/M1.1-引擎分层草案.md §3.1）：
#   1. 本目录内不得出现任何具体游戏领域概念的字样（实体、地图、物块……），
#      字面清单与可执行的检查命令见 docs/plans 的分层草案 §3.1 / §9；
#   2. 不得 include game/、client/、server/、shared/；
#   3. 不得依赖 Qt6::Widgets（engine/render 的 QPainterBackend 例外）。
#
# 各子模块一个静态库，彼此单向依赖。

add_subdirectory(core)
add_subdirectory(physics)
