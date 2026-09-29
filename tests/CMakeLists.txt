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
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/wall.cpp
)

target_include_directories(tankcity_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../server
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
    GTest::gtest_main
)

add_test(NAME tankcity_unit_tests COMMAND tankcity_tests)

# ---------------------------------------------------------------------------
# 【临时工具】导出旧关卡数据（assets/levels/*.json 与 tests/data/legacy_levels.json）
# 它依赖旧的 Map::loadMap()，重构完成后无法编译 —— 提交 3 时删除。
# ---------------------------------------------------------------------------
add_executable(tankcity_legacy_dump
    legacy_level_dump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/wall.cpp
)

target_include_directories(tankcity_legacy_dump PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_compile_definitions(tankcity_legacy_dump PRIVATE
    TANKCITY_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(tankcity_legacy_dump PRIVATE
    Qt6::Core
    Qt6::Gui   # map.h / wall.h 引入了 QPainter
    tankcity_shared
)
