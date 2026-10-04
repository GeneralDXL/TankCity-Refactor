/**
 * @file  terrain_effects_test.cpp
 * @brief M4 步 3：地形词条 —— **视线**（森林挡、海不挡）与森林倍率。
 *
 * 这里守住两件容易被"顺手改坏"的事：
 *
 * 1. **`isLineOfSight` 与 `isLineWalkable` 必须分家**。前者判"看不看得见"（`blocksSight`），
 *    后者判"坦克能否通过"（`blocksTank`），而后者**还被 `PathFinder::simplifyPath`
 *    用来拉直 A\* 路径**。森林上两者结论**相反**，所以这里两条断言并排写：
 *    谁把两个函数合并了，立刻变红。
 *
 * 2. **森林倍率由配置驱动**（0.5 → 0.7）：改 JSON 会改变行为，这是"死配置"的反面守卫。
 *
 * **夹具的取法（踩过坑，写在这里免得后人重踩）**：判定走的是"格"，
 * 而格判据用的是**内缩 5px 的 40×40 探测盒**（`kCellPadding`，它的含义是
 * "格内仍容得下坦克"）。所以：
 *  - 取线要在**物块自己那一格内**（两端都落在同一格里），否则会命中别的物块；
 *  - 拿**整张图**当尺子（例如 `center ± width`）会直接跑到地图外；
 *  - 4 张 tile 试玩图的边界只有 **2px** 厚，**不在**那 40×40 探测盒的范围内 ——
 *    这是既有语义，不是 bug（想验边界请用边界厚 10px 的旧图）。
 */

#include "config/ConfigLoader.h"
#include "config/TankStats.h"
#include "map.h"
#include "player.h"

#include <cmath>

#include <QPoint>
#include <QRect>
#include <QString>

#include <gtest/gtest.h>

namespace {

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);
const QString kLevelsDir = QString::fromUtf8(TANKCITY_LEVELS_DIR);

const tankcity::config::Config &shipped()
{
    static const tankcity::config::Config cfg =
        tankcity::config::ConfigLoader::loadFromDirectory(kConfigDir);
    return cfg;
}

bool loadMap(Map &map, int index)
{
    const tankcity::config::LevelData level =
        tankcity::config::ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());
    return map.loadLevel(level, shipped());
}

/// 在已发布的 14 张关卡里找第一个 @p blockId 物块，把那一关装进 @p map 并返回它的矩形。
QRect findBlock(Map &map, const QString &blockId)
{
    constexpr int kShippedLevels = 15;
    for (int index = 0; index < kShippedLevels; ++index) {
        if (!loadMap(map, index))
            continue;
        for (const Wall &wall : map.getWalls()) {
            if (wall.getBlockId() == blockId)
                return wall.getRect();
        }
    }
    return QRect();
}

/// @p rect 中心所在的那一格（**格坐标**）。
/// 够用的坦克数值（速度取整数 5，位移断言可以写等号）。
tankcity::config::TankStats makePlayerStats()
{
    tankcity::config::TankStats stats;
    stats.health = 7;
    stats.moveSpeed = 5.0;
    stats.shootDelayTicks = 10;
    stats.bulletSpeed = 10.0;
    stats.bulletDamage = 1;
    stats.muzzleOffset = 20;
    return stats;
}

QPoint cellOf(const Map &map, const QRect &rect)
{
    return map.worldToGrid(rect.center());
}

/**
 * 问"这一格看得见吗" —— 取一条**同格内（延伸到隔壁一格）**的短线。
 *
 * ⚠️ **`isLineOfSight` / `isLineWalkable` 收的是「格坐标」，不是世界坐标** ——
 * 名字看不出来（`Enemy::canShoot` 那里是先 `worldToGrid()` 再传的）。
 * 传世界坐标会得到荒谬结果：Bresenham 会把 `(575, 25)` 当成"第 575 格"，
 * 于是问到一个地图外空格，答案永远是"看得见"。这个坑我踩过一次，写在这里。
 */
bool seesWithinItsOwnCell(const Map &map, const QRect &rect)
{
    const QPoint cell = cellOf(map, rect);
    return map.isLineOfSight(cell, cell + QPoint(1, 0));
}

} // namespace

/**
 * 森林：**挡视线，但不挡移动** —— 两个判据结论相反，这正是"森林隐身"的全部机制。
 *
 * 「待在林子里就不被敌怪看见」不需要任何特例：视线判据走 `blocksSight`，
 * 而森林的 `blocksSight` 是 true ⇒ 隐身自然成立 ✓。
 */
