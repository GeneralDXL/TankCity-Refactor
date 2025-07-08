cmake_minimum_required(VERSION 3.10)
project(TankWar)

# 设置统一输出目录（关键修复）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 验证路径
message(STATUS "可执行文件将输出到: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

set(CMAKE_PREFIX_PATH "E:/Qt6/Mingw/6.8.1/mingw_64")
add_subdirectory(client)
add_subdirectory(server)