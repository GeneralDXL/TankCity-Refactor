cmake_minimum_required(VERSION 3.16)
project(TankWar)

# 设置统一输出目录（关键修复）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 验证路径
message(STATUS "可执行文件将输出到: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Qt 定位：不硬编码本机路径。
# 请通过环境变量 CMAKE_PREFIX_PATH，或在 configure 时传入 Qt 安装前缀：
#   cmake -S tankCity -B build -DCMAKE_PREFIX_PATH="<Qt 安装前缀>"
add_subdirectory(client)
add_subdirectory(server)