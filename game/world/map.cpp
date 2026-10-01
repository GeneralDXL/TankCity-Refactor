#include "map.h"

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

bool Map::checkBulletCollision(const QRect &rect, int damage)
{
    // 几何：先问世界「第一个挡子弹的物块是谁」（顺序由 CollisionWorld 的插入顺序保证，
    // 与旧实现「首个命中即返回」一致）。
    const int hitWallId = world_.firstShotBlockerId(rect);
    if (hitWallId < 0)
        return false;

    Wall *wall = world_.mutableWallById(hitWallId);
    if (wall == nullptr)
        return false;

    // 规则：只有可破坏物块吃伤害（边界与钢墙在 blocks.json 里 destructible=false）
    if (wall->isDestructible()) {
        wall->setHealth(wall->getHealth() - damage);
    }

    if (wall->isDestructible() && wall->getHealth() <= 0) {
        // 先取 id 再移除：removeWallById 之后 wall 指针已失效
        const int deadWallId = wall->getId();

        QJsonObject json;
        json["type"] = "delete_wall";
        json["wallId"] = deadWallId;
        emit broadcastMessage(json);

        world_.removeWallById(deadWallId);
    }
    return true;
}
