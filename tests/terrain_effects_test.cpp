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
#include "map.h"

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
    constexpr int kShippedLevels = 14;
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

/** 冰的倍率仍是 1.5（M4 只改冰的**模型**，不改倍率 —— 惯性属步 4）。 */
TEST(TerrainEffects, IceKeepsItsSpeedFactorUntilTheInertialStep) {
    Map map;
    const QRect ice = findBlock(map, QStringLiteral("ice"));
    ASSERT_FALSE(ice.isNull());
    EXPECT_NEAR(map.getMoveSpeedFactor(ice.center()), 1.5, 1e-9);
}
