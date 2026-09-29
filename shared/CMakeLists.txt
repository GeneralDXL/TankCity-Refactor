# 共享层：配置模型与加载器（客户端 / 服务端 / 测试共用）
# 只依赖 Qt6::Core —— 不引入 Widgets，保证可被任意层引用。

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(tankcity_shared STATIC
    config/ConfigError.h
    config/ConfigError.cpp
    config/ConfigTypes.h
    config/ConfigTypes.cpp
    config/ConfigLoader.h
    config/ConfigLoader.cpp
    config/TankStats.h
    config/TankStats.cpp
)

target_include_directories(tankcity_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tankcity_shared PUBLIC
    Qt6::Core
)
