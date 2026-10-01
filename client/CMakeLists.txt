# 客户端：Qt Widgets 表现层

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(tankcity_client
    main.cpp
    clientconfig.cpp
    clientconfig.h
    loginwindow.cpp
    loginwindow.h
    mainwindow.cpp
    mainwindow.h
    gamewindow.cpp
    gamewindow.h
    scoreboard.cpp
    scoreboard.h
    registerwindow.cpp
    registerwindow.h
    render/painter.cpp
    render/painter.h
    net/MessageFramer.cpp
    net/MessageFramer.h
    loadingwidget.cpp
    loadingwidget.h
)

# client/ 作为头文件根：这样 `#include "net/..."` / `"render/..."` 在子目录里也能用
# （与 tests 目标保持一致；painter.cpp 内部同目录引用 `"painter.h"` 不受影响）
target_include_directories(tankcity_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tankcity_client PRIVATE
    Qt6::Widgets
    Qt6::Network
    tankcity_engine_core     # kTickMs：全仓库唯一的「一帧」定义
    tankcity_engine_render   # 地图绘制原语与贴图缓存（engine/render）
    tankcity_engine_input    # 输入状态容器（engine/input）
    tankcity_shared   # 读 assets/config：数值只允许有一处来源，客户端不另留副本
)

set_target_properties(tankcity_client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
