#include "bullet.h"

#include <QtMath>

#include <cmath>

namespace {

/// 命中盒的一半（`getRect()` 是 10×10）。
constexpr int kHalfBox = 5;

} // namespace

Bullet::Bullet(const QPoint &position, float angle, BulletType type, double speed,
               const tankcity::config::BulletProfile &profile,
               CollisionCheckFunc collisionCallback)
    : position(position), exactPosition(position), velocity_(0, 0), type(type),
      profile_(profile), bouncesLeft_(profile.maxBounces),
      collisionCheckCallback(collisionCallback)
{
    // 角度只在这里出现一次：之后一切从速度向量推导，`getAngle()` 再反推回去。
    // 这样"改成速度向量"对协议与渲染都是透明的（客户端仍收到 angle）。
    const double rad = qDegreesToRadians(static_cast<double>(angle));
    velocity_ = QPointF(std::cos(rad) * speed, std::sin(rad) * speed);
}

void Bullet::move()
{
    // 亚像素累加：旧实现是 `position += (int)(speed*cos)`，斜向每帧都被向零截断
    // （与 M3 登记的"斜向慢 15%"同源，子弹在 1px 量级上更明显）。
    // 现在精确位置逐帧累加、整数位置只做取整 —— 长距离速度因此是准的。
    exactPosition += velocity_;
    position = QPoint(qRound(exactPosition.x()), qRound(exactPosition.y()));

    if (position.x() < -1000 || position.x() > 10000 ||
        position.y() < -1000 || position.y() > 10000)
    {
        kill();
        return;
    }

    if (!collisionCheckCallback)
        return;

    const BulletHit hit = collisionCheckCallback(getRect(), profile_);
    if (!hit.hit)
        return;

    if (hit.bouncable && bouncesLeft_ > 0) {
        // 弹射：只在这里改**速度方向**，位置由 placeOutside 摆到墙外
        --bouncesLeft_;
        velocity_ = tankcity::world::reflectVector(velocity_, hit.face);
        placeOutside(hit.wallRect, hit.face);
        return;   // 弹走了，这发子弹不消失
    }

    kill();
}

bool Bullet::isOutOfBounds(int width, int height) const
{
    return position.x() < -20 || position.x() > width + 20 ||
           position.y() < -20 || position.y() > height + 20;
}

QRect Bullet::getRect() const
{
    return QRect(position.x() - kHalfBox, position.y() - kHalfBox, kHalfBox * 2, kHalfBox * 2);
}

BulletType Bullet::getType() const
{
    return type;
}

float Bullet::getAngle() const
{
    if (velocity_.x() == 0.0 && velocity_.y() == 0.0)
        return 0.0f;   // 速度为零（配置成 0 的极端情况）时给一个确定值，不返回 NaN
    return static_cast<float>(qRadiansToDegrees(qAtan2(velocity_.y(), velocity_.x())));
}

void Bullet::placeOutside(const QRect &wallRect, tankcity::world::Face face)
{
    // "贴到外侧"= 中心离表面 `kHalfBox + 1` 像素。**固定摆位**而不是"往回退一点"：
    // 确定性、可测，且不会被高速子弹一步跨过墙面再判定一次。
    const double margin = kHalfBox + 1;
    switch (face) {
    case tankcity::world::Face::Left:     // 物块在子弹右侧 ⇒ 摆到物块左边外侧
        exactPosition.rx() = wallRect.left() - margin;
        break;
    case tankcity::world::Face::Right:
        exactPosition.rx() = wallRect.right() + margin;
        break;
    case tankcity::world::Face::Top:
        exactPosition.ry() = wallRect.top() - margin;
        break;
    case tankcity::world::Face::Bottom:
        exactPosition.ry() = wallRect.bottom() + margin;
        break;
    }
    position = QPoint(qRound(exactPosition.x()), qRound(exactPosition.y()));
}

void Bullet::kill()
{
    exactPosition = QPointF(-2000, -2000);
    position = QPoint(-2000, -2000);   // 由服务端的越界清扫移除（旧约定）
}
