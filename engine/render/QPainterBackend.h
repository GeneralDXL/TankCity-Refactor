#ifndef ENGINE_RENDER_QPAINTERBACKEND_H
#define ENGINE_RENDER_QPAINTERBACKEND_H

#include "render/IRenderer.h"

class QPainter;

namespace engine::render {

/**
 * `IRenderer` 的 QPainter 实现 —— **engine 里唯一持有 `QPainter` 的地方**。
 *
 * 它不拥有 painter，只借用；调用方负责保证 painter 的生命周期长于本对象。
 */
class QPainterBackend : public IRenderer
{
public:
    explicit QPainterBackend(QPainter &painter);

    void drawTexture(const QPixmap &texture, const QRect &target) override;
    void drawRect(const QRect &rect, const QColor &fill, const QColor &border,
                  qreal borderWidth = 1.0) override;

private:
    QPainter &painter_;
};

} // namespace engine::render

#endif // ENGINE_RENDER_QPAINTERBACKEND_H
