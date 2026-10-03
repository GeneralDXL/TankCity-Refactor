#include "map.h"

#include "reflect.h"   // faceOf：撞在哪一面（M4 步 2）

#include <QJsonObject>

// M3：几何部分（格点换算、走线、碰撞查询、物块表）已迁入 World（world.h / world.cpp）。
// 本文件只剩**规则**：子弹命中物块之后会发生什么。
//
// 拆分判据是「这行代码会不会向外发消息」—— 会，所以它留在这里；`World` 连 QObject
// 都不是，结构上就不可能发消息。

Map::Map() = default;

bool Map::loadLevel(const tankcity::config::LevelData &level,
                    const tankcity::config::Config &config)
{
    return world_.loadLevel(level, config);
}

BulletHit Map::checkBulletCollision(const QRect &rect,
                                    const tankcity::config::BulletProfile &profile)
{
    BulletHit result;

    // 几何：先问世界「第一个挡子弹的物块是谁」（顺序由 CollisionWorld 的插入顺序保证，
    // 与旧实现「首个命中即返回」一致）。
    const int hitWallId = world_.firstShotBlockerId(rect);
    if (hitWallId < 0)
        return result;

    Wall *wall = world_.mutableWallById(hitWallId);
    if (wall == nullptr)
        return result;

    result.hit = true;
    result.wallRect = wall->getRect();
    // 撞在哪一面由几何决定：最小穿透轴；正对角落（两轴穿透相等）固定取 x 轴。
    // 确定性是刻意的 —— 无状态的"每帧重猜"正是 M3 贴角抽搐的成因（见 reflect.h）。
    result.face = tankcity::world::faceOf(rect, result.wallRect);

    // 可弹物块：这里只报告"它可弹"，弹不弹（还有没有次数）由**子弹**决定。
    // 规则来自手稿：弹射仅限钢材与边界；击中砖块等直接走"破坏"这条路。
    // 弹射与穿甲互斥（加载器已校验），所以可弹的子弹本来就打不动钢材，两条规则不会打架。
    if (profile.bounceOn.contains(wall->getBlockId())) {
        result.bouncable = true;
        return result;   // 可弹物块不吃伤害
    }

    // 规则：可破坏 **且** 这发子弹打得动它，才吃伤害（M4 步 1）。
    // 钢材的 requiredBulletTags 是 ["armorPiercing"]，普通弹打上去只是"打在钢板上"：
    // 不扣血，但子弹照样命中消失 —— 这正是手感上"打不动"的表达。
    if (wall->isDestructible() && wall->acceptsBullet(profile.tags)) {
        wall->setHealth(wall->getHealth() - profile.damage);
    }

    if (wall->isDestructible() && wall->acceptsBullet(profile.tags) && wall->getHealth() <= 0) {
        // 先取 id 再移除：removeWallById 之后 wall 指针已失效
        const int deadWallId = wall->getId();

        QJsonObject json;
        json["type"] = "delete_wall";
        json["wallId"] = deadWallId;
        emit broadcastMessage(json);

        world_.removeWallById(deadWallId);
    }
    return result;   // 命中即返回（是否消失由子弹决定）
}
