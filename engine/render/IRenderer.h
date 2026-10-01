#ifndef ENGINE_RENDER_IRENDERER_H
#define ENGINE_RENDER_IRENDERER_H

#include <QColor>
#include <QPixmap>
#include <QRect>

namespace engine::render {

/**
 * 绘制原语 —— **只包含与游戏无关的那几种**。
 *
 * ## 为什么不做 QPainter 的完整镜像
 * 客户端画那些实体用的是渐变、路径、多边形、字体……把这些也抽象进来，
 * 得到的会是一份**恰好等于 QPainter 的接口**：多一层间接、零收益，
 * 还要把几百行美术代码重写一遍（M1.1 决议 D3-A 明确要避免这个）。
 *
 * 所以覆盖面刻意压到最小：目前只有地图层的贴图绘制用得上它 —— 那正好是
 * 「与游戏无关的绘制」在这个项目里真实存在的部分。要不要扩大覆盖面，
 * 等 M6 重做表现层时再决定。
 *
 * 实现只有 `QPainterBackend` 一个（engine 内唯一持有 `QPainter` 的地方）。
 */
class IRenderer
{
public:
    virtual ~IRenderer() = default;

    /**
     * 把整张贴图缩放铺满 target。
     *
     * 贴图为空（文件缺失/加载失败）时**什么都不画** —— 与旧代码里
     * `drawPixmap(target, 空QPixmap)` 的行为一致。缺失由 `TextureCache` 报警。
     */
    virtual void drawTexture(const QPixmap &texture, const QRect &target) = 0;

    /**
     * 填充矩形并描边。
     *
     * 两个颜色都要传：旧地图里「没有贴图的物块」画的是 `drawRect`，
     * 也就是「当前画刷填充 + 当前画笔描边」——当时的画笔是 QPainter 的默认黑笔，
     * 所以视觉上是「灰底 + 黑边」。只填充不描边会丢掉那圈边框，画面便与基线不同。
     */
    virtual void drawRect(const QRect &rect, const QColor &fill, const QColor &border,
                          qreal borderWidth = 1.0) = 0;
};

} // namespace engine::render

#endif // ENGINE_RENDER_IRENDERER_H
