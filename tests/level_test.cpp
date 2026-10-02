/**
 * @file  level_test.cpp
 * @brief 关卡 schema 的加载、校验与「外置前后逐项相等」的回归测试（M2）。
 *
 * 三类断言：
 *  1. 数据回归 —— `assets/levels/*.json` 必须与删除前的旧 `Map::loadMap()` 全量快照
 *     (`tests/data/legacy_levels.json`) 逐条相等。这是把关卡数据搬出 C++ 的守门人：
 *     只要有一格几何对不上，重构就失去意义。
 *  2. 装载回归 —— 新 `Map::loadLevel()` 由 JSON 还原出的 `walls` 必须与快照同内容。
 *  3. 反向 —— 关卡文件的各类错误（未知物块、图层写错、矩形越界……）必须被拦下。
 */

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QRect>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include <functional>
#include <memory>
#include <string>

#include "config/ConfigLoader.h"
#include "map.h"

namespace {

using tankcity::config::BlockDef;
using tankcity::config::Config;
using tankcity::config::ConfigError;
using tankcity::config::ConfigLoader;
using tankcity::config::LayerDef;
using tankcity::config::LevelData;
using tankcity::config::LevelEntry;
using tankcity::config::LevelLayer;
using tankcity::config::LevelRect;

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);
const QString kLevelsDir = QString::fromUtf8(TANKCITY_LEVELS_DIR);
const QString kSnapshotPath = QString::fromUtf8(TANKCITY_LEGACY_SNAPSHOT);

/// 10 张关卡共用同一份 blocks/layers 定义，加载一次即可（ConfigLoader 是纯函数）。
const Config &shipped()
{
    static const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    return cfg;
}

QByteArray readBytes(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

QVector<QJsonObject> legacyLevels()
{
    const QByteArray raw = readBytes(kSnapshotPath);
    EXPECT_FALSE(raw.isEmpty()) << "快照缺失：" << kSnapshotPath.toStdString();
    const QJsonObject root = QJsonDocument::fromJson(raw).object();
    QVector<QJsonObject> levels;
    for (const QJsonValue &v : root.value(QStringLiteral("levels")).toArray())
        levels.append(v.toObject());
    return levels;
}

QVector<QPair<QString, QRect>> legacyBlocks(const QJsonObject &level)
{
    QVector<QPair<QString, QRect>> result;
    for (const QJsonValue &v : level.value(QStringLiteral("walls")).toArray()) {
        const QJsonObject w = v.toObject();
        const QString type = w.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("boundary"))
            continue;  // 边界是关卡属性，不参与物块比对
        const QJsonArray r = w.value(QStringLiteral("rect")).toArray();
        result.append({type, QRect(r.at(0).toInt(), r.at(1).toInt(), r.at(2).toInt(), r.at(3).toInt())});
    }
    return result;
}

QVector<QRect> legacyBoundary(const QJsonObject &level)
{
    QVector<QRect> result;
    for (const QJsonValue &v : level.value(QStringLiteral("walls")).toArray()) {
        const QJsonObject w = v.toObject();
        if (w.value(QStringLiteral("type")).toString() != QLatin1String("boundary"))
            continue;
        const QJsonArray r = w.value(QStringLiteral("rect")).toArray();
        result.append(QRect(r.at(0).toInt(), r.at(1).toInt(), r.at(2).toInt(), r.at(3).toInt()));
    }
    return result;
}