TEST(TerrainEffects, ForestBlocksSightButNotMovement) {
    Map map;
    const QRect forest = findBlock(map, QStringLiteral("forest"));
    ASSERT_FALSE(forest.isNull()) << "没有找到含森林的关卡";

    const QPoint cell = map.worldToGrid(forest.center());

    EXPECT_FALSE(seesWithinItsOwnCell(map, forest)) << "森林必须挡住视线（否则隐身失效）";
    EXPECT_TRUE(map.isCellWalkable(cell.x(), cell.y()))
        << "森林**不**挡坦克 —— 若这里是 false，说明视线判据被写进了 isLineWalkable，"
           "那会连带改坏 A* 路径拉直（PathFinder::simplifyPath 用它）";
}

/** 海：**不挡视线，但挡坦克** —— 正是"隔海看见你、但我得绕过去"那条设计。 */
TEST(TerrainEffects, SeaDoesNotBlockSightButBlocksMovement) {
    Map map;
    const QRect sea = findBlock(map, QStringLiteral("sea"));
    ASSERT_FALSE(sea.isNull()) << "没有找到含海洋的关卡";

    const QPoint cell = map.worldToGrid(sea.center());

    EXPECT_TRUE(seesWithinItsOwnCell(map, sea)) << "海的 blocksSight 是 false：隔海应当看得见";
    EXPECT_FALSE(map.isCellWalkable(cell.x(), cell.y())) << "海挡坦克（且它是实心贴图）";
}

/** 冰不挡视线，也不挡移动（只改模型的惯性属 M4 步 4）。 */
TEST(TerrainEffects, IceDoesNotBlockSight) {
    Map map;
    const QRect ice = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(ice.isNull()) << "没有找到含冰块的关卡";

    EXPECT_TRUE(seesWithinItsOwnCell(map, ice));
}

/**
 * 边界挡视线（它挡子弹，也就挡视线）。
 *
 * 用**边界厚 10px 的旧图**（level_01 起的 10 张）—— 4 张 tile 图的边界只有 2px，
 * 落在 `kCellPadding` 的 40×40 探测盒之外，属既有语义（见文件头）。
 */
TEST(TerrainEffects, BoundaryBlocksSight) {
    Map map;
    const QRect boundary = findBlock(map, QStringLiteral("boundary"));
    ASSERT_FALSE(boundary.isNull());
    ASSERT_GT(boundary.height(), 2) << "期望先找到边界厚 10px 的旧图";

    // 从地图内往外穿：线段必经边界那一格（**格坐标** —— 见文件头与 seesWithinItsOwnCell 的说明）
    const QPoint inside = map.worldToGrid(QPoint(Map::MAP_WIDTH / 2, 5));
    const QPoint outside = map.worldToGrid(QPoint(Map::MAP_WIDTH / 2, -50));
    EXPECT_FALSE(map.isLineOfSight(inside, outside));
}

/**
 * 森林倍率 = **0.7**（M4 的目标值；M2 的等价值曾是 0.5）。
 * 同时钉住"配置驱动"：数值来自 blocks.json，`World` 只是读它。
 */
TEST(TerrainEffects, ForestSlowsToSevenTenths) {
    const tankcity::config::BlockDef *forest = shipped().block(QStringLiteral("forest"));
    ASSERT_NE(forest, nullptr);
    EXPECT_NEAR(forest->moveSpeedFactor, 0.7, 1e-9);

    Map map;
    const QRect rect = findBlock(map, QStringLiteral("forest"));
    ASSERT_FALSE(rect.isNull());
    EXPECT_NEAR(map.getMoveSpeedFactor(rect.center()), 0.7, 1e-9)
        << "地形倍率没有从配置走到运行时";
}

/** 冰的倍率仍是 1.5（M4 只改冰的**模型**，不改倍率）。 */
TEST(TerrainEffects, IceKeepsItsSpeedFactorUntilTheInertialStep) {
    Map map;
    const QRect ice = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(ice.isNull());
    EXPECT_NEAR(map.getMoveSpeedFactor(ice.center()), 1.5, 1e-9);
}

