#ifndef TANKCITY_CONFIG_LOADER_H
#define TANKCITY_CONFIG_LOADER_H

#include "ConfigError.h"
#include "ConfigTypes.h"

#include <QHash>
#include <QString>
#include <QStringList>

namespace tankcity::config {

/**
 * 全部已校验的配置。由 ConfigLoader::loadFromDirectory() 一次性构建，之后只读。
 *
 * 所有查找接口未命中时返回 nullptr，调用方无需再处理异常。
 */
class Config {
public:
    GameConfig game;
    QVector<LayerDef> layers;  ///< 已按 renderOrder 升序排列

    QHash<QString, BlockDef> blocks;
    QHash<QString, BulletDef> bullets;
    QHash<QString, TankDef> tanks;

    QVector<ItemDef> items;
    ItemSpawnDef itemSpawn;

    QVector<DifficultyDef> difficulties;

    const LayerDef *layer(const QString &id) const;
    const BlockDef *block(const QString &id) const;
    const BulletDef *bullet(const QString &id) const;
    const TankDef *tank(const QString &id) const;
    const DifficultyDef *difficulty(const QString &id) const;

    QStringList layerIds() const;   ///< 按渲染顺序
    QStringList blockIds() const;   ///< 已排序，用于报错时提示可用值
    QStringList bulletIds() const;
    QStringList tankIds() const;
};

/**
 * 配置加载器。
 *
 * 全部校验前置：字段缺失 / 类型错误 / 引用不存在 / 互斥冲突，一律抛 ConfigError，
 * 错误信息包含「文件 + 字段路径 + 原因」，便于直接定位。
 */
class ConfigLoader {
public:
    /**
     * 读取 @p directory 下的 game / layers / blocks / entities / items / difficulty
     * 六个配置文件并做交叉校验。
     *
     * @throws ConfigError
     */
    static Config loadFromDirectory(const QString &directory);

    /** 重载：从显式指定的六个文件路径加载（便于测试与定制部署）。 */
    static Config loadFiles(const QString &gameFile,
                            const QString &layersFile,
                            const QString &blocksFile,
                            const QString &entitiesFile,
                            const QString &itemsFile,
                            const QString &difficultyFile);
};

} // namespace tankcity::config

#endif // TANKCITY_CONFIG_LOADER_H