QString rectText(const QRect &r)
{
    return QStringLiteral("[%1, %2, %3, %4]").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

/**
 * 物块 id -> wall.h 的 int type。
 *
 * 这层对应关系原本由一次性导出工具的 `blockIdOf()` 建立（BRICK -> "brick" ...）。
 * 这里独立重写一遍，而不是去调 `map.cpp` 里的桥接表 —— 两边若有一处写错，测试会当场发现。
 */
int legacyTypeOfBlock(const QString &id)
{
    if (id == QLatin1String("brick"))
        return BRICK;
    if (id == QLatin1String("steel"))
        return STEEL;
    if (id == QLatin1String("forest"))
        return FOREST;
    if (id == QLatin1String("sea"))
        return SEA;
    if (id == QLatin1String("ice"))
        return ICE;
    return -1;  // 旧关卡里只会出现上述五种
}

QString wallKey(int type, const QRect &r)
{
    return QStringLiteral("%1@%2").arg(type).arg(rectText(r));
}

/**
 * 复现导出工具的分组方式：把旧快照的物块按「物块所属图层」分桶，
 * 再按 layers.json 的 renderOrder 拼平 —— 这正是 LevelData::allRects() 的定义。
 */
QVector<LevelRect> expectedRects(const QVector<QPair<QString, QRect>> &blocks)
{
    QHash<QString, QVector<LevelRect>> byLayer;
    for (const auto &b : blocks) {
        const BlockDef *def = shipped().block(b.first);
        if (def != nullptr)
            byLayer[def->layer].append(LevelRect{b.first, b.second});
    }

    QVector<LevelRect> result;
    for (const LayerDef &l : shipped().layers) {
        const auto it = byLayer.constFind(l.id);
        if (it != byLayer.constEnd())
            result += it.value();
    }
    return result;
}

/// 两组矩形的**像素集合**差异（用于边界比对：新旧只是拆分方式不同）。
int pixelDiff(const QVector<QRect> &a, const QVector<QRect> &b, int w, int h)
{
    QVector<char> ga(w * h, 0);
    QVector<char> gb(w * h, 0);
    auto fill = [w, h](QVector<char> &grid, const QVector<QRect> &rects) {
        for (const QRect &r : rects) {
            const QRect c = r.intersected(QRect(0, 0, w, h));
            for (int y = c.top(); y <= c.bottom(); ++y)
                for (int x = c.left(); x <= c.right(); ++x)
                    grid[y * w + x] = 1;
        }
    };
    fill(ga, a);
    fill(gb, b);

    int diff = 0;
    for (int i = 0; i < w * h; ++i)
        if (ga.at(i) != gb.at(i))
            ++diff;
    return diff;
}

// ---------------------------------------------------------------------------
// 关卡文件构造：真实配置 + 打一处补丁，或直接给一份最小关卡
// ---------------------------------------------------------------------------

/// 一份最小合法关卡：bloks 图层里放一个 brick。
const char *const kBaseLevel = R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 400, "height": 300 },
  "boundary": { "enabled": true, "thickness": 10 },
  "layers": [
    { "layer": "blocks", "format": "rects",
      "rects": [ { "block": "brick", "rect": [40, 40, 40, 40] } ] }
  ]
})";

class TempLevel
{
public:
    explicit TempLevel(QString json = QString::fromUtf8(kBaseLevel))
        : m_dir(std::make_unique<QTemporaryDir>())
    {
        m_path = m_dir->filePath(QStringLiteral("level_01.json"));
        write(json);
    }

    /// 把第一处 from 换成 to —— 用于精准打一处补丁。
    TempLevel &patch(const QString &from, const QString &to)
    {
        QString text = QString::fromUtf8(readBytes(m_path));
        const int idx = text.indexOf(from);
        EXPECT_GE(idx, 0) << "补丁锚点未找到：" << from.toStdString();
        if (idx >= 0)
            text.replace(idx, from.size(), to);
        write(text);
        return *this;
    }

    LevelData load() const { return ConfigLoader::loadLevel(m_path, shipped()); }
    QString dir() const { return m_dir->path(); }
    QString path() const { return m_path; }

private:
    void write(const QString &text)
    {
        QFile f(m_path);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate)) << m_path.toStdString();
        f.write(text.toUtf8());
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_path;
};

void expectConfigError(const std::function<void()> &action,
                       const QString &pathFragment,
                       const QString &reasonFragment)
{
    try {
        action();
    } catch (const ConfigError &e) {
        EXPECT_TRUE(e.path().contains(pathFragment))
            << "字段路径 = " << e.path().toStdString() << "，期望包含 " << pathFragment.toStdString();
        EXPECT_TRUE(QString::fromStdString(e.what()).contains(reasonFragment))
            << "错误信息 = " << e.what();
        return;
    } catch (const std::exception &e) {
        ADD_FAILURE() << "抛出了非 ConfigError 的异常：" << e.what();
        return;
    }
    ADD_FAILURE() << "应当抛出 ConfigError，但调用正常返回了";
}

