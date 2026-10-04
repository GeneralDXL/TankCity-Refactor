/**
 * @file  bullet_damage_test.cpp
 * @brief M4 步 1：物块「挑弹种」的规则 —— 钢材要穿甲弹，砖块不挑。
 *
 * 这是 `requiredBulletTags` 的**第一条运行期守卫**。在此之前该字段被加载器解析、
 * 也被加载期校验使用，但没有任何运行期代码读它 —— 即**改 JSON 不生效**
 * （与 M3 修掉的 `collisionBox`、`difficulty.ai` 是同一类坑）。
 *
 * 本文件同时守住 M2 定下的**数值律**：同级武器破墙恒定 4 下。
 */

#include "config/ConfigLoader.h"
#include "map.h"

#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

namespace {

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);
const QString kLevelsDir = QString::fromUtf8(TANKCITY_LEVELS_DIR);

/// blocks/layers/entities 都只加载一次（ConfigLoader 是纯函数）。
const tankcity::config::Config &shipped()
{
    static const tankcity::config::Config cfg =
        tankcity::config::ConfigLoader::loadFromDirectory(kConfigDir);
    return cfg;
}

/// 把第 index 张关卡装进 Map —— 子弹命中物块的**规则**都在 Map 上。
bool loadMap(Map &map, int index)
{
    const tankcity::config::LevelData level =
        tankcity::config::ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());
    return map.loadLevel(level, shipped());
}

/// 在**已发布的关卡里**找一个"干净"的 @p blockId 物块，返回它的探测矩形（内缩 1px）。
///
/// 「干净」的定义不靠推断，而是**直接问世界**：给 `firstShotBlockerId` 一个探测盒，
/// 它回答"首个命中是谁"；只有答案**正好是这个物块**时才算候选。
///
/// 为什么必须这么筛：`checkBulletCollision` 是"**首个命中即返回**"，而 rects 关卡的矩形是
/// **任意矩形、可以重叠** —— 若目标物块之前还有一面挡弹的墙与探测盒相交，伤害就会记到
/// 那面墙上，用例会盯着一个从没挨过打的对象（这条用例实测踩过两次：先是砖块互相重叠，
/// 再是把铺满全图的 ground 矩形误当成遮挡者）。
///
/// 覆盖全部 14 张已发布关卡（level_01..10 旧图 + level_11..14 tile 试玩图），
/// 任何一张里有干净的候选即可。
QRect findBlock(Map &map, const QString &blockId, int *wallIdOut)
{
    constexpr int kShippedLevels = 15;   // M3 收尾时的关卡数（含 4 张 tile 试玩图）

    for (int index = 0; index < kShippedLevels; ++index) {
        if (!loadMap(map, index))
            continue;

        for (const Wall &wall : map.getWalls()) {
            if (wall.getBlockId() != blockId)
                continue;

            // 内缩 1px：保证探测盒与物块**真正相交**，而不是贴着边界擦过
            const QRect r = wall.getRect();
            const QRect probe(r.x() + 1, r.y() + 1, qMax(1, r.width() - 2), qMax(1, r.height() - 2));

            // 问世界：这个探测盒的"首个命中"是不是它自己
            if (map.world().firstShotBlockerId(probe) != wall.getId())
                continue;

            if (wallIdOut != nullptr)
                *wallIdOut = wall.getId();
            return probe;
        }
    }
    return QRect();
}

bool wallStillExists(const Map &map, int wallId)
{
    for (const Wall &wall : map.getWalls()) {
        if (wall.getId() == wallId)
            return true;
    }
    return false;
}

/// 普通弹：伤害 1、无标签、不弹。
tankcity::config::BulletProfile plainBullet()
{
    tankcity::config::BulletProfile profile;
    profile.damage = 1;
    return profile;
}

/// 穿甲弹：伤害 2、带 `armorPiercing`（`entities.json` 的 `bulletAP`，M4 步 1）。
tankcity::config::BulletProfile armourPiercingBullet()
{
    tankcity::config::BulletProfile profile;
    profile.damage = 2;
    profile.tags = QStringList{QStringLiteral("armorPiercing")};
    return profile;
}

} // namespace

