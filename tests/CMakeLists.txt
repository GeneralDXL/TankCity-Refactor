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
    GTest::gtest_main
)

add_test(NAME tankcity_unit_tests COMMAND tankcity_tests)