/// 文件级错误（文件不存在）：定位信息在 file() 而非 path()。
void expectConfigFileError(const std::function<void()> &action,
                           const QString &fileFragment,
                           const QString &reasonFragment)
{
    try {
        action();
    } catch (const ConfigError &e) {
        EXPECT_TRUE(e.file().contains(fileFragment))
            << "文件 = " << e.file().toStdString() << "，期望包含 " << fileFragment.toStdString();
        EXPECT_TRUE(QString::fromStdString(e.what()).contains(reasonFragment))
            << "错误信息 = " << e.what();
        return;
    } catch (const std::exception &e) {
        ADD_FAILURE() << "抛出了非 ConfigError 的异常：" << e.what();
        return;
    }
    ADD_FAILURE() << "应当抛出 ConfigError，但调用正常返回了";
}

} // namespace

// ---------------------------------------------------------------------------
// 正向：10 张关卡与旧代码逐项相等
// ---------------------------------------------------------------------------

TEST(LevelLoad, ShippedLevelsMatchLegacySnapshotRectByRect)
{
    const QVector<QJsonObject> legacy = legacyLevels();
    ASSERT_EQ(legacy.size(), 10);

    for (int index = 0; index < legacy.size(); ++index) {
        SCOPED_TRACE("关卡序号 " + std::to_string(index));

        const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());
        EXPECT_EQ(level.version, 1);
        EXPECT_EQ(level.id, QStringLiteral("level_%1").arg(index + 1, 2, 10, QLatin1Char('0')));
        EXPECT_EQ(level.worldWidth, 1200);
        EXPECT_EQ(level.worldHeight, 900);

        const QVector<LevelRect> actual = level.allRects();
        const QVector<LevelRect> expected = expectedRects(legacyBlocks(legacy.at(index)));

        ASSERT_EQ(actual.size(), expected.size()) << "物块数量与旧代码不一致";
        for (int i = 0; i < actual.size(); ++i) {
            EXPECT_EQ(actual.at(i).block, expected.at(i).block) << "第 " << i << " 个物块 id 不同";
            EXPECT_EQ(rectText(actual.at(i).rect), rectText(expected.at(i).rect))
                << "第 " << i << " 个物块矩形不同";
        }
    }
}

TEST(LevelLoad, LayersFollowRenderOrderAndSkipEmptyOnes)
{
    const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, 0, shipped());

    ASSERT_FALSE(level.layers.isEmpty());
    int lastOrder = -1;
    for (const auto &layer : level.layers) {
        const LayerDef *def = shipped().layer(layer.layer);
        ASSERT_NE(def, nullptr) << layer.layer.toStdString();
        EXPECT_GT(def->renderOrder, lastOrder) << "关卡图层未按 renderOrder 升序";
        lastOrder = def->renderOrder;
        EXPECT_FALSE(layer.rects.isEmpty()) << "空图层不应出现在结果里";
    }

    // 地图 1 只有 blocks 图层有内容
    ASSERT_EQ(level.layers.size(), 1);
    EXPECT_EQ(level.layers.at(0).layer, QStringLiteral("blocks"));
}

TEST(LevelLoad, BoundaryIsPixelIdenticalToLegacy)
{
    const QVector<QJsonObject> legacy = legacyLevels();
    ASSERT_EQ(legacy.size(), 10);

    for (int index = 0; index < legacy.size(); ++index) {
        SCOPED_TRACE("关卡序号 " + std::to_string(index));
        const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());

        EXPECT_TRUE(level.boundaryEnabled);
        EXPECT_EQ(level.boundaryThickness, shipped().game.boundaryThickness);

        const QVector<QRect> boundary = level.boundaryRects();
        ASSERT_EQ(boundary.size(), 4) << "边界应为四边画框";

        // 新写法必须恰好铺满「四边等厚」的画框：面积 = 外框 - 内框
        const int t = level.boundaryThickness;
        const int expectedArea = level.worldWidth * level.worldHeight
            - (level.worldWidth - 2 * t) * (level.worldHeight - 2 * t);
        int area = 0;
        for (const QRect &r : boundary)
            area += r.width() * r.height();
        EXPECT_EQ(area, expectedArea);

        // 旧代码左侧墙一直延伸到世界底部、底边墙从 x = t 起步 —— 两者重叠的那块 10x10
        // 正好让并集与「四边等厚的画框」完全相同，所以这里可以要求严格零像素差。
        const int diff =
            pixelDiff(boundary, legacyBoundary(legacy.at(index)), level.worldWidth, level.worldHeight);
        EXPECT_EQ(diff, 0) << "边界像素差异 " << diff;
    }
}

