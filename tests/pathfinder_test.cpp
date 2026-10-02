/**
 * @file  pathfinder_test.cpp
 * @brief M3 步 4：A\* 寻路（`game/world/pathfinder.h`）的单元测试。
 *
 * 夹具用**仓库里真实的 10 张关卡**（不是人造网格）：第 4 关「砖墙迷宫」正是
 * DoD 第 4 项的验收场景，拿它跑比造格子更有说服力，也顺手把「A\* 搬到 world 层后
 * 边界常量仍取自地图」这件事钉住（旧实现把 30 / 22.5 抄进了死代码）。
 *
 * ## 两条踩过的坑（写测试时实测得来，别再踩）
 *  1. **路径是简化过的**，相邻两格**不保证**八邻接 —— 平滑本来就是把中间点拉直去掉。
 *     所以这里只断言「每格可通行 + 端点是起点之后/目标本身」，不断言逐格相邻。
 *  2. **第 4 关迷宫的可通行格不是全连通的**：24×18 网格里有 258 格可通行，
 *     其中 239 格与其余 19 格互不相通（`PathFinder.FindsAPathBetweenBothEndsOfEveryLevel`
 *     早期版本取「行优先第一个/最后一个可通行格」当端点，正好落在两个分量里，
 *     于是第 4 关必然求不出路径）。因此端点改为**同一连通分量内**选取。
 *     这个死区的成因与影响见 `docs/plans/M3-世界与玩法重构草案.md` §7。
 */

#include <gtest/gtest.h>

#include <QPoint>
#include <QSet>
#include <QString>
#include <QVector>

#include "config/ConfigLoader.h"
#include "pathfinder.h"
#include "world.h"

namespace {

using tankcity::config::Config;
using tankcity::config::ConfigLoader;
using tankcity::config::LevelData;

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);
const QString kLevelsDir = QString::fromUtf8(TANKCITY_LEVELS_DIR);

/// 关卡数与 blocks/layers 定义都不变，加载一次即可（ConfigLoader 是纯函数）。
const Config &shipped()
{
    static const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    return cfg;
}

/// 第 @p index 张关卡（0 起）装进一个 World。
World loadWorld(int index)
{
    World world;
    const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());
    EXPECT_TRUE(world.loadLevel(level, shipped())) << "第 " << index << " 张关卡装载失败";
    return world;
}

int cellKey(const QPoint &cell)
{
    return cell.x() * 1000 + cell.y();
}

/// 按行优先找出第一个可通行的格。
QPoint firstWalkable(const World &world)
{
    for (int y = 0; y < world.getGridHeight(); ++y)
        for (int x = 0; x < world.getGridWidth(); ++x)
            if (world.isCellWalkable(x, y))
                return QPoint(x, y);
    return QPoint(-1, -1);
}

/**
 * 从 @p start 出发、按八邻接能走到的所有格（**测试自己算的**，不依赖被测代码）。
 *
 * 用来把「A\* 没找到路」与「这两格本来就不通」区分开 —— 这是本文件第一个
 * 假失败的教训：不先分清这两件事，就会把夹具的错当成代码的 bug。
 */
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

/// 分量内按行优先最靠后的那一格 —— 用它当远端，保证"确实能走到"。
QPoint farthestCell(const World &world, const QSet<int> &component)
{
    QPoint far(-1, -1);
    for (int y = 0; y < world.getGridHeight(); ++y)
        for (int x = 0; x < world.getGridWidth(); ++x)
            if (component.contains(cellKey(QPoint(x, y))))
                far = QPoint(x, y);
    return far;
}

/// 一条路径的通用不变量：非空、不含起点、止于目标、每格可通行。
void expectWellFormedPath(const World &world,
                          const QVector<QPoint> &path,
                          const QPoint &start,
                          const QPoint &expectedEnd)
{
    ASSERT_FALSE(path.isEmpty()) << "没有找到路径";
    EXPECT_NE(path.first(), start) << "路径不应包含起点（调用方已经站在那一格）";
    EXPECT_EQ(path.last(), expectedEnd) << "路径终点不是目标格";

    // 注意：**不**断言相邻两格八邻接 —— 路径是简化过的（见文件头说明）
    for (const QPoint &cell : path)
        EXPECT_TRUE(world.isCellWalkable(cell.x(), cell.y()))
            << "路径经过了不可通行的格 (" << cell.x() << ", " << cell.y() << ")";
}

} // namespace

/**
 * 出生点必须放得下整辆车 —— 钉住 level_13「出生在海里、开局动不了」那个 bug。
 *
 * 背景：出生点此前是 `addPlayer` 里写死的 `80,400`，与关卡内容无关；`level_13` 的海
 * 正好覆盖那一格（x=80 → 第 1 列，y=400 → 第 8 行），玩家一出生就被困住。
 *
 * 两侧口径刻意不同：`nearestWalkableCenter` 只保证"这一格按**网格**算可通行"，
 * 这里再用**连续空间**的 `blocksTankAt` 把整辆车放上去复核一遍。两者都过才算真能出生 ——
 * 只用其中一个，都可能漏掉 D1 那种"格与车不一致"的边角。
 */
