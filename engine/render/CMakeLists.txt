# 引擎渲染：与游戏无关的绘制原语 + 贴图缓存。
#
# 依赖 Qt6::Gui（QPixmap / QPainter 都在 Gui，不在 Widgets）——
# 引擎各模块里只有本模块可以碰 Gui；**任何 engine 模块都不依赖 Qt6::Widgets**。

find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(tankcity_engine_render STATIC
    IRenderer.h
    QPainterBackend.h
    QPainterBackend.cpp
    TextureCache.h
    TextureCache.cpp
)

target_include_directories(tankcity_engine_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(tankcity_engine_render PUBLIC
    Qt6::Core
    Qt6::Gui
)
