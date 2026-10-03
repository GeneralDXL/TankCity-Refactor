# 单元测试
# 使用 GoogleTest：优先使用系统已安装的，否则通过 FetchContent 获取。

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(tankcity_tests
    map_grid_test.cpp
    config_test.cpp
    level_test.cpp
    engine_core_test.cpp
    engine_physics_test.cpp
    client_render_test.cpp
    engine_input_test.cpp
    client_net_test.cpp
    engine_asset_test.cpp
    player_movement_test.cpp
    pathfinder_test.cpp
    bullet_damage_test.cpp
    enemy_ai_test.cpp
    # 分帧器也编进测试目标（与 painter.cpp 同理）
    ${CMAKE_SOURCE_DIR}/client/net/MessageFramer.cpp
    ${CMAKE_SOURCE_DIR}/client/net/MessageParser.cpp
    # 客户端的地图绘制：编进测试目标，才能用「假渲染器」把绘制调用断言下来。
    # painter.cpp 里还有坦克/敌人等 Painter（链接时需要 QtGui，测试目标已链接 Qt6::Widgets）。
    ${CMAKE_SOURCE_DIR}/client/render/painter.cpp
)

# 测试会编入 client/render/painter.cpp，所以 client/ 也要作为头文件根
# （这样 `#include "render/painter.h"` 的写法与客户端源码里完全一致）
target_include_directories(tankcity_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/client
)

# 配置测试需要读取仓库内真实的 assets/config、assets/levels 与旧关卡快照
target_compile_definitions(tankcity_tests PRIVATE
    TANKCITY_CONFIG_DIR="${CMAKE_SOURCE_DIR}/assets/config"
    TANKCITY_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
    TANKCITY_LEVELS_DIR="${CMAKE_SOURCE_DIR}/assets/levels"
    TANKCITY_LEGACY_SNAPSHOT="${CMAKE_SOURCE_DIR}/tests/data/legacy_levels.json"
)

target_link_libraries(tankcity_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    tankcity_shared
    tankcity_game   # 地图与墙体：改为链接 game 库，不再把源文件重复编一遍
    tankcity_engine_core   # 固定步长循环：engine_core_test 直接测它
    tankcity_engine_physics   # 几何与碰撞查询：engine_physics_test 直接测它
    tankcity_engine_render    # 绘制原语与贴图缓存：client_render_test 用它做假渲染器
    tankcity_engine_input     # 输入状态容器：engine_input_test 直接测它
    tankcity_engine_asset     # 资源定位：engine_asset_test 直接测它
    GTest::gtest_main
)

add_test(NAME tankcity_unit_tests COMMAND tankcity_tests)