TEST(LevelLoad, DisabledBoundaryProducesNoRects)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("\"thickness\": 10"), QStringLiteral("\"enabled\": false, \"thickness\": 10"));

    const LevelData level = tmp.load();
    EXPECT_FALSE(level.boundaryEnabled);
    EXPECT_TRUE(level.boundaryRects().isEmpty());
}

// ---------------------------------------------------------------------------
// grid 格式：M3 逐格字符网格，一行字符 = 一行格子
// ---------------------------------------------------------------------------

TEST(LevelGrid, EveryCharacterBecomesOneCell)
{
    TempLevel tmp(QString::fromUtf8(R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 400, "height": 300 },
  "layers": [
    { "layer": "blocks", "format": "grid", "cellSize": 40,
      "legend": { "B": "brick", "S": "steel" },
      "rows": [ "BSB",
                "S.S",
                "BSB" ] }
  ]
})"));

    const LevelData level = tmp.load();
    const QVector<LevelRect> rects = level.allRects();

    ASSERT_EQ(rects.size(), 8);  // 9 格中有一个 '.'
    // 不合并：每格一个独立矩形，顺序为逐行逐列
    EXPECT_EQ(rects.at(0).block, QStringLiteral("brick"));
    EXPECT_EQ(rectText(rects.at(0).rect), QStringLiteral("[0, 0, 40, 40]"));
    EXPECT_EQ(rects.at(1).block, QStringLiteral("steel"));
    EXPECT_EQ(rectText(rects.at(1).rect), QStringLiteral("[40, 0, 40, 40]"));
    EXPECT_EQ(rectText(rects.at(2).rect), QStringLiteral("[80, 0, 40, 40]"));
    EXPECT_EQ(rectText(rects.at(3).rect), QStringLiteral("[0, 40, 40, 40]"));  // 中间的 '.' 被跳过
    EXPECT_EQ(rects.at(3).block, QStringLiteral("steel"));
    EXPECT_EQ(rectText(rects.at(4).rect), QStringLiteral("[80, 40, 40, 40]"));
    EXPECT_EQ(rectText(rects.at(7).rect), QStringLiteral("[80, 80, 40, 40]"));
}

TEST(LevelGridErrors, RaggedRowsAreRejected)
{
    TempLevel tmp(QString::fromUtf8(R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 400, "height": 300 },
  "layers": [
    { "layer": "blocks", "format": "grid", "cellSize": 40,
      "legend": { "B": "brick" },
      "rows": [ "BB", "B" ] }
  ]
})"));

    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0].rows[1]"),
                      QStringLiteral("每行长度必须一致"));
}

TEST(LevelGridErrors, GridLargerThanWorldIsRejected)
{
    TempLevel tmp(QString::fromUtf8(R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 100, "height": 100 },
  "layers": [
    { "layer": "blocks", "format": "grid", "cellSize": 40,
      "legend": { "B": "brick" },
      "rows": [ "BBB" ] }
  ]
})"));

    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0]"),
                      QStringLiteral("网格超出世界范围"));
}

// ---------------------------------------------------------------------------
// 反向：关卡错误必须被拦下
// ---------------------------------------------------------------------------

TEST(LevelErrors, UnknownBlockIsRejected)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("\"block\": \"brick\""), QStringLiteral("\"block\": \"brik\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0]"),
                      QStringLiteral("未知物块"));
}

TEST(LevelErrors, BlockPlacedInForeignLayerIsRejected)
{
    TempLevel tmp;
    // brick 属于 blocks 图层，却写进了 ground 小节
    tmp.patch(QStringLiteral("\"layer\": \"blocks\""), QStringLiteral("\"layer\": \"ground\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0]"),
                      QStringLiteral("却写在图层"));
}

TEST(LevelErrors, RectOutsideWorldIsRejected)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("[40, 40, 40, 40]"), QStringLiteral("[380, 40, 40, 40]"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0]"),
                      QStringLiteral("超出世界范围"));
}

TEST(LevelErrors, NegativeCoordinateIsRejected)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("[40, 40, 40, 40]"), QStringLiteral("[-40, 40, 40, 40]"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("rect"),
                      QStringLiteral("不能为负"));
}

TEST(LevelErrors, UnknownFormatIsRejected)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("\"format\": \"rects\""), QStringLiteral("\"format\": \"tiles\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0].format"),
                      QStringLiteral("未知格式"));
}

