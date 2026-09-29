# 服务端：权威游戏逻辑

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

qt_add_executable(tankcity_server
    main.cpp
    game.cpp
    game.h
    player.cpp
    player.h
    enemy.cpp
    enemy.h
    bullet.cpp
    bullet.h
    tank.cpp
    tank.h
    map.cpp
    map.h
    wall.cpp
    wall.h
    server.cpp
    server.h
)

target_link_libraries(tankcity_server PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)

# Windows 特定定义（Winsock 2 / 避免 min/max 宏冲突）
if(WIN32)
    target_compile_definitions(tankcity_server PRIVATE _WIN32_WINNT=0x0601 NOMINMAX)
endif()

set_target_properties(tankcity_server PROPERTIES
    WIN32_EXECUTABLE TRUE
)
