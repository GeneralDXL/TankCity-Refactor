cmake_minimum_required(VERSION 3.16)

project(TankCity VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# 全局设置
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 统一输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 代码覆盖率（默认关闭）
option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Qt 定位：不硬编码本机路径。
# 通过环境变量 CMAKE_PREFIX_PATH，或在 configure 时传入 Qt 安装前缀：
#   cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt 安装前缀>"

include(CTest)  # 定义 BUILD_TESTING（默认 ON）并启用测试

add_subdirectory(client)
add_subdirectory(server)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
