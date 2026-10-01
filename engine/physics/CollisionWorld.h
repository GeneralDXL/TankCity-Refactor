#ifndef ENGINE_PHYSICS_COLLISIONWORLD_H
#define ENGINE_PHYSICS_COLLISIONWORLD_H

#include <QVector>

#include "physics/Aabb.h"

namespace engine::physics {

/**
 * 静态碰撞体的集合：**只回答几何问题**。
 *
 * 它不知道上层那些实体是什么，也不知道撞上之后该发生什么
 * （扣血 / 销毁 / 广播）—— 那些留在游戏层。它只做两件事：
 *  - 用调用方定义的**位标记**（flags）把物体分组，含义由调用方赋予；
 *  - 按**插入顺序**遍历与给定盒子相交、且标记匹配的物体。
 *
 * ## 为什么是「遍历」而不是「返回一个列表」
 * 游戏层的几个查询都是「**首个命中即返回**」，语义依赖插入顺序
 * （回归用例里有专门的顺序敏感性断言）。遍历式接口既保住了顺序，
 * 又避免每次查询都分配列表 —— `isCellWalkable` 在 A* 内层循环里会被调用成千上万次。
 *
 * ## 为什么遍历函数是模板
 * 走 `std::function` 会在每次调用上多一层间接；模板 + 内联 lambda 是零开销的。
 *
 * @code
 * world.forEachOverlap(box, kBlocksMove, [&](const Body &b) {
 *     hitId = b.id;
 *     return false;   // 只要第一个，停止遍历
 * });
 * @endcode
 */
class CollisionWorld
{
public:
    /// 物体的外部标识（含义由调用方定义，例如地图物块的 id）。
    using BodyId = int;
    /// 位标记（含义由调用方定义，例如「挡移动」「挡投射物」）。
    using Flags = quint32;

    struct Body
    {
        BodyId id = 0;
        Flags flags = 0;
        Aabb box;
    };

    void clear();
    /// 追加一个物体（保持插入顺序）。
    void add(BodyId id, const Aabb &box, Flags flags);
    /// 按 id 移除；物体不存在时返回 false。
    bool remove(BodyId id);
    /// 按 id 查物体；不存在返回 nullptr。
    const Body *find(BodyId id) const;

    /**
     * 按插入顺序遍历：与 `box` 相交 **且** `(body.flags & mask) != 0` 的物体。
     * visitor 返回 false 表示提前停止。
     */
    template <typename Visitor>
    void forEachOverlap(const Aabb &box, Flags mask, Visitor &&visitor) const
    {
        for (const Body &body : bodies_) {
            if ((body.flags & mask) == 0)
                continue;
            if (!body.box.intersects(box))
                continue;
            if (!visitor(body))
                return;
        }
    }

    bool empty() const { return bodies_.isEmpty(); }
    int size() const { return bodies_.size(); }

private:
    QVector<Body> bodies_;
};

} // namespace engine::physics

#endif // ENGINE_PHYSICS_COLLISIONWORLD_H
