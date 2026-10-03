/**
 * @file  block_drop_test.cpp
 * @brief M4 步 5：**① 地形即资源** —— 砖块被打掉时按掉落表掉道具。
 *
 * 分三层测，因为这条链有三段：
 *  1. **抽签**：`pickDrop` 是纯函数 —— 给定 roll 必得确定结果，权重算错立刻现形；
 *  2. **配置**：brick 的掉落表声明齐全，且引用的 id 都能被运行期认出来（见下）；
 *  3. **接线**：物块倒下时广播的 `delete_wall` **带着"是什么物块、倒在哪里"** ——
 *     服务端正是靠这两个字段决定"掉什么、掉在哪儿"。少了它，整条链静默断掉。
 */

#include "config/ConfigLoader.h"
#include "config/DropTable.h"
#include "map.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

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

/// 找一个"干净"的物块（首个命中确实是它自己）—— 与另两个测试同法：直接问世界。
QRect findBlock(Map &map, const QString &blockId, int *wallIdOut)
{
    constexpr int kShippedLevels = 15;
    for (int index = 0; index < kShippedLevels; ++index) {
        if (!loadMap(map, index))
            continue;
        for (const Wall &wall : map.getWalls()) {
            if (wall.getBlockId() != blockId)
                continue;
            const QRect r = wall.getRect();
            const QRect probe(r.x() + 1, r.y() + 1, qMax(1, r.width() - 2), qMax(1, r.height() - 2));
            if (map.world().firstShotBlockerId(probe) != wall.getId())
                continue;
            if (wallIdOut != nullptr)
                *wallIdOut = wall.getId();
            return probe;
        }
    }
    return QRect();
}

tankcity::config::BulletProfile plainBullet()
{
    tankcity::config::BulletProfile profile;
    profile.damage = 1;
    return profile;
}

} // namespace

/** 抽签：权重决定区间，且给定 roll 的结果是确定的。 */
TEST(BlockDrop, PickDropFollowsTheWeights) {
    const QVector<tankcity::config::DropDef> drops{
        {QStringLiteral("a"), 3}, {QStringLiteral("b"), 2}, {QStringLiteral("c"), 2}};   // 合计 7

    EXPECT_EQ(tankcity::config::pickDrop(drops, 0.0)->item, QStringLiteral("a"));
    EXPECT_EQ(tankcity::config::pickDrop(drops, 0.4)->item, QStringLiteral("a"));   // 2.8 → a
    EXPECT_EQ(tankcity::config::pickDrop(drops, 0.5)->item, QStringLiteral("b"));   // 3.5 → b
    EXPECT_EQ(tankcity::config::pickDrop(drops, 0.99)->item, QStringLiteral("c"));  // 6.93 → c

    // 权重为 0 的项永远抽不到
    const QVector<tankcity::config::DropDef> withZero{
        {QStringLiteral("a"), 1}, {QStringLiteral("b"), 0}};
    for (double roll = 0.0; roll < 1.0; roll += 0.1)
        EXPECT_EQ(tankcity::config::pickDrop(withZero, roll)->item, QStringLiteral("a"));

    // 空表 / 全零权重 → 不抽
    EXPECT_EQ(tankcity::config::pickDrop({}, 0.5), nullptr);
    EXPECT_EQ(tankcity::config::pickDrop({{QStringLiteral("a"), 0}}, 0.5), nullptr);
}

/** 配置：砖块确实声明了掉落表（①的"资源"来源），且权重为正。 */
TEST(BlockDrop, BrickDeclaresItsDrops) {
    const tankcity::config::BlockDef *brick = shipped().block(QStringLiteral("brick"));
    ASSERT_NE(brick, nullptr);
    ASSERT_FALSE(brick->drops.isEmpty()) << "砖块不掉东西，① 地形即资源就无从谈起";

    for (const tankcity::config::DropDef &drop : brick->drops) {
        EXPECT_GT(drop.weight, 0);
        bool inItems = false;
        for (const tankcity::config::ItemDef &item : shipped().items)
            inItems = inItems || item.id == drop.item;
        EXPECT_TRUE(inItems) << "掉落表引用了 items.json 里不存在的道具："
                             << drop.item.toStdString();
    }

    // 钢材不掉（它不吃普通弹，掉东西会让人误以为"打钢有收益"）
    const tankcity::config::BlockDef *steel = shipped().block(QStringLiteral("steel"));
    ASSERT_NE(steel, nullptr);
    EXPECT_TRUE(steel->drops.isEmpty());
}

/**
 * 运行期的 id → `ItemType` 桥**不能漏项**。
 *
 * `Game::dropItemForBlock` 里那张表是临时桥（M4.5 用「效果数组」替掉它）。
 * 这里把"配置里出现的 id"与"运行期认识的 id"对齐 —— 谁在 blocks.json 里
 * 加了一个新道具名而忘了接桥，这条会红，而不是在战场上静默地什么都不掉。
 */
TEST(BlockDrop, EveryShippedDropIdIsKnownToTheRuntime) {
    const QStringList known{QStringLiteral("healthPack"), QStringLiteral("ammoBoost"),
                            QStringLiteral("speedBoost")};

    for (const tankcity::config::BlockDef &block : shipped().blocks) {
        for (const tankcity::config::DropDef &drop : block.drops) {
            EXPECT_TRUE(known.contains(drop.item))
                << "物块 " << block.id.toStdString() << " 的掉落 " << drop.item.toStdString()
                << " 没有对应的 ItemType —— 请同步 Game::dropItemForBlock 里的桥";
        }
    }
}

/**
 * 接线：物块倒下时广播的 `delete_wall` 必须带 **block + 坐标**。
 *
 * 服务端的掉落逻辑完全依赖这两个字段（掉什么、掉在哪）。少一个，整条链静默断掉 ——
 * 所以这条断言盯的是"消息里有没有货"，而不是"有没有发消息"。
 */
TEST(BlockDrop, DestroyedBlockAnnouncesWhatFellAndWhere) {
    Map map;
    int brickId = -1;
    const QRect probe = findBlock(map, QStringLiteral("brick"), &brickId);
    ASSERT_FALSE(probe.isNull()) << "没有找到含砖块的关卡";

    QJsonObject message;
    bool gotDelete = false;
    QObject::connect(&map, &Map::broadcastMessage, &map, [&](const QJsonObject &json) {
        if (json.value(QStringLiteral("type")).toString() == QLatin1String("delete_wall")) {
            message = json;
            gotDelete = true;
        }
    });

    for (int i = 0; i < 4; ++i)   // 砖 4 血、普弹 1 伤 → 第 4 下倒
        map.checkBulletCollision(probe, plainBullet());

    ASSERT_TRUE(gotDelete) << "砖块被打掉了却没有广播 delete_wall";
    EXPECT_EQ(message.value(QStringLiteral("block")).toString(), QStringLiteral("brick"))
        << "报文没带物块 id —— 服务端据此查掉落表";
    EXPECT_GT(message.value(QStringLiteral("x")).toInt(), 0);
    EXPECT_GT(message.value(QStringLiteral("y")).toInt(), 0);
}
