# 客户端：Qt Widgets 表现层

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(tankcity_client
    main.cpp
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
)

set_target_properties(tankcity_client PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
