#include "bullet.h"
#include <QtMath>
#include <cmath>

// bullet.cpp
Bullet::Bullet(const QPoint &position, float angle, BulletType type, double speed, int damage,
               CollisionCheckFunc collisionCallback)
    : position(position), angle(angle), type(type), speed(speed),
    collisionCheckCallback(collisionCallback), injury(damage)
{
}

void Bullet::move()
{
    // 保存移动前的位置
    QPoint oldPos = position;

    // 使用浮点数计算角度
    float rad = qDegreesToRadians(angle);
    position.rx() += speed * cos(rad);
    position.ry() += speed * sin(rad);

    if (position.x() < -1000 || position.x() > 10000 ||
        position.y() < -1000 || position.y() > 10000)
    {
        // 标记为需要移除而不是回退位置
        position = QPoint(-2000, -2000); // 确保被边界检测移除
        return;
    }
    // 碰撞检测回调函数
    if (collisionCheckCallback) {
        // 只检查当前位置，不再检查路径上的点
        if (collisionCheckCallback(QRect(position.x() - 5, position.y() - 5, 10, 10), injury)) {
            // 遇到碰撞直接移出屏幕，以便被移除
            position = QPoint(-2000, -2000);
            return;
        }
    }
}

bool Bullet::isOutOfBounds(int width, int height) const
{
    return position.x() < -20 || position.x() > width + 20 ||
           position.y() < -20 || position.y() > height + 20;
}

QRect Bullet::getRect() const
{
    return QRect(position.x() - 5, position.y() - 5, 10, 10);
}

BulletType Bullet::getType() const
{
    return type;
}
