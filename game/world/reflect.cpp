#include "reflect.h"

#include <QtGlobal>

namespace tankcity::world {

namespace {

/// 两矩形在某一轴上的重叠长度（像素）。
/// QRect 的两端都是**闭区间**，所以重叠像素数要 +1。
int overlapOnAxis(int a1, int a2, int b1, int b2)
{
    const int lo = qMax(a1, b1);
    const int hi = qMin(a2, b2);
    return hi >= lo ? hi - lo + 1 : 0;
}

} // namespace

QPointF normalOf(Face face)
{
    switch (face) {
    case Face::Left:
        return QPointF(-1, 0);
    case Face::Right:
        return QPointF(1, 0);
    case Face::Top:
        return QPointF(0, -1);
    case Face::Bottom:
        return QPointF(0, 1);
    }
    return QPointF(0, 0);   // 不可达；只为消除"缺少返回语句"的警告
}

Face faceOf(const QRect &hitBox, const QRect &wallRect)
{
    const int overlapX =
        overlapOnAxis(hitBox.left(), hitBox.right(), wallRect.left(), wallRect.right());
    const int overlapY =
        overlapOnAxis(hitBox.top(), hitBox.bottom(), wallRect.top(), wallRect.bottom());

    // 穿透更浅的那个轴 = 接触面所在的轴。相等时固定取 x 轴（确定性优于"看着对"）。
    if (overlapX <= overlapY) {
        return hitBox.center().x() <= wallRect.center().x() ? Face::Left : Face::Right;
    }
    return hitBox.center().y() <= wallRect.center().y() ? Face::Top : Face::Bottom;
}

QPointF reflectVector(const QPointF &velocity, Face face)
{
    const QPointF n = normalOf(face);
    const double dot = velocity.x() * n.x() + velocity.y() * n.y();
    return QPointF(velocity.x() - 2 * dot * n.x(),
                   velocity.y() - 2 * dot * n.y());
}

} // namespace tankcity::world
