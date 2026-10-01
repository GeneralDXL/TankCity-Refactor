/**
 * @file  enemy_ai_test.cpp
 * @brief M3 步 4b：敌人在迷宫里的移动行为（"不卡死、不原地抽搐"）。
 *
 * 这一步把敌人的避障从「每帧在 16 个方向里重挑离障碍最远的那个」换成了
 * 「确定顺序的贴墙滑动 + 方向记忆」，并把卡住判据从"动没动"改成"有没有更近"。
 * 前者是启发式，没法用断言穷举；但**结果**可以：给它足够的时间，
 * 它必须真的走到玩家附近 —— 而不是在墙角来回挪。
 *
 * 夹具是真实的第 4 关「砖墙迷宫」（DoD 第 4 项的场景），玩家位置固定在
 * 同一连通分量的最远端。
 */

#include <gtest/gtest.h>

#include <QPoint>
#include <QSet>
#include <QString>
#include <QVector>
#include <cmath>

#include "config/ConfigLoader.h"
#include "config/TankStats.h"
#include "enemy.h"
#include "map.h"

namespace {

using tankcity::config::Config;
using tankcity::config::ConfigLoader;
using tankcity::config::LevelData;

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);
const QString kLevelsDir = QString::fromUtf8(TANKCITY_LEVELS_DIR);

const Config &shipped()
{
    static const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    return cfg;
}

int cellKey(const QPoint &cell)
{
    return cell.x() * 1000 + cell.y();
}

QPoint firstWalkable(const World &world)
{
    for (int y = 0; y < world.getGridHeight(); ++y)
        for (int x = 0; x < world.getGridWidth(); ++x)
            if (world.isCellWalkable(x, y))
                return QPoint(x, y);
    return QPoint(-1, -1);
}

/// 与 pathfinder_test 同一套做法：测试自己算连通分量，避免把夹具的错当成代码的 bug。
QSet<int> reachableCells(const World &world, const QPoint &start)
{
    QSet<int> seen;
    if (!world.isCellWalkable(start.x(), start.y()))
        return seen;

    QVector<QPoint> frontier{start};
    seen.insert(cellKey(start));
    while (!frontier.isEmpty()) {
        const QPoint cur = frontier.takeFirst();
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                const QPoint next(cur.x() + dx, cur.y() + dy);
                if (seen.contains(cellKey(next)))
                    continue;
                if (next.x() < 0 || next.y() < 0 ||
                    next.x() >= world.getGridWidth() || next.y() >= world.getGridHeight())
                    continue;
                if (!world.isCellWalkable(next.x(), next.y()))
                    continue;
                seen.insert(cellKey(next));
                frontier.append(next);
            }
        }
    }
    return seen;
}

QPoint farthestCell(const World &world, const QSet<int> &component)
{
    QPoint far(-1, -1);
    for (int y = 0; y < world.getGridHeight(); ++y)
        for (int x = 0; x < world.getGridWidth(); ++x)
            if (component.contains(cellKey(QPoint(x, y))))
                far = QPoint(x, y);
    return far;
}

double distance(const QPoint &a, const QPoint &b)
{
    return std::hypot(static_cast<double>(a.x() - b.x()), static_cast<double>(a.y() - b.y()));
}

/// 迷宫关（索引 3）里一对"同分量、尽量远"的起终点。
struct MazeFixture
{
    Map map;
    QPoint enemyStart;
    QPoint playerPos;
};

/// 填充式而非返回式：`Map` 是 QObject，不可拷贝也不可移动。
void setupMaze(MazeFixture &fixture)
{
    const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, 3, shipped());
    EXPECT_TRUE(fixture.map.loadLevel(level, shipped()));

    const World &world = fixture.map.world();
    const QPoint startCell = firstWalkable(world);
    const QPoint targetCell = farthestCell(world, reachableCells(world, startCell));

    fixture.enemyStart = world.gridToWorld(startCell);
    fixture.playerPos = world.gridToWorld(targetCell);
}

} // namespace

/**
 * 敌人在砖墙迷宫里必须真的走到玩家附近。
 *
 * 给足 1800 帧（30 秒）。旧实现在墙角会"原地来回挪且永远不算卡住"，
 * 因此永远走不到；这条用例正是那个行为（GeneralDXL 反馈的贴角抽搐）的守门人。
 */
TEST(EnemyAi, ReachesThePlayerAcrossTheMaze) {
    MazeFixture fixture;
    setupMaze(fixture);
    const double initialDistance = distance(fixture.enemyStart, fixture.playerPos);
    ASSERT_GT(initialDistance, 300.0) << "这对待测位置太近，测不出寻路与避障";

    tankcity::config::TankStats stats =
        tankcity::config::resolveEnemyStats(shipped(), shipped().difficulties.at(0));
    const tankcity::config::AiDef ai;

    Enemy enemy(&fixture.map, fixture.enemyStart, 0, stats, ai);

    for (int frame = 0; frame < 1800; ++frame)
        enemy.update(fixture.playerPos, &fixture.map);

    const double finalDistance = distance(enemy.getPosition(), fixture.playerPos);
    EXPECT_LT(finalDistance, 60.0)
        << "敌人在迷宫里没能接近玩家（初始 " << initialDistance
        << "，最终 " << finalDistance << "）—— 疑似卡在墙上反复挪动";
}

/**
 * 朝玩家走的过程里，**净位移应当接近总路程**。
 *
 * 这是"抽搐"的量化判据：来回横跳会让总路程远大于净位移。
 * 阈值放得很宽（4 倍 + 400px 余量），只用来抓住"原地打转"这种量级的退化，
 * 不去约束正常的贴墙绕行。
 */
TEST(EnemyAi, DoesNotWanderInPlace) {
    MazeFixture fixture;
    setupMaze(fixture);

    tankcity::config::TankStats stats =
        tankcity::config::resolveEnemyStats(shipped(), shipped().difficulties.at(0));
    const tankcity::config::AiDef ai;

    Enemy enemy(&fixture.map, fixture.enemyStart, 0, stats, ai);

    QPoint previous = enemy.getPosition();
    double travelled = 0.0;
    for (int frame = 0; frame < 1800; ++frame) {
        enemy.update(fixture.playerPos, &fixture.map);
        const QPoint now = enemy.getPosition();
        travelled += distance(previous, now);
        previous = now;
    }

    const double net = distance(fixture.enemyStart, enemy.getPosition());
    EXPECT_LT(travelled, net * 4.0 + 400.0)
        << "总路程 " << travelled << " 远大于净位移 " << net << " —— 像是在原地来回挪";
}
