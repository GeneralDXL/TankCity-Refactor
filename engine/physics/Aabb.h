#ifndef ENGINE_PHYSICS_AABB_H
#define ENGINE_PHYSICS_AABB_H

#include <QRect>

namespace engine::physics {

/**
 * 轴对齐包围盒：整数像素、左上角 + 宽高，与 `QRect` 同一约定。
 *
 * ## 相交判定为什么委托给 QRect
 * M1.1 的验收口径是**行为不变**，而 `QRect` 的边界约定坑不少：
 * `QRect(x, y, w, h)` 覆盖的是 `x..x+w-1`，空矩形（w 或 h 为 0）不与任何东西相交，
 * 恰好贴边的两个矩形也**不相交**。自己照抄一遍公式很容易在差一像素上出岔子，
 * 而且这种错误在游戏里极难发现。所以这里直接把判定转交给 `QRect`，
 * 等 M3 需要浮点 / 连续坐标时再换实现（那时会连渲染一起改，有截图回归兜底）。
 */
struct Aabb
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    Aabb() = default;
    Aabb(int x, int y, int w, int h) : x(x), y(y), w(w), h(h) {}

    static Aabb fromRect(const QRect &r) { return Aabb(r.x(), r.y(), r.width(), r.height()); }
    QRect toRect() const { return QRect(x, y, w, h); }

    bool intersects(const Aabb &other) const { return toRect().intersects(other.toRect()); }
    bool contains(int px, int py) const { return toRect().contains(px, py); }

    /// 非空（宽高都为正）—— 空盒子与任何东西都不相交。
    bool isValid() const { return w > 0 && h > 0; }
};

} // namespace engine::physics

#endif // ENGINE_PHYSICS_AABB_H
