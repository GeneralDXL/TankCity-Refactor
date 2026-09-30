#include "tank.h"
#include <QDebug>

Tank::Tank(Map *gameMap, const tankcity::config::TankStats &stats)
    : health(stats.health), speed(stats.moveSpeed), bulletSpeed(stats.bulletSpeed),
      bulletDamage(stats.bulletDamage), muzzleOffset(stats.muzzleOffset),
      shootDelay(stats.shootDelayTicks), gameMap(gameMap)
{
    shootCooldown = 0;
}

void Tank::move(float angle, float distance, Map *map)
{
    if (shootCooldown > 0) shootCooldown--;

    // 地形对移动的影响来自 blocks.json 的 moveSpeedFactor（森林 0.5、冰 1.5），
    // 无影响时为 1.0。与旧代码「只认 FOREST/ICE 两个类型」等价 —— 取倍率而不是
    // 判断类型，新增地形不必再改这里。
    const float scale = static_cast<float>(map->getMoveSpeedFactor(position));
    // 计算移动向量（乘 1.0f 是精确的，不受地形影响时与旧写法逐位一致）
    float rad = qDegreesToRadians(angle);
    float moveX = cos(rad) * distance * scale;
    float moveY = sin(rad) * distance * scale;


    QPoint newPos = position + QPoint(moveX, moveY);

    // 创建测试矩形
    QRect testRect(newPos.x() - 20, newPos.y() - 20, 40, 40);

    if(!canMove(angle, distance, map)) {
        // 如果不能移动，直接返回
        qDebug() << "Tank cannot move to position:" << newPos;
        return;
    }

    int check = map->checkTankCollision(testRect, this);
    switch (check)
    {
    case FOREST:
    case ICE:
        position = newPos;
        return;

    case BOUNDARY:
    case BRICK:
    case STEEL:
    case SEA:
        // 如果是边界或不可破坏的墙体，停止移动
        return;

    default:
        // 如果没有碰撞，直接更新位置
        position = newPos;
        return;
    }
}

bool Tank::canMove(float angle, float distance, Map *map) const
{
    // 计算移动向量
    float rad = qDegreesToRadians(angle);
    float moveX = cos(rad) * distance;
    float moveY = sin(rad) * distance;

    QPoint testPos = position + QPoint(moveX, moveY);

    // 创建测试矩形
    QRect testRect(testPos.x() - 20, testPos.y() - 20, 40, 40);

    int check = map->checkTankCollision(testRect, this);

    return check == FOREST || check == ICE || check == -1;
}

void Tank::takeDamage(int amount)
{
    health -= amount;
    if (health < 0) health = 0;
}

bool Tank::isDestroyed() const
{
    return health <= 0;
}

int Tank::getHealth() const
{
    return health;
}

QPoint Tank::getPosition() const
{
    return position;
}
