#include "asset/AssetPaths.h"

#include <QCoreApplication>
#include <QDir>

namespace engine::asset {

QString assetRootFrom(const QString &applicationDir, const QString &relativeRoot)
{
    // cleanPath 把 "…/build/bin/../../assets" 收敛成 "…/assets"：
    // 传出去的路径里不留 ".."，也就少了「一层没剥干净」这类隐患。
    return QDir::cleanPath(QDir(applicationDir).filePath(relativeRoot));
}

QString assetPathFrom(const QString &applicationDir, const QString &relativePath,
                      const QString &relativeRoot)
{
    return QDir(assetRootFrom(applicationDir, relativeRoot)).filePath(relativePath);
}

QString assetRoot(const QString &relativeRoot)
{
    return assetRootFrom(QCoreApplication::applicationDirPath(), relativeRoot);
}

QString assetPath(const QString &relativePath, const QString &relativeRoot)
{
    return assetPathFrom(QCoreApplication::applicationDirPath(), relativePath, relativeRoot);
}

} // namespace engine::asset
