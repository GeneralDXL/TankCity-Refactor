/**
 * @file  level_test.cpp
 * @brief 关卡 schema 的加载、校验与「外置前后逐项相等」的回归测试（M2）。
 *
 * 两类断言：
 *  1. 回归 —— `assets/levels/*.json` 必须与删除前的旧 `Map::loadMap()` 全量快照
 *     (`tests/data/legacy_levels.json`) 逐条相等。这是把关卡数据搬出 C++ 的守门人：
 *     只要有一格几何对不上，重构就失去意义。
 *  2. 反向 —— 关卡文件的各类错误（未知物块、图层写错、矩形越界……）必须被拦下。
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

namespace {

using tankcity::config::BlockDef;
using tankcity::config::Config;
using tankcity::config::ConfigError;
using tankcity::config::ConfigLoader;
using tankcity::config::LayerDef;
using tankcity::config::LevelData;
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

TEST(LevelLoad, BoundaryUnionMatchesLegacyWithinOneTile)
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

        // 旧代码左侧墙多出的 10px 与底边墙重叠，所以并集应当完全一致；
        // 留一格（10x10 = 100px）容差是为了让断言只约束"玩家能到达的区域"。
        const int diff =
            pixelDiff(boundary, legacyBoundary(legacy.at(index)), level.worldWidth, level.worldHeight);
        EXPECT_LE(diff, 100) << "边界像素差异 " << diff;
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