/**
 * 钢材「挑弹种」：**普通弹打不掉**。
 *
 * 旧代码里钢材耐久 200000，靠数字大假装不可破；M4 起它真的可破坏，
 * 但要求子弹带 `armorPiercing` 标签。这条断言把"打不动"钉死：
 * 即便连打 20 发，钢材仍在（血量一点没掉）。
 */
TEST(BulletDamage, SteelIgnoresBulletsWithoutTheRequiredTag) {
    Map map;
    int steelId = -1;
    const QRect probe = findBlock(map, QStringLiteral("steel"), &steelId);
    ASSERT_FALSE(probe.isNull()) << "没有找到含钢材的关卡，这条用例失去意义";

    // 普通弹：伤害 1、无标签
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(map.checkBulletCollision(probe, plainBullet()).hit)
            << "打不动也要算命中 —— 子弹照样消失（手感上就是'打在钢板上'）";
    }

    EXPECT_TRUE(wallStillExists(map, steelId)) << "普通弹居然把钢材打掉了";
}

/**
 * 穿甲弹破钢：**8 血 ÷ 2 伤 = 4 下**（M2 的数值律：同级武器恒定 4 下）。
 *
 * 顺便钉住"第 4 下才消失"：前 3 下钢材都还在。
 */
TEST(BulletDamage, ArmourPiercingBreaksSteelInFourHits) {
    Map map;
    int steelId = -1;
    const QRect probe = findBlock(map, QStringLiteral("steel"), &steelId);
    ASSERT_FALSE(probe.isNull());

    const tankcity::config::BulletProfile ap = armourPiercingBullet();

    for (int hit = 1; hit <= 3; ++hit) {
        EXPECT_TRUE(map.checkBulletCollision(probe, ap).hit);
        EXPECT_TRUE(wallStillExists(map, steelId)) << "第 " << hit << " 下就把钢材打掉了（早于 4 下）";
    }

    EXPECT_TRUE(map.checkBulletCollision(probe, ap).hit);
    EXPECT_FALSE(wallStillExists(map, steelId)) << "第 4 下没打掉钢材（数值律失效）";
}

/** 砖块不挑弹种：普通弹照样**4 下**破（既有数值律的对照）。 */
TEST(BulletDamage, BrickStillBreaksInFourHitsWithPlainBullets) {
    Map map;
    int brickId = -1;
    const QRect probe = findBlock(map, QStringLiteral("brick"), &brickId);
    ASSERT_FALSE(probe.isNull());

    for (int hit = 1; hit <= 3; ++hit) {
        EXPECT_TRUE(map.checkBulletCollision(probe, plainBullet()).hit);
        EXPECT_TRUE(wallStillExists(map, brickId)) << "第 " << hit << " 下就破了砖块";
    }
    EXPECT_TRUE(map.checkBulletCollision(probe, plainBullet()).hit);
    EXPECT_FALSE(wallStillExists(map, brickId)) << "第 4 下没打掉砖块";
}

/**
 * 配置与行为的**纽带**：改 JSON 必须改变行为。
 *
 * 这条断言把"钢要穿甲、且真有子弹提供该标签"两件事一起钉住 ——
 * 加载器有一条校验「requiredBulletTags 里的标签必须有子弹提供」，
 * 所以这两项必须同时存在，缺一个连加载都过不去。
 */
TEST(BulletDamage, ShippedConfigWiresArmourPiercing) {
    const tankcity::config::BlockDef *steel = shipped().block(QStringLiteral("steel"));
    ASSERT_NE(steel, nullptr);
    EXPECT_TRUE(steel->destructible);
    EXPECT_EQ(steel->maxHealth, 8);
    EXPECT_EQ(steel->requiredBulletTags, QStringList{QStringLiteral("armorPiercing")});

    const tankcity::config::BulletDef *ap = shipped().bullet(QStringLiteral("bulletAP"));
    ASSERT_NE(ap, nullptr) << "没有 bulletAP，钢材的 requiredBulletTags 就无人满足";
    EXPECT_TRUE(ap->tags.contains(QStringLiteral("armorPiercing")));
    EXPECT_EQ(ap->damage, 2);

    // 数值律：8 ÷ 2 = 4
    EXPECT_EQ(steel->maxHealth / ap->damage, 4);
}
