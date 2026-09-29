/**
 * @file  legacy_level_dump.cpp
 * @brief 【临时工具 · 一次性】把旧 `Map::loadMap()` 的 10 张关卡导出为两份数据。
 *
 * 为什么必须先跑它：
 *   提交 `refactor: load levels from json` 会删除 `Map::loadMap()` 里的 switch-case，
 *   那是关卡几何的**唯一**权威来源。删掉之前必须先把数据冻结下来，否则再也没有对比基准。
 *
 * 产物（均提交入库）：
 *   1. `assets/levels/level_01..10.json`  —— 新 schema 的关卡文件（机械导出，非手工转录）
 *   2. `tests/data/legacy_levels.json`    —— 旧关卡全量快照（含边界），供回归对比测试比对
 *
 * 用它而不是手工转录的原因：case 6 / 7 / 8 / 9 的墙体坐标由表达式计算
 * （`oceanX` / `gapWidth` / `MAP_WIDTH * 0.4` …），手工抄写必然出错。
 *
 * 注意：本工具**依赖旧代码**，重构完成后无法编译 —— 提交 3 时删除。
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "config/ConfigLoader.h"
#include "map.h"
#include "wall.h"

namespace {

/// 旧 `wall.h` 的 int 类型 → 新 `blocks.json` 的字符串 id。
/// BOUNDARY 返回空串：边界不是物块，而是关卡属性。
QString blockIdOf(int legacyType)
{
    switch (legacyType) {
    case BRICK:  return QStringLiteral("brick");
    case STEEL:  return QStringLiteral("steel");
    case FOREST: return QStringLiteral("forest");
    case SEA:    return QStringLiteral("sea");
    case ICE:    return QStringLiteral("ice");
    default:     return QString();
    }
}

QString rectText(const QRect &r)
{
    return QStringLiteral("[%1, %2, %3, %4]").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

bool writeText(const QString &path, const QString &text)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "无法写入" << path << f.errorString();
        return false;
    }
    f.write(text.toUtf8());
    return true;
}

QString levelName(int index)
{
    // 前三个沿用旧注释，其余按地图特征命名（旧 case 3..9 只有零散注释）
    static const char *const kNames[10] = {
        "地图 1", "地图 2", "地图 3", "砖墙迷宫", "林间战场",
        "冰封之地", "海峡", "上下双洋", "中央密林", "环形孤岛",
    };
    return QString::fromUtf8(kNames[index]);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString root = QString::fromUtf8(TANKCITY_SOURCE_DIR);
    const QString configDir = root + QStringLiteral("/assets/config");
    const QString levelsDir = root + QStringLiteral("/assets/levels");
    const QString dataDir = root + QStringLiteral("/tests/data");

    QDir().mkpath(levelsDir);
    QDir().mkpath(dataDir);

    const tankcity::config::Config cfg =
        tankcity::config::ConfigLoader::loadFromDirectory(configDir);

    QJsonArray snapshotLevels;
    int totalWalls = 0;
    int totalRects = 0;

    for (int index = 0; index < 10; ++index) {
        Map map;
        map.loadMap(index);

        // ---- 归类：按物块所属图层分组（图层次序 = layers.json 的 renderOrder）----
        QHash<QString, QVector<QPair<QString, QRect>>> byLayer;
        QJsonArray snapshotWalls;

        for (const Wall &wall : map.getWalls()) {
            ++totalWalls;

            QJsonObject w;
            if (wall.getType() == BOUNDARY) {
                w[QStringLiteral("type")] = QStringLiteral("boundary");
            } else {
                const QString id = blockIdOf(wall.getType());
                w[QStringLiteral("type")] = id;
                const auto *block = cfg.block(id);
                Q_ASSERT(block != nullptr);
                byLayer[block->layer].append({id, wall.getRect()});
                ++totalRects;
            }
            QJsonArray rect;
            rect.append(wall.getRect().x());
            rect.append(wall.getRect().y());
            rect.append(wall.getRect().width());
            rect.append(wall.getRect().height());
            w[QStringLiteral("rect")] = rect;
            snapshotWalls.append(w);
        }

        QJsonObject snapLevel;
        snapLevel[QStringLiteral("index")] = index;
        snapLevel[QStringLiteral("walls")] = snapshotWalls;
        snapshotLevels.append(snapLevel);

        // ---- 生成 assets/levels/level_XX.json ----
        QString out;
        QTextStream ts(&out);
        ts << "{\n";
        ts << "  \"version\": 1,\n";
        ts << "  \"id\": \"level_" << QStringLiteral("%1").arg(index + 1, 2, 10, QLatin1Char('0'))
           << "\",\n";
        ts << "  \"name\": \"" << levelName(index) << "\",\n";
        ts << "  \"note\": \"由旧 server/map.cpp 的 Map::loadMap(" << index
           << ") 机械导出，未手工修改；边界由 boundary 属性生成，未出现在 rects 中。\",\n";
        ts << "  \"world\": { \"width\": " << Map::MAP_WIDTH << ", \"height\": " << Map::MAP_HEIGHT
           << " },\n";
        ts << "  \"boundary\": { \"enabled\": true, \"thickness\": "
           << cfg.game.boundaryThickness << " },\n";
        ts << "  \"layers\": [\n";

        bool firstLayer = true;
        for (const QString &layerId : cfg.layerIds()) {
            if (!byLayer.contains(layerId) || byLayer.value(layerId).isEmpty())
                continue;
            const auto rects = byLayer.value(layerId);

            if (!firstLayer)
                ts << ",\n";
            firstLayer = false;

            ts << "    {\n";
            ts << "      \"layer\": \"" << layerId << "\",\n";
            ts << "      \"format\": \"rects\",\n";
            ts << "      \"rects\": [\n";
            for (int i = 0; i < rects.size(); ++i) {
                // 对齐用的补白必须留在**引号之外**，否则 id 会带上尾随空格而校验失败
                const QString quoted = QStringLiteral("\"%1\"").arg(rects.at(i).first);
                ts << "        { \"block\": " << quoted.leftJustified(10)
                   << ", \"rect\": " << rectText(rects.at(i).second) << " }";
                ts << (i + 1 < rects.size() ? ",\n" : "\n");
            }
            ts << "      ]\n";
            ts << "    }";
        }
        ts << "\n  ]\n";
        ts << "}\n";

        // 回读校验：确保手工格式化产生的是合法 JSON（避免生成坏数据入库）
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            qCritical() << "生成的 level" << index << "JSON 非法：" << err.errorString()
                        << "偏移" << err.offset;
            return 1;
        }

        const QString path = levelsDir
            + QStringLiteral("/level_%1.json").arg(index + 1, 2, 10, QLatin1Char('0'));
        if (!writeText(path, out))
            return 1;
        qInfo() << "已写出" << path;
    }

    // ---- 冻结快照 ----
    QJsonObject snapshot;
    snapshot[QStringLiteral("version")] = 1;
    snapshot[QStringLiteral("note")] =
        QStringLiteral("由旧 server/map.cpp 的 Map::loadMap() 在删除前冻结导出，"
                       "供 M2 回归对比测试使用。请勿手工修改。");
    QJsonObject world;
    world[QStringLiteral("width")] = Map::MAP_WIDTH;
    world[QStringLiteral("height")] = Map::MAP_HEIGHT;
    snapshot[QStringLiteral("world")] = world;
    snapshot[QStringLiteral("levels")] = snapshotLevels;

    const QString snapshotPath = dataDir + QStringLiteral("/legacy_levels.json");
    if (!writeText(snapshotPath,
                   QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Indented))))
        return 1;

    qInfo() << "已写出" << snapshotPath;
    qInfo() << "合计" << totalWalls << "条墙记录，其中非边界" << totalRects << "条";
    return 0;
}
