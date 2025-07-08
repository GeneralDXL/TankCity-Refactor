cmake_minimum_required(VERSION 3.16)

project(tank VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 特定配置
if(WIN32)
    # 定义 Windows 版本以启用 Winsock 2 功能
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 及以上
    
    # 定义 NOMINMAX 以避免 Windows 头文件与标准库冲突
    add_definitions(-DNOMINMAX)
endif()

# 查找 Qt 核心、网络和 Widgets 模块
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Widgets)

# add_subdirectory(./googletest-main/googletest) # 替换为你的GTest源码路径
# enable_testing()
# include_directories(./googletest-main/googletest/include) # 替换路径

set(PROJECT_SOURCES
        main.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(tank
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        game.h game.cpp
        player.h player.cpp
        enemy.h enemy.cpp
        bullet.h bullet.cpp
        tank.h tank.cpp
        map.h map.cpp
        wall.h wall.cpp
        server.cpp server.h
    )
else()
    if(ANDROID)
        add_library(tank SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(tank
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# 链接 Qt 模块
target_link_libraries(tank PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Widgets
)

# # 添加 Winsock 2 链接库（仅 Windows 需要）
# if(WIN32)
#     target_link_libraries(tank PRIVATE ws2_32)
# endif()

# # 添加测试可执行文件
# add_executable(tank_tests
#     # 测试源文件
#     tests/test.cpp  # 根据需要添加更多测试文件
# )

# # 链接测试依赖
# target_link_libraries(tank_tests PRIVATE 
#     Qt${QT_VERSION_MAJOR}::Core
#     Qt${QT_VERSION_MAJOR}::Network
#     Qt${QT_VERSION_MAJOR}::Widgets
#     GTest::gtest 
#     GTest::gtest_main 
#     # GTest::gmock 
#     # GTest::gmock_main
# )

# # 如果是 Windows，测试程序也需要链接 ws2_32
# if(WIN32)
#     target_link_libraries(tank_tests PRIVATE ws2_32)
# endif()

# # 添加测试
# add_test(NAME tank_tests COMMAND tank_tests)

# 设置 macOS/iOS 应用属性
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.tank)
endif()
set_target_properties(tank PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS tank
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(tank)
endif()

option(CODE_COVERAGE "Enable coverage reporting" ON)

if(CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Building with code coverage flags")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()

