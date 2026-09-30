#include "bullet.h"
#include <QPainter>
#include <cmath>
#include <QPainterPath>

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

void Bullet::draw(QPainter &painter) const
{
    painter.save();
    painter.translate(position);
    painter.rotate(angle); // 根据子弹角度旋转

    // 子弹主体 - 流线型设计
    QPainterPath path;
    path.moveTo(-8, 0);
    path.lineTo(6, -3);
    path.lineTo(6, 3);
    path.closeSubpath();

    // 子弹颜色
    QColor bulletColor = type == BulletType::Player ?
                             QColor(100, 255, 255) : QColor(255, 100, 100);

    painter.setPen(QPen(bulletColor.darker(), 1));
    painter.setBrush(bulletColor);
    painter.drawPath(path);

    // 弹头尖端
    QPolygonF tip;
    tip << QPointF(6, -3) << QPointF(10, 0) << QPointF(6, 3);
    painter.setBrush(bulletColor.lighter(150));
    painter.drawPolygon(tip);

    painter.restore();

    // 绘制尾迹
    QPoint prevPos = position;
    prevPos.rx() -= 12 * cos(angle * M_PI / 180.0f);
    prevPos.ry() -= 12 * sin(angle * M_PI / 180.0f);

    QRadialGradient grad(prevPos, 8);
    grad.setColorAt(0, type == BulletType::Player ?
                           QColor(100, 255, 255, 150) : QColor(255, 100, 100, 150));
    grad.setColorAt(1, Qt::transparent);

    painter.setBrush(grad);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(prevPos, 6, 6);
}

QRect Bullet::getRect() const
{
    return QRect(position.x() - 5, position.y() - 5, 10, 10);
}

BulletType Bullet::getType() const
{
    return type;
}
