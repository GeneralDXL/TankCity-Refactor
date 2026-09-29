cmake_minimum_required(VERSION 3.16)

project(tankll2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

# 添加测试支持
# enable_testing()
# find_package(GTest CONFIG REQUIRED)

set(PROJECT_SOURCES
        main.cpp
        widget.cpp
        widget.h
        widget.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(tankll2
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        loginwindow.h loginwindow.cpp
        mainwindow.h mainwindow.cpp
        gamewindow.h gamewindow.cpp
        # player.h player.cpp
        # enemy.h enemy.cpp
        # bullet.h bullet.cpp
        # tank.h tank.cpp
        # map.h map.cpp
        scoreboard.h scoreboard.cpp
        registerwindow.h registerwindow.cpp
        painter.h painter.cpp
        # wall.h wall.cpp
        loadingwidget.h loadingwidget.cpp
    )
else()
    if(ANDROID)
        add_library(tankll2 SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(tankll2
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(tankll2 PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets 
    Qt${QT_VERSION_MAJOR}::Network
)

# # 添加测试可执行文件
# add_executable(tankll2_tests
#     # 测试源文件
#     tests/test.cpp  # 示例测试文件，需根据实际情况添加
# )

# target_link_libraries(tankll2_tests PRIVATE
#     Qt${QT_VERSION_MAJOR}::Widgets
#     Qt${QT_VERSION_MAJOR}::Network
#     GTest::gtest
#     GTest::gtest_main
#     GTest::gmock
#     GTest::gmock_main
# )

# # 添加测试
# add_test(NAME tankll2_tests COMMAND tankll2_tests)

# 设置目标属性
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.tankll2)
endif()
set_target_properties(tankll2 PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS tankll2
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(tankll2)
endif()

option(CODE_COVERAGE "Enable coverage reporting" OFF)

if(CODE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Building with code coverage flags")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -O0 -g")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()
endif()
