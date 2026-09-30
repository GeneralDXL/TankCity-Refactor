# 客户端：Qt Widgets 表现层

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(tankcity_client
    main.cpp
    clientconfig.cpp
    clientconfig.h
    widget.cpp
    widget.h
    widget.ui
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
    painter.cpp
    painter.h
    loadingwidget.cpp
    loadingwidget.h
)

target_link_libraries(tankcity_client PRIVATE
    Qt6::Widgets
    Qt6::Network
    tankcity_shared   # 读 assets/config：数值只允许有一处来源，客户端不另留副本
)

set_target_properties(tankcity_client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
