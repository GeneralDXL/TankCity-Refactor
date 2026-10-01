# 游戏层：实体与世界（坦克 / 玩家 / 敌人 / 子弹 / 墙体 / 关卡地图）
# 只依赖 Qt6::Core 与 shared/config —— **不得** include client/ 或 server/。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_game STATIC
    entity/tank.h
    entity/tank.cpp
    entity/player.h
    entity/player.cpp
    entity/enemy.h
    entity/enemy.cpp
    entity/bullet.h
    entity/bullet.cpp
    entity/wall.h
    entity/wall.cpp
    world/map.h
    world/map.cpp
)

# 两个子目录都放进 include 路径，这样搬家前既有的 `#include "map.h"` / `"tank.h"`
# 写法无需改动 —— 本次是纯搬家。把 include 规范成 `game/entity/xxx.h` 的做法
# 留到 M3 随 tile 世界一起做，那时这两行会被删掉。
target_include_directories(tankcity_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/entity
    ${CMAKE_CURRENT_SOURCE_DIR}/world
)

target_link_libraries(tankcity_game PUBLIC
    Qt6::Core
    tankcity_engine_physics   # 几何查询：相交、格点换算、走线（M1.1 从 Map 里抽出来的）
    tankcity_shared   # 关卡几何与实体数值都来自 assets/*.json
)
