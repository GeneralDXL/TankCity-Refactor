#ifndef ENGINE_ASSET_ASSETPATHS_H
#define ENGINE_ASSET_ASSETPATHS_H

#include <QString>

namespace engine::asset {

/// 资源根相对**可执行文件目录**的默认位置：开发期 exe 在 `<仓库根>/build/bin`，
/// 两级之上就是仓库根。
inline constexpr const char *kDefaultRelativeRoot = "../../assets";

/**
 * 资源定位：把资源相对路径解析成绝对路径。
 *
 * 基准是**可执行文件所在目录**，而不是当前工作目录。搬迁前的客户端写的是
 * `./../../assets/...` 这类 cwd 相对路径 —— 只有当前工作目录恰好是 `build/bin`
 * 时才碰巧成立：换个目录启动、或从 IDE 直接运行，贴图就**静默加载失败**
 * （`QPixmap` 加载不出只返回空，不抛异常）；打包发给别人则必然失败。
 * 服务端当时已改用 `applicationDirPath()`，这里把两边的做法统一成一处。
 *
 * `*From` 这组重载接受显式的「可执行文件目录」，因此可以脱离
 * `QCoreApplication` 单元测试；另一组只是它的便捷包装。
 */
QString assetRootFrom(const QString &applicationDir,
                      const QString &relativeRoot = kDefaultRelativeRoot);
QString assetPathFrom(const QString &applicationDir, const QString &relativePath,
                      const QString &relativeRoot = kDefaultRelativeRoot);

/// 便捷包装：取 `QCoreApplication::applicationDirPath()` 作为基准。
QString assetRoot(const QString &relativeRoot = kDefaultRelativeRoot);
QString assetPath(const QString &relativePath,
                  const QString &relativeRoot = kDefaultRelativeRoot);

} // namespace engine::asset

#endif // ENGINE_ASSET_ASSETPATHS_H