TEST(PathFinder, SpawnPointFitsTheTankOnEveryLevel) {
    const QVector<tankcity::config::LevelEntry> levels = ConfigLoader::listLevels(kLevelsDir);
    ASSERT_GE(levels.size(), 10);

    for (int index = 0; index < levels.size(); ++index) {
        SCOPED_TRACE(testing::Message() << "关卡序号 " << index);

        const World world = loadWorld(index);

        // 用玩家出生点的"期望值"发问（与 game.cpp 的 addPlayer 同一个入参）
        const QPoint spawn = world.nearestWalkableCenter(QPoint(80, 400));

        const QPoint cell = world.worldToGrid(spawn);
        EXPECT_TRUE(world.isCellWalkable(cell.x(), cell.y()))
            << "出生点所在的格不可通行（level_13 的海就是这么卡住玩家的）";

        const QRect probe(spawn.x() - 20, spawn.y() - 20, 40, 40);
        EXPECT_FALSE(world.blocksTankAt(probe))
            << "出生点放不下整辆车，玩家一开局就会被卡住";

        // 现场取证：level_13 里原先那个写死的出生点（80,400）**确实**是不可通行的 ——
        // 这条断言保证上面那两条不是"碰巧通过"：真正的病灶就在这里。
        // 若哪天有人把海改走、使这里变得可通行，一条会失败，提醒他确认修复是否仍被覆盖。
        if (levels.at(index).id == QStringLiteral("level_13")) {
            EXPECT_TRUE(world.blocksTankAt(QRect(80 - 20, 400 - 20, 40, 40)))
                << "level_13 的 80,400 本应是海（这个 bug 的现场）——"
                   "地图被改过？请确认出生点修复仍被这条用例覆盖";
        }
    }
}

/**
 * 全部 10 张关卡：各自连通分量内最远的两格之间都能求出**格式正确**的路径。
 *
 * 覆盖面最大的守卫 —— 同时检查了跨整张图的寻路、关卡装载，以及
 * 「每格可通行 + 端点正确」这两条不变量。
 */
TEST(PathFinder, FindsAPathBetweenBothEndsOfEveryLevel) {
    for (int index = 0; index < 10; ++index) {
        const World world = loadWorld(index);
        const QPoint start = firstWalkable(world);
        ASSERT_NE(start, QPoint(-1, -1)) << "第 " << index << " 张关卡没有可通行格";

        const QSet<int> component = reachableCells(world, start);
        ASSERT_GE(component.size(), 2) << "第 " << index << " 张关卡的可通行格不足两格";

        const QPoint target = farthestCell(world, component);
        const QVector<QPoint> path = PathFinder::findPath(world, start, target);

        expectWellFormedPath(world, path, start, target);
    }
}

/**
 * 砖墙迷宫（第 4 关，0 起的索引 3）：必须**绕过** 31 个阻挡物，而不是直线穿过去。
 *
 * DoD 第 4 项的自动化部分 —— 它证明「敌人能绕墙抵达玩家」里的**寻路**这一半成立。
 * 另一半（不卡死、不抽搐）是移动层的责任，只能试玩确认。
 */
TEST(PathFinder, RoutesAroundTheBrickMaze) {
    const World world = loadWorld(3);
    const QPoint start = firstWalkable(world);
    const QPoint target = farthestCell(world, reachableCells(world, start));

    // 先确认这不是一条"两点之间本来就没墙"的平凡路径
    EXPECT_FALSE(world.isLineWalkable(start, target))
        << "迷宫关的这两个端点之间居然能直着走，这条用例就失去意义了";

    const QVector<QPoint> path = PathFinder::findPath(world, start, target);

    expectWellFormedPath(world, path, start, target);
    EXPECT_GT(path.size(), 1) << "迷宫关的路径不该只有一格";
}

/**
 * 起点与终点同格：返回只含该格的一条路径。
 *
 * 这是旧实现的行为（`startGrid == targetGrid` 时 `path = {targetGrid}`），
 * 调用方据此知道"朝目标格中心走就行"，故照搬。
 */
TEST(PathFinder, SameCellYieldsThatSingleCell) {
    const World world = loadWorld(0);
    const QPoint cell = firstWalkable(world);

    const QVector<QPoint> path = PathFinder::findPath(world, cell, cell);

    ASSERT_EQ(path.size(), 1);
    EXPECT_EQ(path.first(), cell);
}

/**
 * 落在墙里的坐标会被吸附到可通行的格。
 *
 * 敌人被挤进墙角、玩家贴着墙时，起点/终点就可能落在不可通行格上；
 * 直接寻路必然失败，所以两个端点都要先吸附。
 */
TEST(PathFinder, SnapsAnUnwalkableCellToAWalkableOne) {
    const World world = loadWorld(3);   // 迷宫关：墙多，容易取到一个墙格

    // 取第一个"中心落在不可通行格上"的物块
    QPoint insideWall(-1, -1);
    for (const Wall &wall : world.getWalls()) {
        const QPoint cell = world.worldToGrid(wall.getRect().center());
        if (!world.isCellWalkable(cell.x(), cell.y())) {
            insideWall = cell;
            break;
        }
    }
    ASSERT_NE(insideWall, QPoint(-1, -1)) << "这张关卡里没找到一个完全落在墙内的格";

    const QPoint snapped = PathFinder::snapToWalkable(world, insideWall);

    EXPECT_TRUE(world.isCellWalkable(snapped.x(), snapped.y()))
        << "吸附结果仍不可通行";
    // 旧实现的半径是 5，所以吸附点不应跑到更远的地方
    EXPECT_LE(qAbs(snapped.x() - insideWall.x()), 5);
    EXPECT_LE(qAbs(snapped.y() - insideWall.y()), 5);
}

/**
 * 已经可通行的格不被吸附（"没坏就别动"的边界）。
 */
TEST(PathFinder, KeepsAWalkableCellUnchanged) {
    const World world = loadWorld(0);
    const QPoint cell = firstWalkable(world);

    EXPECT_EQ(PathFinder::snapToWalkable(world, cell), cell);
}
