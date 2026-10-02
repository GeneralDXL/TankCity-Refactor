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
void setupMaze(MazeFixture &fixture, int levelIndex = 3)
{
    const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, levelIndex, shipped());
    EXPECT_TRUE(fixture.map.loadLevel(level, shipped()));

    const World &world = fixture.map.world();
    const QPoint startCell = firstWalkable(world);
    const QPoint targetCell = farthestCell(world, reachableCells(world, startCell));

    fixture.enemyStart = world.gridToWorld(startCell);
    fixture.playerPos = world.gridToWorld(targetCell);
}

/**
 * 找一个"坦克站得住、但**它所在的格**被判为不可通行"的位置 —— 实测反馈里的"无敌点"。
 *
 * 为什么必须按**位置**扫而不是按格心扫：`isCellWalkable` 用的探针正是"格心处 40x40"
 * （D1 就是这么定义的），所以两者的结论在**格心**必然一致 —— 按格心搜会一个都搜不到。
 * 病灶在偏离格心的位置：坦克站在离墙 20px 处站得住，可它的格心却可能落在墙的探测范围里，
 * 于是"玩家所在格"不可通行，路径终点被吸附到别处。
 *
 * 判据两侧刻意用不同口径：连续侧用 `Map::checkTankCollision`（移动真正用的判定），
 * 网格侧用 `isCellWalkable`。二者不一致的地方，正是敌人找不着玩家的地方。
 */
bool findInvincibilitySpot(Map &map, const QPoint &farFrom, QPoint &out)
{
    const World &world = map.world();
    bool found = false;
    double best = -1.0;

    for (int py = 30; py < Map::MAP_HEIGHT - 30; py += 5) {
        for (int px = 30; px < Map::MAP_WIDTH - 30; px += 5) {
            const QRect probe(px - 20, py - 20, 40, 40);
            if (map.checkTankCollision(probe, nullptr) != -1)
                continue;   // 连续空间站不住，跳过

            const QPoint cell = world.worldToGrid(QPoint(px, py));
            if (world.isCellWalkable(cell.x(), cell.y()))
                continue;   // 网格也说能站，那就不是我们要找的地方

            // 取离敌人最远的那个：位置太近就测不出"走不到"这件事
            const double d = std::hypot(static_cast<double>(px - farFrom.x()),
                                        static_cast<double>(py - farFrom.y()));
            if (d > best) {
                best = d;
                out = QPoint(px, py);
                found = true;
            }
        }
    }
    return found;
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
 * 玩家贴在墙边（所在格被网格判为不可通行）时，敌人也**必须能贴近**。
 *
 * 这是实测反馈的"无敌点"：路径终点是玩家所在格的中心，那一格不可通行时会被吸附到
 * 别处，敌人走到吸附点就以为到位了 —— 站在那儿打墙或不动，直到玩家挪到空地。
 * 修法是「最后一段直冲玩家本人」，用连续空间的判定闭合最后一两格。
 *
 * 用第 1 关（开阔）当夹具：那里空地连通，位置选取不会掺进"死区"这个独立问题。
 */
TEST(EnemyAi, ReachesAPlayerHuggingAWall) {
    MazeFixture fixture;
    setupMaze(fixture, 0);

    const World &world = fixture.map.world();
    const QPoint enemyStart = world.gridToWorld(firstWalkable(world));

    QPoint spot;
    ASSERT_TRUE(findInvincibilitySpot(fixture.map, enemyStart, spot))
        << "这张关卡里没找到\"坦克站得住但网格说不可通行\"的位置";
    ASSERT_GT(distance(enemyStart, spot), 300.0) << "找不出够远的无敌点，测不出问题";

    tankcity::config::TankStats stats =
        tankcity::config::resolveEnemyStats(shipped(), shipped().difficulties.at(0));
    const tankcity::config::AiDef ai;
    Enemy enemy(&fixture.map, enemyStart, 0, stats, ai);

    for (int frame = 0; frame < 1800; ++frame)
        enemy.update(spot, &fixture.map);

    const double finalDistance = distance(enemy.getPosition(), spot);
    EXPECT_LT(finalDistance, 60.0)
        << "敌人没能贴近贴墙站的玩家（最终距离 " << finalDistance
        << "）—— 疑似走到吸附点就以为到位了";
}

/**
 * 未贴墙运动时车体朝向**不得高频左右翻转**（实测反馈的"高频左右震动"）。
 *
 * 判据刻意不是"相邻帧角度差要小"——那会误伤正当的方向变更：换路径点、每 60 帧重寻路
 * 时方向本来就该跳。真正属于"摆动"的特征是**翻转**：这一帧往一侧偏、下一帧往另一侧偏，
 * 来回交替。所以这里统计"最近两次可观转角反号"的次数。
 *
 * 前 10 帧不算：开局那一次"转向对准目标"本来就该是一个大跳变。
 */
TEST(EnemyAi, HullAngleDoesNotAlternateWhileCrossingOpenGround) {
    MazeFixture fixture;
    setupMaze(fixture, 0);   // 开阔关

    const World &world = fixture.map.world();
    const QPoint enemyStart = world.gridToWorld(firstWalkable(world));

    tankcity::config::TankStats stats =
        tankcity::config::resolveEnemyStats(shipped(), shipped().difficulties.at(0));
    const tankcity::config::AiDef ai;
    Enemy enemy(&fixture.map, enemyStart, 0, stats, ai);

    auto wrapTo180 = [](double deg) {
        while (deg > 180.0) deg -= 360.0;
        while (deg < -180.0) deg += 360.0;
        return deg;
    };

    float previous = enemy.getBodyAngle();
    double lastSignificant = 0.0;
    int reversals = 0;
    for (int frame = 0; frame < 300; ++frame) {
        enemy.update(fixture.playerPos, &fixture.map);

        const double delta = wrapTo180(static_cast<double>(enemy.getBodyAngle()) - previous);
        if (frame >= 10 && std::fabs(delta) > 10.0 && std::fabs(lastSignificant) > 10.0
            && (delta > 0.0) != (lastSignificant > 0.0)) {
            ++reversals;
        }
        if (std::fabs(delta) > 10.0)
            lastSignificant = delta;
        previous = enemy.getBodyAngle();
    }

    // 实测：车体角取自"整数位移"时 300 帧里反号 **117** 次（就是玩家看到的高频摆动）；
    // 改成取自"转向决策方向"后降到 **17** 次。这里取 30 作为上限 ——
    // 它既能挡住退回旧行为的回归，也如实承认还有残留（见测试末尾的说明）。
    EXPECT_LT(reversals, 30)
        << "方向在 " << reversals << " 处相邻帧内反号 —— 这就是高频左右摆动";

    // 残留（17 次）尚未解决：怀疑是接近目标时各候选方向打分差距太小（转弯代价只有
    // 0.05px/度，而不同偏转带来的距离差在近距离时也小于 1px），于是来回抢。
    // 要确认得先把"反号发生在哪些帧/什么距离上"打出来，属下一步。
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
