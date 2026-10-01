# 服务端：权威游戏逻辑

# 只依赖 Core / Network：服务端不绘制，Widgets（乃至 Gui）都不该出现在这里
find_package(Qt6 REQUIRED COMPONENTS Core Network)

qt_add_executable(tankcity_server
    main.cpp
    game.cpp
    game.h
    server.cpp
    server.h
)

target_link_libraries(tankcity_server PRIVATE
    Qt6::Core
    Qt6::Network
    tankcity_engine_core   # 固定步长逻辑循环（引擎层）
    tankcity_game     # 实体与世界（M1.1 从 server/ 提升为独立层）
    tankcity_shared   # 关卡/配置模型：assets/levels/*.json 的加载器（由 game 传递依赖）
)

# Windows 特定定义（Winsock 2 / 避免 min/max 宏冲突）
if(WIN32)
    target_compile_definitions(tankcity_server PRIVATE _WIN32_WINNT=0x0601 NOMINMAX)
endif()

# 控制台程序（M1.1）：服务端不能是隐藏窗口的 GUI 程序 ——
# 配置校验失败时的「文件 + 字段路径 + 原因」正是靠 qCritical() 报出来的，
# 没有终端等于没报。客户端仍保持 GUI 子系统不变。
set_target_properties(tankcity_server PROPERTIES
    WIN32_EXECUTABLE FALSE
)