TEST(LevelErrors, DuplicateLayerSectionIsRejected)
{
    TempLevel tmp(QString::fromUtf8(R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 400, "height": 300 },
  "layers": [
    { "layer": "blocks", "format": "rects",
      "rects": [ { "block": "brick", "rect": [0, 0, 40, 40] } ] },
    { "layer": "blocks", "format": "rects",
      "rects": [ { "block": "steel", "rect": [40, 0, 40, 40] } ] }
  ]
})"));

    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[1].layer"),
                      QStringLiteral("只能出现一次"));
}

TEST(LevelErrors, TypoInFieldNameIsRejected)
{
    TempLevel tmp;
    tmp.patch(QStringLiteral("\"rects\":"), QStringLiteral("\"rectz\":"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("layers[0].rectz"),
                      QStringLiteral("未知字段"));
}

TEST(LevelErrors, BoundaryThickerThanWorldIsRejected)
{
    TempLevel tmp(QString::fromUtf8(R"({
  "version": 1,
  "id": "tmp",
  "world": { "width": 400, "height": 300 },
  "boundary": { "enabled": true, "thickness": 200 },
  "layers": [
    { "layer": "blocks", "format": "rects", "rects": [] }
  ]
})"));

    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("boundary.thickness"),
                      QStringLiteral("过大"));
}

TEST(LevelErrors, MissingLevelFileIsReported)
{
    // 由 loadLevelByIndex 拼出的文件不存在时，错误要指明实际查找的路径与约定命名
    expectConfigFileError([&] { ConfigLoader::loadLevelByIndex(kLevelsDir, 99, shipped()); },
                          QStringLiteral("level_100.json"), QStringLiteral("关卡文件不存在"));
}

TEST(LevelErrors, NegativeIndexIsRejected)
{
    expectConfigError([&] { ConfigLoader::loadLevelByIndex(kLevelsDir, -1, shipped()); },
                      QStringLiteral("index"), QStringLiteral("不能为负"));
}

// ---------------------------------------------------------------------------
// 装载回归：Map::loadLevel() 还原出的 walls 必须与旧快照同内容
// ---------------------------------------------------------------------------

TEST(MapLoad, JsonDrivenMapReproducesLegacyWallSet)
{
    const QVector<QJsonObject> legacy = legacyLevels();
    ASSERT_EQ(legacy.size(), 10);

    for (int index = 0; index < legacy.size(); ++index) {
        SCOPED_TRACE("关卡序号 " + std::to_string(index));

        Map map;
        const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());
        ASSERT_TRUE(map.loadLevel(level, shipped()));

        int boundaryCount = 0;
        QVector<QString> actualBlocks;
        for (const Wall &w : map.getWalls()) {
            if (w.getType() == BOUNDARY) {
                ++boundaryCount;
                continue;
            }
            actualBlocks.append(wallKey(w.getType(), w.getRect()));
        }
        actualBlocks.sort();

        // 按集合比较：关卡文件按图层书写，旧代码里物块按 case 内 append 的顺序排列，
        // 两者的分组方式不同（这一点由 LevelLoad 用例证明几何与类型完全一致）。
        QVector<QString> expectedBlocks;
        for (const auto &b : legacyBlocks(legacy.at(index)))
            expectedBlocks.append(wallKey(legacyTypeOfBlock(b.first), b.second));
        expectedBlocks.sort();

        EXPECT_EQ(actualBlocks, expectedBlocks);
        EXPECT_EQ(boundaryCount, 4);
        EXPECT_EQ(map.getWalls().size(), expectedBlocks.size() + 4);
    }
}

// ---------------------------------------------------------------------------
// 顺序等价：外置后 walls 的排列顺序变了，但「首个命中」类算法必须看不到差别
// ---------------------------------------------------------------------------

namespace {

/// `Map::checkTankCollision` 会跳过的地形（坦克能穿过）。
bool tankSees(int type)
{
    return type != FOREST && type != ICE;
}

/// `Map::checkBulletCollision` 会跳过的地形（子弹能穿过）。
bool bulletSees(int type)
{
    return type != FOREST && type != SEA && type != ICE;
}

struct TypedRect
{
    int type;
    QRect rect;
};

/// 旧快照的墙体，**保留数组顺序**——它就是旧 `Map::loadMap()` 的 append 顺序。
QVector<TypedRect> legacyWallsOf(const QJsonObject &level)
{
    QVector<TypedRect> result;
    for (const QJsonValue &v : level.value(QStringLiteral("walls")).toArray()) {
        const QJsonObject w = v.toObject();
        const QString type = w.value(QStringLiteral("type")).toString();
        const QJsonArray r = w.value(QStringLiteral("rect")).toArray();
        result.append(TypedRect{
            type == QLatin1String("boundary") ? BOUNDARY : legacyTypeOfBlock(type),
            QRect(r.at(0).toInt(), r.at(1).toInt(), r.at(2).toInt(), r.at(3).toInt())});
    }
    return result;
}

} // namespace

