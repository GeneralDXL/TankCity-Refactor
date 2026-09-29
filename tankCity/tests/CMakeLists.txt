# 单元测试目标（M0 骨架）
# 使用 GoogleTest：优先使用系统已安装的，否则通过 FetchContent 获取。

# 被测源码含 Q_OBJECT（Map），需要 moc
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../server/wall.cpp
)

target_include_directories(tankcity_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_link_libraries(tankcity_tests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    GTest::gtest_main
)

add_test(NAME tankcity_unit_tests COMMAND tankcity_tests)
