/**
 * @file  engine_core_test.cpp
 * @brief engine/core 的回归测试 —— 重点是 GameLoop 真的在跑。
 *
 * 为什么值得单独测：M1.1 把服务端的 `new QTimer(this) + connect(...) + start(16)`
 * 换成 `engine::core::GameLoop` 之后，**循环是否仍然在驱动游戏**只有连上客户端才能
 * 观察到；单元测试能提前把这个风险钉死，不必等到人工试玩。
 *
 * 覆盖三件事：
 *  1. `kTickMs` 这个「帧」的唯一定义没有被随手改动（它是逻辑单位，见 ConfigTypes 的 *Ticks）；
 *  2. `start()` 之后回调会**反复**触发（帧率量级正确）；
 *  3. `stop()` 之后回调**彻底停止**。
 *
 * 注意：QTimer 需要事件循环，所以测试自己维护一个 QCoreApplication 并手动 spin。
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#include "core/GameLoop.h"

namespace {

/// 整个测试进程共用一个 QCoreApplication（QTimer 的前提）；gtest_main 不会建它。
QCoreApplication &ensureApplication()
{
    static int argc = 1;
    static char name[] = "tankcity_tests";
    static char *argv[] = {name, nullptr};
    static QCoreApplication instance(argc, argv);
    return instance;
}

/// 跑事件循环 `ms` 毫秒后返回（期间 QTimer 正常触发）。
void spinEventLoop(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

} // namespace

TEST(EngineCoreGameLoop, TickConstantIsStable)
{
    // 帧数是逻辑单位：改这个值等于同时改所有人的速度、射速与时长。
    EXPECT_EQ(engine::core::kTickMs, 16);
}

TEST(EngineCoreGameLoop, CallbackKeepsFiringWhileRunning)
{
    ensureApplication();

    QObject context;
    int ticks = 0;
    engine::core::GameLoop loop([&ticks] { ++ticks; }, &context);

    EXPECT_FALSE(loop.isRunning());

    loop.start();
    EXPECT_TRUE(loop.isRunning());

    spinEventLoop(100);
    loop.stop();

    EXPECT_FALSE(loop.isRunning());

    // 16ms 一帧：100ms 理论上是 6 帧，放宽到 >=3 以容忍 CI 调度抖动。
    // 若这里只得到 0，基本就是 timer 没启动或 parent 错了（循环失效）。
    EXPECT_GE(ticks, 3) << "循环没有驱动起来";
    // 上界防「间隔没生效」：即便机器再快也不该超过 100 帧/100ms。
    EXPECT_LE(ticks, 100) << "帧间隔没有生效";
}

TEST(EngineCoreGameLoop, StopHaltsTheCallback)
{
    ensureApplication();

    QObject context;
    int ticks = 0;
    engine::core::GameLoop loop([&ticks] { ++ticks; }, &context);

    loop.start();
    spinEventLoop(60);
    loop.stop();

    const int ticksAtStop = ticks;
    ASSERT_GT(ticksAtStop, 0) << "前置条件失败：循环根本没跑";

    spinEventLoop(60);

    // 旧代码在停表之外还 disconnect 了一次；若 stop() 语义不等价，这里会继续涨。
    EXPECT_EQ(ticks, ticksAtStop) << "stop() 之后回调仍在触发";
}

TEST(EngineCoreGameLoop, RestartAfterStopWorks)
{
    ensureApplication();

    QObject context;
    int ticks = 0;
    engine::core::GameLoop loop([&ticks] { ++ticks; }, &context);

    loop.start();
    spinEventLoop(50);
    loop.stop();
    const int firstRound = ticks;

    loop.start();
    spinEventLoop(50);
    loop.stop();

    // 旧写法用 disconnect 收尾，连接会永久消失、再也 start 不起来 —— 这里要求能重启。
    EXPECT_GT(ticks, firstRound) << "stop() 之后无法重新启动";
}