/**
 * 旧代码按 case 内 append 的顺序、新代码按图层分组，两者的 walls 顺序不同。
 *
 * `checkBulletCollision` / `checkTankCollision` 都是「首个命中即返回」，顺序一变就可能
 * 改变行为（打中哪块砖、踩在哪层地形上）。本用例不去复刻这两个函数，而是直接守住使它们
 * 无法观察差异的结构性前提：
 *
 *   **凡是「重叠」且「两个类型都会被该函数看见」的墙对，相对先后必须在两种顺序下一致。**
 *
 * 为什么这条前提就够：设 u 是旧序里第一个与探针矩形相交的墙。若新序里存在一个更早的
 * 相交墙 v，则 v 在旧序里必然晚于 u（否则旧序的首个命中就不是 u），即 u、v 这一对
 * 既互相重叠（都与探针相交）、类型又都被看见，却在换序后颠倒了 —— 这正是上面禁止的情况。
 * 故新序的首个命中仍是 u。
 *
 * 反过来若不成立，就必须回去对齐装载顺序，而不是放宽这条断言。
 */
TEST(MapOrder, ReorderingIsInvisibleToFirstHitLogic)
{
    const QVector<QJsonObject> legacy = legacyLevels();
    ASSERT_EQ(legacy.size(), 10);

    for (int index = 0; index < legacy.size(); ++index) {
        SCOPED_TRACE("关卡序号 " + std::to_string(index));

        const LevelData level = ConfigLoader::loadLevelByIndex(kLevelsDir, index, shipped());

        const QVector<TypedRect> oldWalls = legacyWallsOf(legacy.at(index));
        QVector<TypedRect> newWalls;
        for (const QRect &r : level.boundaryRects())
            newWalls.append(TypedRect{BOUNDARY, r});
        for (const LevelRect &lr : level.allRects())
            newWalls.append(TypedRect{legacyTypeOfBlock(lr.block), lr.rect});

        ASSERT_EQ(oldWalls.size(), newWalls.size());

        // 1) 边界在两种顺序里都排在所有物块之前（旧代码先 append 四条边界，新代码同理）。
        //    边界的矩形拆分方式变了（像素相同），所以只比"位置",不比矩形。
        const auto firstBlockIndex = [](const QVector<TypedRect> &v) {
            for (int i = 0; i < v.size(); ++i)
                if (v.at(i).type != BOUNDARY)
                    return i;
            return static_cast<int>(v.size());
        };
        const auto lastBoundaryIndex = [](const QVector<TypedRect> &v) {
            int last = -1;
            for (int i = 0; i < v.size(); ++i)
                if (v.at(i).type == BOUNDARY)
                    last = i;
            return last;
        };
        EXPECT_LT(lastBoundaryIndex(oldWalls), firstBlockIndex(oldWalls));
        EXPECT_LT(lastBoundaryIndex(newWalls), firstBlockIndex(newWalls));

        // 2) 物块按 (类型, 矩形) 一一配对，得到"旧序下标 -> 新序下标"。
        QVector<int> newIndexOf(oldWalls.size(), -1);
        QVector<bool> matched(newWalls.size(), false);
        for (int i = 0; i < oldWalls.size(); ++i) {
            if (oldWalls.at(i).type == BOUNDARY)
                continue;
            for (int j = 0; j < newWalls.size(); ++j) {
                if (matched.at(j) || newWalls.at(j).type == BOUNDARY)
                    continue;
                if (newWalls.at(j).type == oldWalls.at(i).type
                    && newWalls.at(j).rect == oldWalls.at(i).rect) {
                    matched[j] = true;
                    newIndexOf[i] = j;
                    break;
                }
            }
            ASSERT_GE(newIndexOf.at(i), 0) << "新顺序里找不到与旧序第 " << i << " 项对应的物块";
        }

        // 3) 对每个碰撞函数各查一遍：重叠且都会被看见的墙对，相对顺序必须保持。
        for (int pass = 0; pass < 2; ++pass) {
            const bool forBullet = (pass == 1);
            for (int i = 0; i < oldWalls.size(); ++i) {
                const TypedRect &a = oldWalls.at(i);
                if (newIndexOf.at(i) < 0)  // 边界：拆分方式不同，不参与配对
                    continue;
                if (!(forBullet ? bulletSees(a.type) : tankSees(a.type)))
                    continue;

                for (int j = i + 1; j < oldWalls.size(); ++j) {
                    const TypedRect &b = oldWalls.at(j);
                    if (newIndexOf.at(j) < 0)
                        continue;
                    if (!(forBullet ? bulletSees(b.type) : tankSees(b.type)))
                        continue;
                    if (!a.rect.intersects(b.rect))
                        continue;

                    EXPECT_LT(newIndexOf.at(i), newIndexOf.at(j))
                        << (forBullet ? "子弹" : "坦克") << "：旧序第 " << i << "、" << j
                        << " 项重叠且旧序在前，新序却颠倒了 —— 首个命中会改变";
                }
            }
        }
    }
}