/** 冰面的移动模型是**惯性**（配置驱动），且参数已接线。 */
TEST(TerrainEffects, IceUsesTheInertialMovementModel) {
    const tankcity::config::BlockDef *ice = shipped().block(QStringLiteral("ice"));
    ASSERT_NE(ice, nullptr);
    EXPECT_EQ(ice->movement.model, tankcity::config::MovementModel::Inertial);
    EXPECT_GT(ice->movement.frictionPerTick, 0.0);
    EXPECT_LT(ice->movement.frictionPerTick, 1.0) << "摩擦必须小于 1，否则松手就等于急停";
    EXPECT_GT(ice->movement.accelScale, 0.0);
    EXPECT_LT(ice->movement.accelScale, 1.0) << "加速必须弱于 1，否则惯性地形没有'难控'可言";

    Map map;
    const QRect rect = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(rect.isNull());
    EXPECT_EQ(map.getMovementAt(rect.center()).model,
              tankcity::config::MovementModel::Inertial)
        << "地形模型没有从配置走到运行时";
}

/**
 * 冰面：**松手后继续滑行**（惯性的全部意义）。
 *
 * 先在冰格中心朝右推两帧，再**清空输入**走三帧 —— 普通地形上位置应当纹丝不动，
 * 冰上则必须继续向右。位移刻意取得小（合计约 20px < 25px），保证玩家始终留在同一格里，
 * 不会滑出冰面把用例变成"看运气"。
 */
TEST(TerrainEffects, IceKeepsSlidingAfterTheInputStops) {
    Map map;
    const QRect ice = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(ice.isNull()) << "没有找到含冰块的关卡";

    const QPoint start = map.gridToWorld(map.worldToGrid(ice.center()));
    Player player(&map, makePlayerStats());
    player.init(start.x(), start.y());

    player.setMoveVector(QPointF(1, 0));
    for (int i = 0; i < 2; ++i)
        player.update();
    const QPoint afterPush = player.getPosition();

    player.setMoveVector(QPointF(0, 0));
    for (int i = 0; i < 3; ++i)
        player.update();

    EXPECT_GT(player.getPosition().x(), afterPush.x())
        << "冰上松手应当继续滑行；若这里没动，说明摩擦/惯性没有接线"
           "（位移量刻意小：加速度很弱，见 IceDirectionalResponseIsDeliberatelySluggish）";
}

/**
 * 冰面的**方向响应**必须迟钝 —— 这条钉的就是 2026-10-03 试玩反馈 #1。
 *
 * 反馈原话：*"我向前有一个速度、想往后撤，这个加速度会小很多，我仍然会向前滑行一段距离
 * 才会向后开始加速；正交方向同理 —— 向前的速度按惯性衰减、向右则慢慢加速到设定移速，
 * 而不是立刻得到很高的速度。"*
 *
 * 模型可测：`v ← v·friction + target·accel`（当 `accel = 1 − friction` 时与"朝目标按比例靠拢"等价）。
 * 于是**一帧输入只把速度朝目标推进 accel**，而**旧方向的分量每帧只衰减 accel**。
 * M4 步 4 的初值 accel = 0.3 太快（3 帧就冲到目标的 66%）—— 表现为"冰也不太难控制"；
 * 现在 accel = 0.05（= 1 − friction）：
 *  · 按住 3 帧 → 只到目标的 ~14%（"慢慢加速"✓）
 *  · 换向 1 帧 → 旧分量仍保留 ~95%（"先滑一段"✓）
 */
/**
 * 森林隐身必须**连贴身那一格**也守住 —— 2026-10-04 走查实测出的缺陷。
 *
 * 观察原话：*"进入林子后，炮口不指向我，但是仍然会朝我这里运动，且在林子内部他们会和我
 * 贴脸然后攻击，然后我会扣血，这个过程看不到子弹。"*
 *
 * 根因不在森林判据，而在引擎的一个**退化情形**：`Grid::walkLine` 在**起终点同格**时
 * 一次都不回调、直接返回 `true`（既有契约）。敌人贴到玩家身上 ⇒ 两者同格 ⇒ 视线判定
 * "看得见" ✗ ⇒ 开火，而子弹生在玩家身上（`muzzleOffset` 只有 25px，贴身时仍在身上）⇒
 * **掉血看不到子弹** ✓。修法是同格时改问"这一格本身挡不挡视线"。
 *
 * 第三条断言是**反面**：别把这条修成"同格永远看不见" —— 海不挡视线，隔海互射是既有口径 ✓。
 */
