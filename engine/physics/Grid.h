#ifndef ENGINE_PHYSICS_GRID_H
#define ENGINE_PHYSICS_GRID_H

#include <QPoint>
#include <QRect>
#include <cstdlib>

namespace engine::physics {

/**
 * 方格世界的坐标换算与「两点之间走过哪些格」。
 *
 * 尺寸与内缩都当参数传进来（调用方现在传的是旧的 40 / 5 两个魔数），
 * 引擎不假设世界有多大 —— 世界尺寸是上层的事。
 *
 * 这些换算原本散在地图层里，寻路与「某个单位能否通过这一格」的判定都在用；
 * M3 的 tile 世界要的正是这套东西。
 */
class Grid
{
public:
    explicit Grid(int cellSize);

    int cellSize() const { return cellSize_; }

    /// 世界坐标 → 格坐标。整数除法，负坐标向零截断（与旧实现逐位一致）。
    QPoint toCell(const QPoint &world) const;
    /// 格坐标 → 该格中心的世界坐标（`cell * size + size / 2`）。
    QPoint toWorld(const QPoint &cell) const;

    /// 整格区域。
    QRect cellRect(const QPoint &cell) const;
    /// 某格内缩 `padding` 后的区域（判定「这一格能不能过」时用）。
    QRect cellRect(const QPoint &cell, int padding) const;

    /**
     * 用 Bresenham 从 `fromCell` 的中心走到 `endCell` 的中心，对**经过的每一格**调用
     * `visitor(cell)`；visitor 返回 false 即提前结束并返回 false，走完没被拒绝则返回 true。
     *
     * （参数不叫 `toCell`，否则会遮蔽同名的成员函数。）
     *
     * 与搬迁前那版逐点等价（`game/world` 里原来的走线判定）。两个容易看错的细节：
     *  - **起终点两格都会被检查**：循环在 `current == end` 时退出，而终点是格中心，
     *    像素级一定先踏入终点格的范围（如 (0,0)→(4,0) 会依次回调 0,1,2,3,4 五格）；
     *  - **起终点同格时一次都不回调、直接返回 true**（循环体不执行）。
     *
     * 唯一的有意差异：连续重复的同一格只回调一次（旧实现每格在像素级会重复检查约 40 次）。
     * 判定函数是纯函数，少调几次不改变结果，但 A* 内层循环能少跑很多。
     */
    template <typename Visitor>
    bool walkLine(const QPoint &fromCell, const QPoint &endCell, Visitor &&visitor) const
    {
        const QPoint start = toWorld(fromCell);
        const QPoint end = toWorld(endCell);

        int dx = std::abs(end.x() - start.x());
        int dy = std::abs(end.y() - start.y());
        int sx = (start.x() < end.x()) ? 1 : -1;
        int sy = (start.y() < end.y()) ? 1 : -1;
        int err = dx - dy;

        QPoint current = start;
        QPoint previousCell;
        bool hasPrevious = false;

        while (current != end) {
            const QPoint cell = toCell(current);
            if (!hasPrevious || cell != previousCell) {
                if (!visitor(cell))
                    return false;
                previousCell = cell;
                hasPrevious = true;
            }

            const int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                current.rx() += sx;
            }
            if (e2 < dx) {
                err += dx;
                current.ry() += sy;
            }
        }
        return true;
    }

private:
    int cellSize_;
};

} // namespace engine::physics

#endif // ENGINE_PHYSICS_GRID_H