TEST(MapLoad, WorldSizeMismatchIsRejected)
{
    LevelData level;
    level.worldWidth = 800;
    level.worldHeight = 600;

    Map map;
    EXPECT_FALSE(map.loadLevel(level, shipped()));
    EXPECT_TRUE(map.getWalls().isEmpty());
}

TEST(MapLoad, BlockIdWithoutWallTypeLeavesEmptyMap)
{
    LevelData level;
    level.worldWidth = Map::MAP_WIDTH;
    level.worldHeight = Map::MAP_HEIGHT;
    level.boundaryEnabled = true;
    level.boundaryThickness = 10;
    level.layers.append(LevelLayer{QStringLiteral("blocks"),
                                   {LevelRect{QStringLiteral("brick"), QRect(0, 0, 40, 40)},
                                    LevelRect{QStringLiteral("lava"), QRect(40, 0, 40, 40)}}});

    Map map;
    EXPECT_FALSE(map.loadLevel(level, shipped()));
    EXPECT_TRUE(map.getWalls().isEmpty()) << "失败时不应留下半张地图";
}

// ---------------------------------------------------------------------------
// 关卡列表：菜单的数据来源（DoD 5）
//
// 旧客户端把 10 项写死在 mainwindow.cpp 里，所以「新增 level_11.json」只做对了一半
// —— 服务端按序号拼文件名已经通用，菜单里却不会多出第 11 项。
// 这组测试是那半个缺口的守门人。
// ---------------------------------------------------------------------------

namespace {

/// 把仓库里的关卡复制进临时目录，供列表测试增删文件。
class TempLevelsDir
{
public:
    TempLevelsDir()
        : m_dir(std::make_unique<QTemporaryDir>())
    {
        const QDir src(kLevelsDir);
        const QStringList names =
            src.entryList({QStringLiteral("level_*.json")}, QDir::Files, QDir::Name);
        for (const QString &name : names)
            writeFile(name, readBytes(src.filePath(name)));
    }

    void writeFile(const QString &name, const QByteArray &data) const
    {
        QFile f(m_dir->filePath(name));
        ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate)) << name.toStdString();
        f.write(data);
    }

    QString dir() const { return m_dir->path(); }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

/// 一张最小合法关卡，id / name 可按需替换。
QByteArray minimalLevel(const QString &id, const QString &name = QString())
{
    const QString nameLine = name.isEmpty()
                                 ? QString()
                                 : QStringLiteral("\n  \"name\": \"%1\",").arg(name);
    return QStringLiteral(R"({
  "version": 1,%1
  "id": "%2",
  "world": { "width": 1200, "height": 900 },
  "boundary": { "enabled": true, "thickness": 10 },
  "layers": [ { "layer": "blocks", "format": "rects",
                "rects": [ { "block": "brick", "rect": [40, 40, 40, 40] } ] } ]
})")
        .arg(nameLine, id)
        .toUtf8();
}

} // namespace

/// 仓库里现有多少张关卡。列表用例一律以它为基数 —— 这样**以后加图不必改测试**，
/// 这正是 M3 DoD 第 6 条（新图靠 JSON 即可玩）的要求：加一张图只该动数据。
int shippedLevelCount()
{
    return ConfigLoader::listLevels(kLevelsDir).size();
}

