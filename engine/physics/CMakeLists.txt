# 引擎物理：几何基元与静态碰撞查询。只依赖 Qt6::Core。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_engine_physics STATIC
    Aabb.h
    Grid.h
    Grid.cpp
    CollisionWorld.h
    CollisionWorld.cpp
)

target_include_directories(tankcity_engine_physics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tankcity_engine_physics PUBLIC
    Qt6::Core
)
