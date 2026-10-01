#ifndef ENGINE_RENDER_TEXTURECACHE_H
#define ENGINE_RENDER_TEXTURECACHE_H

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QString>

namespace engine::render {

/**
 * 贴图缓存：按「相对资源根目录的路径」取图，**每个文件只加载一次**。
 *
 * 之前这 5 张地形贴图是在 `MapPainter::draw()` 里现场 `QPixmap(...)` 构造的，
 * 也就是**每帧每张都重新解码一次 JPEG**（一类很贵的小浪费）；缓存之后只在
 * 首次用到时读盘。视觉上没有区别，纯收益。
 *
 * 失败路径只警告一次，避免每帧刷屏；返回空 `QPixmap`，由调用方
 * （见 `IRenderer::drawTexture`）表现为「什么都不画」—— 与旧行为一致。
 *
 * 刻意只管「读盘 + 缓存 + 按路径索引」，不认识任何具体实体概念；
 * 「哪种物块用哪张贴图」留在表现层（`client/render`）。
 */
class TextureCache
{
public:
    /// @param assetRoot 资源根目录（客户端目前传 `./../../assets`，M1.1 后续会改为按可执行文件定位）
    explicit TextureCache(QString assetRoot);

    /**
     * 取贴图；`relativePath` 相对资源根目录，例如 `images/textures/brick.jpg`。
     * 按值返回（QPixmap 是隐式共享，拷贝很便宜）—— 避免调用方持有
     * 指向哈希表内部的引用，那会在后续插入时失效。
     */
    QPixmap texture(const QString &relativePath);

    QString assetRoot() const { return assetRoot_; }

private:
    QString assetRoot_;
    QHash<QString, QPixmap> cache_;
    QSet<QString> warned_;
};

} // namespace engine::render

#endif // ENGINE_RENDER_TEXTURECACHE_H