TEST(LevelList, ShippedLevelsAreListedInOrder)
{
    const QVector<LevelEntry> levels = ConfigLoader::listLevels(kLevelsDir);

    // 只要求"不低于最初的 10 张"：张数会随试玩图与正式关卡增加，写死等于每加一张就改测试
    ASSERT_GE(levels.size(), 10) << "assets/levels 下至少应有 10 张关卡（index.json 不算关卡）";
    for (int i = 0; i < levels.size(); ++i) {
        EXPECT_EQ(levels.at(i).index, i) << "第 " << i << " 项的序号必须与其位置一致";
        EXPECT_EQ(levels.at(i).id, QStringLiteral("level_%1").arg(i + 1, 2, 10, QLatin1Char('0')));
        EXPECT_FALSE(levels.at(i).name.isEmpty()) << "第 " << i << " 项没有显示名";
    }
}

/// DoD 5：新增关卡文件即出现在列表里，且序号可直接交给服务端。
TEST(LevelList, NewLevelFileAppearsWithoutCppChange)
{
    TempLevelsDir tmp;
    // 编号必须**紧接在现有之后**：序号就是文件名里的数字减一（服务端按「序号+1」拼
    // level_NN.json），所以不能跳号 —— 用 level_99 的话它的 index 会是 98，
    // 而 loadLevelByIndex(14) 依旧去找 level_15.json。也不能复用 level_11：
    // 那会覆盖复制过来的真文件，测不出「多了一项」。
    const int base = shippedLevelCount();
    const QString newId = QStringLiteral("level_%1").arg(base + 1, 2, 10, QLatin1Char('0'));
    tmp.writeFile(newId + QStringLiteral(".json"),
                  minimalLevel(newId, QStringLiteral("新增测试关")));

    const QVector<LevelEntry> levels = ConfigLoader::listLevels(tmp.dir());

    ASSERT_EQ(levels.size(), base + 1) << "多放一个关卡文件，列表就该多一项";
    EXPECT_EQ(levels.last().index, base) << "序号排在现有全部之后";
    EXPECT_EQ(levels.last().id, newId);
    EXPECT_EQ(levels.last().name, QStringLiteral("新增测试关"));

    // 序号是 UI ↔ 服务端的契约：服务端拿这个序号去拼文件名必须拼得到。
    const LevelData loaded = ConfigLoader::loadLevelByIndex(tmp.dir(), base, shipped());
    EXPECT_EQ(loaded.id, newId);
}

TEST(LevelList, NameFallsBackToIdWhenMissing)
{
    TempLevelsDir tmp;
    tmp.writeFile(QStringLiteral("level_99.json"), minimalLevel(QStringLiteral("level_99")));

    const QVector<LevelEntry> levels = ConfigLoader::listLevels(tmp.dir());

    ASSERT_EQ(levels.size(), shippedLevelCount() + 1);
    EXPECT_EQ(levels.last().name, QStringLiteral("level_99")) << "没写 name 时退回 id";
}

TEST(LevelList, BrokenFileIsSkippedInsteadOfFailing)
{
    TempLevelsDir tmp;
    // 同样避开仓库里已有的名字，否则会把复制过来的好文件也写坏
    tmp.writeFile(QStringLiteral("level_99.json"), QByteArray("{ 这不是 JSON"));

    // 列表是菜单的入口：一张关卡写坏不该把整个菜单拖死（可玩性由开局时的加载兜底）。
    EXPECT_EQ(ConfigLoader::listLevels(tmp.dir()).size(), shippedLevelCount());
}

TEST(LevelList, OnlyLevelFilesAreListed)
{
    TempLevelsDir tmp;
    tmp.writeFile(QStringLiteral("level_10_backup.json"), QByteArray("{}"));
    tmp.writeFile(QStringLiteral("readme.txt"), QByteArray("hi"));

    EXPECT_EQ(ConfigLoader::listLevels(tmp.dir()).size(), shippedLevelCount())
        << "只认 level_NN.json：备份与说明文件不该出现在菜单里";
    EXPECT_TRUE(ConfigLoader::listLevels(QStringLiteral("no/such/levels/dir")).isEmpty())
        << "目录不存在时返回空列表，不抛异常";
}
