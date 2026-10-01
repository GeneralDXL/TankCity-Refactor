#include "render/TextureCache.h"

#include <QDebug>
#include <QDir>
#include <utility>

namespace engine::render {

TextureCache::TextureCache(QString assetRoot)
    : assetRoot_(std::move(assetRoot))
{
}

QPixmap TextureCache::texture(const QString &relativePath)
{
    const auto cached = cache_.constFind(relativePath);
    if (cached != cache_.constEnd())
        return cached.value();

    const QString fullPath = QDir(assetRoot_).filePath(relativePath);
    QPixmap pixmap(fullPath);

    if (pixmap.isNull() && !warned_.contains(relativePath)) {
        qWarning() << "贴图加载失败：" << fullPath
                   << "（工作目录是否在 build/bin？贴图路径仍是相对路径）";
        warned_.insert(relativePath);
    }

    cache_.insert(relativePath, pixmap);   // 失败也缓存，避免每帧反复读盘
    return pixmap;
}

} // namespace engine::render
