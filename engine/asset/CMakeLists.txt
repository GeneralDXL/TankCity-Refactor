# 引擎资源：资源定位（按可执行文件而非工作目录）。只依赖 Qt6::Core。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_engine_asset STATIC
    AssetPaths.h
    AssetPaths.cpp
)

target_include_directories(tankcity_engine_asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tankcity_engine_asset PUBLIC
    Qt6::Core
)
