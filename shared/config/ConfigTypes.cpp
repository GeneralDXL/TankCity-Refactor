#include "ConfigTypes.h"

namespace tankcity::config {

QVector<LevelRect> LevelData::allRects() const
{
    QVector<LevelRect> result;
    for (const LevelLayer &layer : layers) {
        for (const LevelRect &lr : layer.rects)
            result.append(lr);
    }
    return result;
}

QVector<QRect> LevelData::boundaryRects() const
{
    if (!boundaryEnabled)
        return {};

    const int t = boundaryThickness;
    // 厚到把内部区域占满时视为没有边界（加载期已拒绝这种数据，这里只做兜底）。
    if (t <= 0 || worldWidth <= 2 * t || worldHeight <= 2 * t)
        return {};

    const int innerH = worldHeight - 2 * t;
    return {
        QRect(0, 0, worldWidth, t),                 // 上
        QRect(0, worldHeight - t, worldWidth, t),   // 下
        QRect(0, t, t, innerH),                     // 左
        QRect(worldWidth - t, t, t, innerH),        // 右
    };
}

} // namespace tankcity::config
