/**
 * @file  ricochet_test.cpp
 * @brief M4 步 2：弹射 —— 谁可弹（配置驱动）、弹了之后子弹怎么变。
 *
 * 分两层测：
 *  · **`Map` 层**：哪些物块可弹（`bounceOn`，含保留 id `"boundary"`），以及"可弹物块不吃伤害"；
 *  · **`Bullet` 层**：撞上可弹面后速度反向、弹数减一、被摆到墙外、**弹数耗尽才消失**。
 *    后者用**桩回调**驱动 —— 与关卡内容无关，完全确定。
 */

#include "bullet.h"
#include "config/ConfigLoader.h"
#include "config/TankStats.h"
#include "map.h"

#include <gtest/gtest.h>

using tankcity::config::BulletProfile;
using tankcity::world::Face;

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

/// 与 entities.json 的 `bulletRicochet` 等价：伤害 1、不穿甲、可弹钢与边界、最多 3 次。
BulletProfile ricochetProfile()
{
    BulletProfile profile;
    profile.damage = 1;
    profile.bounceOn = QStringList{QStringLiteral("steel"), QStringLiteral("boundary")};
    profile.maxBounces = 3;
    return profile;
}

/// 找一个"干净"的物块（首个命中确实是它自己）—— 直接问世界，不推断插入顺序与矩形重叠。
QRect findBlock(Map &map, const QString &blockId, int *wallIdOut)
{
    constexpr int kShippedLevels = 14;
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

bool wallStillExists(const Map &map, int wallId)
{
    for (const Wall &wall : map.getWalls()) {
        if (wall.getId() == wallId)
            return true;
    }
    return false;
}

/// 桩回调：永远报"撞到一个可弹的竖直面" —— 用来单独检验子弹自己的反射逻辑。
BulletHit alwaysBouncable(const QRect &wall)
{
    BulletHit hit;
    hit.hit = true;
    hit.bouncable = true;
    hit.wallRect = wall;
    hit.face = Face::Left;
    return hit;
}

} // namespace

/** 钢材可弹，**且不吃伤害**（弹射与穿甲互斥，弹性弹本来也打不动它）。 */
TEST(Ricochet, SteelIsBouncableAndTakesNoDamage) {
    Map map;
    int steelId = -1;
    const QRect probe = findBlock(map, QStringLiteral("steel"), &steelId);
    ASSERT_FALSE(probe.isNull()) << "没有找到含钢材的关卡";

    for (int i = 0; i < 8; ++i) {
        const BulletHit hit = map.checkBulletCollision(probe, ricochetProfile());
        EXPECT_TRUE(hit.hit);
        EXPECT_TRUE(hit.bouncable) << "钢材在 bounceOn 清单里，应当可弹";
    }

    EXPECT_TRUE(wallStillExists(map, steelId)) << "可弹物块不该吃伤害";
}

/** 砖块**不可弹**：走"破坏"这条路（手稿：击中砖块时优先执行破坏）。 */
TEST(Ricochet, BrickIsNotBouncableAndStillTakesDamage) {
    Map map;
    int brickId = -1;
    const QRect probe = findBlock(map, QStringLiteral("brick"), &brickId);
    ASSERT_FALSE(probe.isNull());

    const BulletHit hit = map.checkBulletCollision(probe, ricochetProfile());
    EXPECT_TRUE(hit.hit);
    EXPECT_FALSE(hit.bouncable) << "砖块不在 bounceOn 清单里";

    for (int i = 1; i < 4; ++i)
        EXPECT_TRUE(map.checkBulletCollision(probe, ricochetProfile()).hit);
    EXPECT_FALSE(wallStillExists(map, brickId)) << "弹性弹打砖块仍应 4 下破（走破坏那条路）";
}

/**
 * 边界可弹 —— 同时证明**加载器的保留 id 豁免端到端生效**：
 * `"boundary"` 不是 blocks.json 里的物块，却能被 `bounceOn` 引用（手稿：弹射仅限钢材与边界）。
 */
TEST(Ricochet, BoundaryIsBouncable) {
    Map map;
    int boundaryId = -1;
    const QRect probe = findBlock(map, QStringLiteral("boundary"), &boundaryId);
    ASSERT_FALSE(probe.isNull()) << "没有找到边界墙";

    const BulletHit hit = map.checkBulletCollision(probe, ricochetProfile());
    EXPECT_TRUE(hit.bouncable) << "边界应当可弹（加载器需要为保留 id 放行）";
    EXPECT_TRUE(wallStillExists(map, boundaryId));
}

/** `Bullet` 层：撞可弹面 → 速度反向、弹数减一、被摆到墙外（不会卡在墙里）。 */
TEST(Ricochet, BulletReflectsAndStepsOutsideTheWall) {
    const QRect wall(100, 0, 40, 200);

    Bullet bullet(QPoint(80, 100), 0.0f, BulletType::Player, 5.0, ricochetProfile(),
                  [&wall](const QRect &rect, const BulletProfile &) {
                      BulletHit hit;
                      if (rect.right() >= wall.left()) {   // 命中盒碰到墙面才报命中
                          hit.hit = true;
                          hit.bouncable = true;
                          hit.wallRect = wall;
                          hit.face = Face::Left;
                      }
                      return hit;
                  });

    for (int i = 0; i < 20 && bullet.velocity().x() > 0; ++i)
        bullet.move();

    EXPECT_LT(bullet.velocity().x(), 0.0) << "撞上可弹面后没有反向";
    EXPECT_EQ(bullet.bouncesLeft(), 2) << "弹一次应当只减一次弹数";
    EXPECT_LT(bullet.getPosition().x(), wall.left())
        << "反射后应被摆到**墙外侧**，否则下一帧会再次判成同一面的碰撞";
    EXPECT_NEAR(bullet.getAngle(), 180.0, 1.0)
        << "角度由速度反推 —— 协议里的 angle 字段要跟着变（客户端据此画朝向）";
}

/** 弹数耗尽：`maxBounces` 次之后，下一次命中就消失。 */
TEST(Ricochet, BulletDiesWhenBouncesRunOut) {
    const QRect wall(100, 0, 40, 200);
    BulletProfile profile = ricochetProfile();
    profile.maxBounces = 2;

    Bullet bullet(QPoint(0, 100), 0.0f, BulletType::Player, 5.0, profile,
                  [&wall](const QRect &, const BulletProfile &) { return alwaysBouncable(wall); });

    for (int i = 1; i <= 2; ++i) {
        bullet.move();
        EXPECT_EQ(bullet.bouncesLeft(), 2 - i) << "第 " << i << " 次应当消耗一次弹数";
    }

    bullet.move();   // 第三次：弹数为 0 → 不再弹，改为消失
    EXPECT_EQ(bullet.bouncesLeft(), 0);
    EXPECT_TRUE(bullet.isOutOfBounds(Map::MAP_WIDTH, Map::MAP_HEIGHT))
        << "弹数耗尽后应被标记为待清除（沿用把位置移到屏幕外的旧约定）";
}
