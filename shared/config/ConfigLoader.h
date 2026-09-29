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

    /**
     * 读取单个关卡文件。
     *
     * 需要传入已加载的 @p config 用于交叉校验：每个物块必须存在，且物块所属图层
     * 必须与它所在的图层小节一致（否则就是两份配置对不上，属于典型的写错但不报错）。
     *
     * @throws ConfigError
     */
    static LevelData loadLevel(const QString &file, const Config &config);

    /**
     * 便捷重载：加载 @p levelsDir 下 0 起的第 @p index 张关卡。
     *
     * 文件名按「关号 = index + 1」两位零填充，即 index 0 → `level_01.json`，
     * 与旧 `Map::loadMap(index)` 的 0 起序号一一对应。
     *
     * @throws ConfigError（含文件不存在）
     */
    static LevelData loadLevelByIndex(const QString &levelsDir, int index, const Config &config);

    /**
     * 列出 @p levelsDir 下所有 `level_NN.json`，按序号升序返回。
     *
     * 只读每个文件的顶层 `id` / `name`，**不做**完整校验（不查物块引用、不查几何），
     * 也不需要 @p Config —— 供 UI 关卡列表使用。真正能不能开局由 loadLevelByIndex()
     * 在开局时判定，所以这里刻意对坏文件宽容：读不出来的关卡跳过并 qWarning，
     * 避免一个写坏的关卡文件把整个菜单弄崩。
     *
     * 这是「新增关卡不改 C++」的关键：把新文件放进目录，列表里就会多一项。
     */
    static QVector<LevelEntry> listLevels(const QString &levelsDir);
};

} // namespace tankcity::config

#endif // TANKCITY_CONFIG_LOADER_H
