/**
 * @file  engine_input_test.cpp
 * @brief engine/input 的回归测试。
 *
 * 重点是**搬进测试里**的那条既有行为：主键（射击）是边沿触发的 ——
 * 一次点击只发一次 `shoot = true`，长按不连发。这条原本只写在基线行为清单里
 * （第 8 项，靠人工核对），现在由用例钉住：谁把它改成电平触发，测试立刻失败。
 */

#include <gtest/gtest.h>

#include <QPoint>

#include "input/InputTracker.h"

namespace {

constexpr int kW = 0x57;   // Qt::Key_W
constexpr int kA = 0x41;   // Qt::Key_A
constexpr int kP = 0x50;   // Qt::Key_P（与本游戏语义无关，用来验证「任意键都能被记录」）

} // namespace

TEST(EngineInputInputTracker, KeysAreTrackedAndReleased)
{
    engine::input::InputTracker tracker;
    EXPECT_TRUE(tracker.takeSnapshot().keysDown.isEmpty());

    tracker.keyPressed(kW);
    tracker.keyPressed(kA);
    EXPECT_TRUE(tracker.isKeyDown(kW));
    EXPECT_EQ(tracker.takeSnapshot().keysDown, (QSet<int>{kW, kA}));

    tracker.keyReleased(kW);
    EXPECT_FALSE(tracker.isKeyDown(kW));
    EXPECT_EQ(tracker.takeSnapshot().keysDown, (QSet<int>{kA}));
}

TEST(EngineInputInputTracker, TracksAnyKeyNotJustTheOnesTheGameUses)
{
    // 容器不认识「哪个键算什么」；客户端那张键位表才是解释者。
    engine::input::InputTracker tracker;
    tracker.keyPressed(kP);
    EXPECT_TRUE(tracker.takeSnapshot().keysDown.contains(kP));
}

TEST(EngineInputInputTracker, PointerPositionIsCarriedThrough)
{
    engine::input::InputTracker tracker;
    tracker.setPointer(QPoint(123, 456));
    EXPECT_EQ(tracker.takeSnapshot().pointer, QPoint(123, 456));

    tracker.setPointer(QPoint(7, 8));
    EXPECT_EQ(tracker.takeSnapshot().pointer, QPoint(7, 8));
}

TEST(EngineInputInputTracker, PrimaryPressIsEdgeTriggered)
{
    engine::input::InputTracker tracker;
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed) << "没按过就应当是 false";

    tracker.primaryButtonPressed();

    EXPECT_TRUE(tracker.takeSnapshot().primaryPressed) << "按下后第一次快照应为 true";
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed)
        << "取走即清除 —— 这条守住「长按不连发」的既有行为";
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed);
}

TEST(EngineInputInputTracker, RepeatedPressesProduceRepeatedEdges)
{
    engine::input::InputTracker tracker;

    tracker.primaryButtonPressed();
    EXPECT_TRUE(tracker.takeSnapshot().primaryPressed);

    // 中间夹几次空快照（模拟 16ms 定时器持续发送）
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed);
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed);

    tracker.primaryButtonPressed();   // 第二次点击
    EXPECT_TRUE(tracker.takeSnapshot().primaryPressed) << "每一次点击都要能发出去";
    EXPECT_FALSE(tracker.takeSnapshot().primaryPressed);
}

TEST(EngineInputInputTracker, ResetClearsKeysAndPendingEdgeButKeepsPointer)
{
    engine::input::InputTracker tracker;
    tracker.keyPressed(kW);
    tracker.setPointer(QPoint(11, 22));
    tracker.primaryButtonPressed();

    tracker.reset();   // 等价于搬迁前的 pressedKeys.clear()

    const engine::input::InputTracker::Snapshot snapshot = tracker.takeSnapshot();
    EXPECT_TRUE(snapshot.keysDown.isEmpty());
    EXPECT_FALSE(snapshot.primaryPressed);
    EXPECT_EQ(snapshot.pointer, QPoint(11, 22)) << "旧的 clear 只清按键，不动指针";
}