TEST(TerrainEffects, ForestHidesTheTankEvenAtPointBlankRange) {
    Map map;

    const QRect forestRect = findBlock(map, QStringLiteral("forest"));
    ASSERT_FALSE(forestRect.isNull());
    const QPoint forestCell = map.worldToGrid(forestRect.center());

    // ① 同一格（贴身）：仍应"看不见"
    EXPECT_FALSE(map.isLineOfSight(forestCell, forestCell))
        << "起终点同格时视线退化成 true —— 贴身的敌人因此能开火（走查实测的掉血来源）";

    // ② 相邻格、目标在森林里：看不见
    EXPECT_FALSE(map.isLineOfSight(forestCell + QPoint(1, 0), forestCell));

    // ③ 海：同格仍然"看得见"（不挡视线，隔海能互射）
    const QRect seaRect = findBlock(map, QStringLiteral("sea"));
    ASSERT_FALSE(seaRect.isNull());
    const QPoint seaCell = map.worldToGrid(seaRect.center());
    EXPECT_TRUE(map.isLineOfSight(seaCell, seaCell))
        << "把同格一律判成看不见会把海也遮掉 —— 海只挡车、不挡视线";
}

TEST(TerrainEffects, IceDirectionalResponseIsDeliberatelySluggish) {
    const tankcity::config::BlockDef *ice = shipped().block(QStringLiteral("ice"));
    ASSERT_NE(ice, nullptr);
    const double friction = ice->movement.frictionPerTick;
    const double accel = ice->movement.accelScale;

    // 两条参数必须**配对**：accel = 1 − friction 时，"输入加速"与"松手滑行"是同一个速率。
    // 否则转向会比滑行灵敏得多 —— 反馈 #1 的手感问题正是这么来的。
    EXPECT_NEAR(accel, 1.0 - friction, 0.02)
        << "accelScale 与 frictionPerTick 不配对：转向会比滑行灵敏得多";

    Map map;
    const QRect iceRect = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(iceRect.isNull());
    const QPoint start = map.gridToWorld(map.worldToGrid(iceRect.center()));

    const tankcity::config::TankStats stats = makePlayerStats();
    Player player(&map, stats);
    player.init(start.x(), start.y());

    // 目标速度 = 移速 × 冰面倍率
    const double target = stats.moveSpeed * map.getMoveSpeedFactor(start);
    ASSERT_GT(target, 0.0);

    // ① 按住一个方向 3 帧：只应达到目标的一小部分
    player.setMoveVector(QPointF(1, 0));
    for (int i = 0; i < 3; ++i)
        player.update();

    const double expectedAfterThree = target * (1.0 - std::pow(friction, 3));
    EXPECT_NEAR(player.velocity().x(), expectedAfterThree, 0.15)
        << "3 帧的加速量与 friction/accel 不吻合 —— 方向响应太灵敏或太迟钝（反馈 #1）";
    EXPECT_LT(player.velocity().x(), target * 0.25)
        << "按住 3 帧就到了目标速度的 1/4 以上：「冰面难控制」的手感会消失";

    // ② 换向一帧：旧方向的分量只衰减一个 accel 的份额，且**方向还没反过来** ——
    //    这正是反馈 #1 说的「先滑一段、还没开始向后加速」。
    //    公式：v' = v·(1−accel) + (−target)·accel
    const double beforeTurn = player.velocity().x();
    player.setMoveVector(QPointF(-1, 0));
    player.update();

    EXPECT_NEAR(player.velocity().x(), beforeTurn * (1.0 - accel) - target * accel, 0.15)
        << "换向一帧后的速度与 friction/accel 不吻合";
    EXPECT_GT(player.velocity().x(), 0.0)
        << "刚按下反方向就立刻倒车：应当「先滑行一段才反向加速」（反馈 #1）";
}

/** 对照组：**普通地形上松手立刻停** —— 惯性不许泄漏到别的地形上。 */
TEST(TerrainEffects, NormalTerrainStillStopsImmediately) {
    Map map;   // 空地图：没有任何地形 → 普通模型
    Player player(&map, makePlayerStats());
    player.init(600, 450);

    player.setMoveVector(QPointF(1, 0));
    player.update();
    const QPoint afterPush = player.getPosition();

    player.setMoveVector(QPointF(0, 0));
    for (int i = 0; i < 10; ++i)
        player.update();

    EXPECT_EQ(player.getPosition(), afterPush)
        << "普通地形上松手必须立即停住（惯性状态不得泄漏到非冰地形）";
}
