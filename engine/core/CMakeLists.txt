# 引擎核心：逻辑循环与时间。只依赖 Qt6::Core。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_engine_core STATIC
    GameLoop.h
    GameLoop.cpp
)

# include 根设为 engine/，所以引用写法是 #include "core/GameLoop.h"
target_include_directories(tankcity_engine_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tankcity_engine_core PUBLIC
    Qt6::Core
)
