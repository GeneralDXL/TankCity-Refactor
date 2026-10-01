#include "render/QPainterBackend.h"

#include <QPainter>

namespace engine::render {

QPainterBackend::QPainterBackend(QPainter &painter)
    : painter_(painter)
{
}

void QPainterBackend::drawTexture(const QPixmap &texture, const QRect &target)
{
    if (texture.isNull())
        return;   // 与旧的 drawPixmap(target, 空QPixmap) 一致：什么都不画

    painter_.drawPixmap(target, texture);
}

void QPainterBackend::drawRect(const QRect &rect, const QColor &fill, const QColor &border,
                               qreal borderWidth)
{
    painter_.setPen(QPen(border, borderWidth));
    painter_.setBrush(fill);
    painter_.drawRect(rect);
}

} // namespace engine::render
