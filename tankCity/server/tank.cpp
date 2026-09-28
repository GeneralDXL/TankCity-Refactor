#include "tank.h"
#include <QDebug>

Tank::Tank(Map *gameMap, int health, double speed, int shootDelay)
    : gameMap(gameMap), health(health), speed(speed), shootDelay(shootDelay)
{
    shootCooldown = 0;
}

void Tank::move(float angle, float distance, Map *map)
{
    if (shootCooldown > 0) shootCooldown--;

    // 获取当前地形类型
    int terrainType = map->getTerrainType(position);
    // 计算移动向量
    float rad = qDegreesToRadians(angle);
    float moveX = cos(rad) * distance;
    float moveY = sin(rad) * distance;
    if(terrainType==FOREST)
    {
        moveX = cos(rad) * distance*0.5f;
        moveY = sin(rad) * distance*0.5f;
    }
    else if(terrainType==ICE)
    {
        moveX = cos(rad) * distance*1.5f;
        moveY = sin(rad) * distance*1.5f;
    }


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
