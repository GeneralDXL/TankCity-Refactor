# 引擎输入：与游戏无关的输入状态容器。只依赖 Qt6::Core（Qt::Key 与 QPoint 都在 Core）。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_engine_input STATIC
    InputTracker.h
    InputTracker.cpp
)

target_include_directories(tankcity_engine_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tankcity_engine_input PUBLIC
    Qt6::Core
)
