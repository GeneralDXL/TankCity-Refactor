#include "physics/Grid.h"

namespace engine::physics {

Grid::Grid(int cellSize)
    : cellSize_(cellSize > 0 ? cellSize : 1)   // 0 会让换算除零，宁可退化成 1
{
}

QPoint Grid::toCell(const QPoint &world) const
{
    return QPoint(world.x() / cellSize_, world.y() / cellSize_);
}

QPoint Grid::toWorld(const QPoint &cell) const
{
    return QPoint(cell.x() * cellSize_ + cellSize_ / 2,
                  cell.y() * cellSize_ + cellSize_ / 2);
}

QRect Grid::cellRect(const QPoint &cell) const
{
    return QRect(cell.x() * cellSize_, cell.y() * cellSize_, cellSize_, cellSize_);
}

QRect Grid::cellRect(const QPoint &cell, int padding) const
{
    return QRect(cell.x() * cellSize_ + padding,
                 cell.y() * cellSize_ + padding,
                 cellSize_ - 2 * padding,
                 cellSize_ - 2 * padding);
}

} // namespace engine::physics
