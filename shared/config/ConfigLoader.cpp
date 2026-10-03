#include "ConfigLoader.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace tankcity::config {
namespace {

// ---------------------------------------------------------------------------
// 通用工具
// ---------------------------------------------------------------------------

QString childPath(const QString &parent, const QString &key)
{
    return parent.isEmpty() ? key : QStringLiteral("%1.%2").arg(parent, key);
}

QString indexPath(const QString &parent, int index)
{
    return QStringLiteral("%1[%2]").arg(parent).arg(index);
}

QString jsonTypeName(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null:   return QStringLiteral("null");
    case QJsonValue::Bool:   return QStringLiteral("布尔");
    case QJsonValue::Double: return QStringLiteral("数字");
    case QJsonValue::String: return QStringLiteral("字符串");
    case QJsonValue::Array:  return QStringLiteral("数组");
    case QJsonValue::Object: return QStringLiteral("对象");
    default:                 return QStringLiteral("未知");
    }
}

[[noreturn]] void fail(const QString &file, const QString &path, const QString &reason)
{
    throw ConfigError(file, path, reason);
}

/// 列出可用的 id，用于报错时给出提示。
QString joinIds(QStringList ids)
{
    ids.sort();
    return ids.join(QStringLiteral(" / "));
}

QJsonObject readRootObject(const QString &file)
{
    QFile f(file);
    if (!f.exists())
        fail(file, QString(), QStringLiteral("文件不存在"));
    if (!f.open(QIODevice::ReadOnly))
        fail(file, QString(), QStringLiteral("无法打开：%1").arg(f.errorString()));

    const QByteArray raw = f.readAll();
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError)
        fail(file, QString(),
             QStringLiteral("JSON 语法错误（偏移 %1）：%2").arg(err.offset).arg(err.errorString()));
    if (!doc.isObject())
        fail(file, QString(), QStringLiteral("顶层必须是 JSON 对象"));
    return doc.object();
}

void rejectUnknownKeys(const QJsonObject &o, const QStringList &allowed,
                       const QString &file, const QString &path)
{
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (!allowed.contains(it.key()))
            fail(file, childPath(path, it.key()),
                 QStringLiteral("未知字段（可用：%1）").arg(allowed.join(QStringLiteral(" / "))));
    }
}

void checkVersion(const QJsonObject &root, const QString &file)
{
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version != 1)
        fail(file, QStringLiteral("version"),
             QStringLiteral("不支持的配置版本 %1（当前支持：1）").arg(version));
}

// ------------------------- 取值：必需 -------------------------

QJsonValue requireValue(const QJsonObject &o, const QString &key,
                        const QString &file, const QString &path)
{
    const auto it = o.constFind(key);
    if (it == o.constEnd())
        fail(file, childPath(path, key), QStringLiteral("缺少必需字段"));
    return *it;
}

QJsonObject requireObject(const QJsonObject &o, const QString &key,
                          const QString &file, const QString &path)
{
    const QJsonValue v = requireValue(o, key, file, path);
    if (!v.isObject())
        fail(file, childPath(path, key),
             QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(v)));
    return v.toObject();
}

QJsonArray requireArray(const QJsonObject &o, const QString &key,
                        const QString &file, const QString &path)
{
    const QJsonValue v = requireValue(o, key, file, path);
    if (!v.isArray())
        fail(file, childPath(path, key),
             QStringLiteral("应为数组，实际为%1").arg(jsonTypeName(v)));
    return v.toArray();
}

QString requireString(const QJsonObject &o, const QString &key,
                      const QString &file, const QString &path)
{
    const QJsonValue v = requireValue(o, key, file, path);
    if (!v.isString())
        fail(file, childPath(path, key),
             QStringLiteral("应为字符串，实际为%1").arg(jsonTypeName(v)));
    return v.toString();
}

int requireInt(const QJsonObject &o, const QString &key,
               const QString &file, const QString &path)
{
    const QJsonValue v = requireValue(o, key, file, path);
    if (!v.isDouble())
        fail(file, childPath(path, key),
             QStringLiteral("应为数字，实际为%1").arg(jsonTypeName(v)));
    const double d = v.toDouble();
    if (d != static_cast<double>(static_cast<int>(d)))
        fail(file, childPath(path, key), QStringLiteral("应为整数"));
    return static_cast<int>(d);
}

double requireDouble(const QJsonObject &o, const QString &key,
                     const QString &file, const QString &path)
{
    const QJsonValue v = requireValue(o, key, file, path);
    if (!v.isDouble())
        fail(file, childPath(path, key),
             QStringLiteral("应为数字，实际为%1").arg(jsonTypeName(v)));
    return v.toDouble();
}

// ------------------------- 取值：可选 -------------------------

QJsonValue optionalValue(const QJsonObject &o, const QString &key)
{
    const auto it = o.constFind(key);
    return it == o.constEnd() ? QJsonValue(QJsonValue::Undefined) : *it;
}

QString optionalString(const QJsonObject &o, const QString &key, const QString &def,
                       const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return def;
    if (!v.isString())
        fail(file, childPath(path, key),
             QStringLiteral("应为字符串，实际为%1").arg(jsonTypeName(v)));
    return v.toString();
}

bool optionalBool(const QJsonObject &o, const QString &key, bool def,
                  const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return def;
    if (!v.isBool())
        fail(file, childPath(path, key),
             QStringLiteral("应为布尔值，实际为%1").arg(jsonTypeName(v)));
    return v.toBool();
}

int optionalInt(const QJsonObject &o, const QString &key, int def,
                const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return def;
    if (!v.isDouble())
        fail(file, childPath(path, key),
             QStringLiteral("应为数字，实际为%1").arg(jsonTypeName(v)));
    return static_cast<int>(v.toDouble());
}

double optionalDouble(const QJsonObject &o, const QString &key, double def,
                      const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return def;
    if (!v.isDouble())
        fail(file, childPath(path, key),
             QStringLiteral("应为数字，实际为%1").arg(jsonTypeName(v)));
    return v.toDouble();
}

QStringList optionalStringArray(const QJsonObject &o, const QString &key,
                                const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return {};
    if (!v.isArray())
        fail(file, childPath(path, key),
             QStringLiteral("应为字符串数组，实际为%1").arg(jsonTypeName(v)));

    QStringList result;
    const QJsonArray arr = v.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        if (!arr.at(i).isString())
            fail(file, indexPath(childPath(path, key), i),
                 QStringLiteral("数组元素应为字符串，实际为%1").arg(jsonTypeName(arr.at(i))));
        result.append(arr.at(i).toString());
    }
    return result;
}

// 某字段若存在则必须是对象；不存在返回 false。
bool optionalObject(const QJsonObject &o, const QString &key, QJsonObject &out,
                    const QString &file, const QString &path)
{
    const QJsonValue v = optionalValue(o, key);
    if (v.isUndefined())
        return false;
    if (!v.isObject())
        fail(file, childPath(path, key),
             QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(v)));
    out = v.toObject();
    return true;
}

// ---------------------------------------------------------------------------
// game.json
// ---------------------------------------------------------------------------

GameConfig parseGame(const QString &file)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("world"),
                             QStringLiteral("loop"), QStringLiteral("tuning")},
                      file, QString());

    GameConfig cfg;
    cfg.version = requireInt(root, QStringLiteral("version"), file, QString());
    if (cfg.version != 1)
        fail(file, QStringLiteral("version"), QStringLiteral("当前仅支持 version = 1"));

    const QJsonObject world = requireObject(root, QStringLiteral("world"), file, QString());
    rejectUnknownKeys(world, {QStringLiteral("tileSize"), QStringLiteral("boundaryThickness")},
                      file, QStringLiteral("world"));
    cfg.tileSize = requireInt(world, QStringLiteral("tileSize"), file, QStringLiteral("world"));
    cfg.boundaryThickness =
        requireInt(world, QStringLiteral("boundaryThickness"), file, QStringLiteral("world"));

    const QJsonObject loop = requireObject(root, QStringLiteral("loop"), file, QString());
    rejectUnknownKeys(loop, {QStringLiteral("ticksPerSecond")}, file, QStringLiteral("loop"));
    cfg.ticksPerSecond = requireInt(loop, QStringLiteral("ticksPerSecond"), file, QStringLiteral("loop"));

    const QJsonObject tuning = requireObject(root, QStringLiteral("tuning"), file, QString());
    rejectUnknownKeys(tuning, {QStringLiteral("baseMoveSpeed"), QStringLiteral("baseBulletSpeed")},
                      file, QStringLiteral("tuning"));
    cfg.tuning.baseMoveSpeed =
        requireDouble(tuning, QStringLiteral("baseMoveSpeed"), file, QStringLiteral("tuning"));
    cfg.tuning.baseBulletSpeed =
        requireDouble(tuning, QStringLiteral("baseBulletSpeed"), file, QStringLiteral("tuning"));

    if (cfg.tileSize <= 0)
        fail(file, QStringLiteral("world.tileSize"), QStringLiteral("必须为正整数"));
    if (cfg.boundaryThickness <= 0)
        fail(file, QStringLiteral("world.boundaryThickness"), QStringLiteral("必须为正整数"));
    if (cfg.ticksPerSecond <= 0)
        fail(file, QStringLiteral("loop.ticksPerSecond"), QStringLiteral("必须为正整数"));
    if (cfg.tuning.baseMoveSpeed <= 0.0)
        fail(file, QStringLiteral("tuning.baseMoveSpeed"), QStringLiteral("必须为正数"));
    if (cfg.tuning.baseBulletSpeed <= 0.0)
        fail(file, QStringLiteral("tuning.baseBulletSpeed"), QStringLiteral("必须为正数"));

    return cfg;
}

// ---------------------------------------------------------------------------
// layers.json
// ---------------------------------------------------------------------------

QVector<LayerDef> parseLayers(const QString &file)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("renderOrder"),
                             QStringLiteral("layers")},
                      file, QString());

    const QJsonArray order = requireArray(root, QStringLiteral("renderOrder"), file, QString());
    const QJsonObject defs = requireObject(root, QStringLiteral("layers"), file, QString());
    if (order.isEmpty())
        fail(file, QStringLiteral("renderOrder"), QStringLiteral("至少要声明一个图层"));

    QVector<LayerDef> result;
    QSet<QString> seen;
    for (int i = 0; i < order.size(); ++i) {
        const QString path = indexPath(QStringLiteral("renderOrder"), i);
        const QJsonValue v = order.at(i);
        if (!v.isString())
            fail(file, path, QStringLiteral("应为图层 id 字符串，实际为%1").arg(jsonTypeName(v)));

        const QString id = v.toString();
        if (seen.contains(id))
            fail(file, path, QStringLiteral("图层 \"%1\" 重复出现").arg(id));
        if (!defs.contains(id))
            fail(file, path, QStringLiteral("图层 \"%1\" 未在 layers 中定义（已定义：%2）")
                                 .arg(id, joinIds(defs.keys())));
        seen.insert(id);

        if (!defs.value(id).isObject())
            fail(file, childPath(QStringLiteral("layers"), id),
                 QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(defs.value(id))));

        const QJsonObject o = defs.value(id).toObject();
        const QString layerPath = childPath(QStringLiteral("layers"), id);
        rejectUnknownKeys(o, {QStringLiteral("blocksTank"), QStringLiteral("blocksBullet"),
                              QStringLiteral("blocksSight")},
                          file, layerPath);

        LayerDef def;
        def.id = id;
        def.renderOrder = i;  // 数组次序即绘制次序：越小越先绘制
        def.blocksTank = optionalBool(o, QStringLiteral("blocksTank"), false, file, layerPath);
        def.blocksBullet = optionalBool(o, QStringLiteral("blocksBullet"), false, file, layerPath);
        def.blocksSight = optionalBool(o, QStringLiteral("blocksSight"), false, file, layerPath);
        result.append(def);
    }

    for (auto it = defs.constBegin(); it != defs.constEnd(); ++it) {
        if (!seen.contains(it.key()))
            fail(file, childPath(QStringLiteral("layers"), it.key()),
                 QStringLiteral("已定义但未出现在 renderOrder 中，将永远不会被绘制"));
    }

    return result;
}

// ---------------------------------------------------------------------------
// blocks.json
// ---------------------------------------------------------------------------

MovementDef parseMovement(const QJsonObject &owner, const QString &ownerPath, const QString &file)
{
    MovementDef def;
    QJsonObject o;
    if (!optionalObject(owner, QStringLiteral("movement"), o, file, ownerPath))
        return def;

    const QString path = childPath(ownerPath, QStringLiteral("movement"));
    rejectUnknownKeys(o, {QStringLiteral("model"), QStringLiteral("frictionPerTick"),
                          QStringLiteral("accelScale"), QStringLiteral("reverseAccelScale")},
                      file, path);

    const QString model = optionalString(o, QStringLiteral("model"), QStringLiteral("normal"), file, path);
    if (model == QLatin1String("normal"))
        def.model = MovementModel::Normal;
    else if (model == QLatin1String("inertial"))
        def.model = MovementModel::Inertial;
    else
        fail(file, childPath(path, QStringLiteral("model")),
             QStringLiteral("未知移动模型 \"%1\"（可用：normal / inertial）").arg(model));

    def.frictionPerTick =
        optionalDouble(o, QStringLiteral("frictionPerTick"), 1.0, file, path);
    def.accelScale = optionalDouble(o, QStringLiteral("accelScale"), 1.0, file, path);
    def.reverseAccelScale =
        optionalDouble(o, QStringLiteral("reverseAccelScale"), 1.0, file, path);

    if (def.frictionPerTick < 0.0 || def.frictionPerTick > 1.0)
        fail(file, childPath(path, QStringLiteral("frictionPerTick")),
             QStringLiteral("应在 [0, 1] 区间内"));

    return def;
}

DamageVisualDef parseDamageVisual(const QJsonObject &owner, const QString &ownerPath, const QString &file)
{
    DamageVisualDef def;
    QJsonObject o;
    if (!optionalObject(owner, QStringLiteral("damageVisual"), o, file, ownerPath))
        return def;

    const QString path = childPath(ownerPath, QStringLiteral("damageVisual"));
    rejectUnknownKeys(o, {QStringLiteral("base"), QStringLiteral("crackOverlays"),
                          QStringLiteral("thresholds"), QStringLiteral("tintDarken")},
                      file, path);

    def.base = optionalString(o, QStringLiteral("base"), QString(), file, path);
    def.crackOverlays = optionalStringArray(o, QStringLiteral("crackOverlays"), file, path);
    def.tintDarken = optionalDouble(o, QStringLiteral("tintDarken"), 0.0, file, path);

    const QJsonValue tv = optionalValue(o, QStringLiteral("thresholds"));
    if (!tv.isUndefined()) {
        if (!tv.isArray())
            fail(file, childPath(path, QStringLiteral("thresholds")),
                 QStringLiteral("应为数字数组，实际为%1").arg(jsonTypeName(tv)));
        const QJsonArray arr = tv.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            if (!arr.at(i).isDouble())
                fail(file, indexPath(childPath(path, QStringLiteral("thresholds")), i),
                     QStringLiteral("应为数字，实际为%1").arg(jsonTypeName(arr.at(i))));
            def.thresholds.append(arr.at(i).toDouble());
        }
    }

    if (!def.crackOverlays.isEmpty() && def.crackOverlays.size() != def.thresholds.size())
        fail(file, path, QStringLiteral("crackOverlays 有 %1 项，thresholds 有 %2 项，必须一一对应")
                             .arg(def.crackOverlays.size()).arg(def.thresholds.size()));

    return def;
}

QHash<QString, BlockDef> parseBlocks(const QString &file, const QVector<LayerDef> &layers)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("blocks")}, file, QString());

    const QJsonObject defs = requireObject(root, QStringLiteral("blocks"), file, QString());
    if (defs.isEmpty())
        fail(file, QStringLiteral("blocks"), QStringLiteral("至少要定义一个物块"));

    QHash<QString, LayerDef> layerById;
    QStringList layerIds;
    for (const LayerDef &l : layers) {
        layerById.insert(l.id, l);
        layerIds.append(l.id);
    }

    QHash<QString, BlockDef> result;
    for (auto it = defs.constBegin(); it != defs.constEnd(); ++it) {
        const QString id = it.key();
        const QString path = childPath(QStringLiteral("blocks"), id);
        if (!it->isObject())
            fail(file, path, QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(*it)));
        const QJsonObject o = it->toObject();

        rejectUnknownKeys(o,
                          {QStringLiteral("layer"), QStringLiteral("destructible"),
                           QStringLiteral("maxHealth"), QStringLiteral("requiredBulletTags"),
                           QStringLiteral("blocksTank"), QStringLiteral("blocksBullet"),
                           QStringLiteral("blocksSight"), QStringLiteral("moveSpeedFactor"),
                           QStringLiteral("hidesTankFromEnemyAI"), QStringLiteral("movement"),
                           QStringLiteral("damageVisual")},
                          file, path);

        BlockDef def;
        def.id = id;
        def.layer = requireString(o, QStringLiteral("layer"), file, path);

        const auto layerIt = layerById.constFind(def.layer);
        if (layerIt == layerById.constEnd())
            fail(file, childPath(path, QStringLiteral("layer")),
                 QStringLiteral("未知图层 \"%1\"（可用：%2）").arg(def.layer, joinIds(layerIds)));
        const LayerDef layer = layerIt.value();

        def.destructible = optionalBool(o, QStringLiteral("destructible"), false, file, path);
        def.maxHealth = optionalInt(o, QStringLiteral("maxHealth"), 0, file, path);
        def.requiredBulletTags = optionalStringArray(o, QStringLiteral("requiredBulletTags"), file, path);

        // 未显式声明时继承所属图层
        def.blocksTank = optionalBool(o, QStringLiteral("blocksTank"), layer.blocksTank, file, path);
        def.blocksBullet = optionalBool(o, QStringLiteral("blocksBullet"), layer.blocksBullet, file, path);
        def.blocksSight = optionalBool(o, QStringLiteral("blocksSight"), layer.blocksSight, file, path);

        def.moveSpeedFactor = optionalDouble(o, QStringLiteral("moveSpeedFactor"), 0.0, file, path);
        def.hidesTankFromEnemyAI =
            optionalBool(o, QStringLiteral("hidesTankFromEnemyAI"), false, file, path);

        def.movement = parseMovement(o, path, file);
        def.damageVisual = parseDamageVisual(o, path, file);

        if (def.destructible && def.maxHealth <= 0)
            fail(file, childPath(path, QStringLiteral("maxHealth")),
                 QStringLiteral("可破坏物块必须给出正的 maxHealth"));
        if (!def.destructible && def.maxHealth != 0)
            fail(file, childPath(path, QStringLiteral("maxHealth")),
                 QStringLiteral("不可破坏物块的 maxHealth 必须为 0 或省略"));
        if (!def.destructible && !def.requiredBulletTags.isEmpty())
            fail(file, childPath(path, QStringLiteral("requiredBulletTags")),
                 QStringLiteral("不可破坏的物块不应声明所需子弹标签"));
        if (def.moveSpeedFactor < 0.0)
            fail(file, childPath(path, QStringLiteral("moveSpeedFactor")),
                 QStringLiteral("不能为负数"));

        result.insert(id, def);
    }

    return result;
}

// ---------------------------------------------------------------------------
// entities.json
// ---------------------------------------------------------------------------

void parseEntities(const QString &file, Config &out)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("bullets"),
                             QStringLiteral("tanks")},
                      file, QString());

    // ---- bullets ----
    const QJsonObject bullets = requireObject(root, QStringLiteral("bullets"), file, QString());
    if (bullets.isEmpty())
        fail(file, QStringLiteral("bullets"), QStringLiteral("至少要定义一种子弹"));

    for (auto it = bullets.constBegin(); it != bullets.constEnd(); ++it) {
        const QString id = it.key();
        const QString path = childPath(QStringLiteral("bullets"), id);
        if (!it->isObject())
            fail(file, path, QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(*it)));
        const QJsonObject o = it->toObject();

        rejectUnknownKeys(o, {QStringLiteral("speed"), QStringLiteral("damage"),
                              QStringLiteral("tags"), QStringLiteral("ricochet"),
                              QStringLiteral("homing")},
                          file, path);

        BulletDef def;
        def.id = id;
        def.speed = requireDouble(o, QStringLiteral("speed"), file, path);
        def.damage = requireInt(o, QStringLiteral("damage"), file, path);
        def.tags = optionalStringArray(o, QStringLiteral("tags"), file, path);

        if (def.speed <= 0.0)
            fail(file, childPath(path, QStringLiteral("speed")), QStringLiteral("必须为正数"));
        if (def.damage <= 0)
            fail(file, childPath(path, QStringLiteral("damage")), QStringLiteral("必须为正整数"));

        // ricochet
        QJsonObject ro;
        if (optionalObject(o, QStringLiteral("ricochet"), ro, file, path)) {
            const QString rpath = childPath(path, QStringLiteral("ricochet"));
            rejectUnknownKeys(ro, {QStringLiteral("enabled"), QStringLiteral("maxBounces"),
                                   QStringLiteral("bounceOn")},
                              file, rpath);
            def.ricochet.enabled = optionalBool(ro, QStringLiteral("enabled"), false, file, rpath);
            def.ricochet.maxBounces = optionalInt(ro, QStringLiteral("maxBounces"), 0, file, rpath);
            def.ricochet.bounceOn = optionalStringArray(ro, QStringLiteral("bounceOn"), file, rpath);

            if (def.ricochet.enabled && def.ricochet.maxBounces <= 0)
                fail(file, childPath(rpath, QStringLiteral("maxBounces")),
                     QStringLiteral("启用弹射时必须给出正的 maxBounces"));
            if (!def.ricochet.enabled && !def.ricochet.bounceOn.isEmpty())
                fail(file, childPath(rpath, QStringLiteral("bounceOn")),
                     QStringLiteral("未启用弹射时不应声明 bounceOn"));
        }

        // homing
        QJsonObject ho;
        if (optionalObject(o, QStringLiteral("homing"), ho, file, path)) {
            const QString hpath = childPath(path, QStringLiteral("homing"));
            rejectUnknownKeys(ho, {QStringLiteral("enabled"), QStringLiteral("radius"),
                                   QStringLiteral("turnRatePerTick")},
                              file, hpath);
            def.homing.enabled = optionalBool(ho, QStringLiteral("enabled"), false, file, hpath);
            def.homing.radius = optionalDouble(ho, QStringLiteral("radius"), 0.0, file, hpath);
            def.homing.turnRatePerTick =
                optionalDouble(ho, QStringLiteral("turnRatePerTick"), 0.0, file, hpath);

            if (def.homing.enabled && def.homing.radius <= 0.0)
                fail(file, childPath(hpath, QStringLiteral("radius")),
                     QStringLiteral("启用追踪时必须给出正的 radius"));
        }

        // ---- 互斥规则（设计上明确要求，防止击中钢材等场景出现两套冲突处理） ----
        if (def.tags.contains(QLatin1String("armorPiercing")) && def.ricochet.enabled)
            fail(file, path,
                 QStringLiteral("穿甲与弹射互斥：tags 含 armorPiercing 时 ricochet.enabled 必须为 false"));
        if (def.homing.enabled && def.ricochet.enabled)
            fail(file, path,
                 QStringLiteral("追踪与弹射互斥：homing.enabled 与 ricochet.enabled 不能同时为 true"));

        out.bullets.insert(id, def);
    }

    // ---- tanks ----
    const QJsonObject tanks = requireObject(root, QStringLiteral("tanks"), file, QString());
    if (tanks.isEmpty())
        fail(file, QStringLiteral("tanks"), QStringLiteral("至少要定义一种坦克"));

    for (auto it = tanks.constBegin(); it != tanks.constEnd(); ++it) {
        const QString id = it.key();
        const QString path = childPath(QStringLiteral("tanks"), id);
        if (!it->isObject())
            fail(file, path, QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(*it)));
        const QJsonObject o = it->toObject();

        rejectUnknownKeys(o, {QStringLiteral("health"), QStringLiteral("speed"),
                              QStringLiteral("shootDelayTicks"), QStringLiteral("collisionBox"),
                              QStringLiteral("muzzleOffset"), QStringLiteral("bullet"),
                              QStringLiteral("ammo")},
                          file, path);

        TankDef def;
        def.id = id;
        def.health = requireInt(o, QStringLiteral("health"), file, path);
        def.speed = requireDouble(o, QStringLiteral("speed"), file, path);
        def.shootDelayTicks = requireInt(o, QStringLiteral("shootDelayTicks"), file, path);
        def.muzzleOffset = optionalInt(o, QStringLiteral("muzzleOffset"), 0, file, path);
        def.bullet = requireString(o, QStringLiteral("bullet"), file, path);

        if (def.health <= 0)
            fail(file, childPath(path, QStringLiteral("health")), QStringLiteral("必须为正整数"));
        if (def.speed <= 0.0)
            fail(file, childPath(path, QStringLiteral("speed")), QStringLiteral("必须为正数"));
        if (def.shootDelayTicks <= 0)
            fail(file, childPath(path, QStringLiteral("shootDelayTicks")),
                 QStringLiteral("必须为正整数"));

        const QJsonArray box = requireArray(o, QStringLiteral("collisionBox"), file, path);
        if (box.size() != 2)
            fail(file, childPath(path, QStringLiteral("collisionBox")),
                 QStringLiteral("应为 [宽, 高] 两个数字"));
        def.collisionBoxW = box.at(0).toInt();
        def.collisionBoxH = box.at(1).toInt();
        if (def.collisionBoxW <= 0 || def.collisionBoxH <= 0)
            fail(file, childPath(path, QStringLiteral("collisionBox")),
                 QStringLiteral("宽高必须为正整数"));

        QJsonObject ao;
        if (optionalObject(o, QStringLiteral("ammo"), ao, file, path)) {
            const QString apath = childPath(path, QStringLiteral("ammo"));
            rejectUnknownKeys(ao, {QStringLiteral("defaultBullet"), QStringLiteral("capacity"),
                                   QStringLiteral("reloadMs")},
                              file, apath);
            def.ammo.present = true;
            def.ammo.defaultBullet =
                requireString(ao, QStringLiteral("defaultBullet"), file, apath);
            def.ammo.capacity = requireInt(ao, QStringLiteral("capacity"), file, apath);
            def.ammo.reloadMs = requireInt(ao, QStringLiteral("reloadMs"), file, apath);
            if (def.ammo.capacity <= 0)
                fail(file, childPath(apath, QStringLiteral("capacity")),
                     QStringLiteral("必须为正整数"));
            if (def.ammo.reloadMs <= 0)
                fail(file, childPath(apath, QStringLiteral("reloadMs")),
                     QStringLiteral("必须为正整数"));
        }

        out.tanks.insert(id, def);
    }
}

// ---------------------------------------------------------------------------
// items.json
// ---------------------------------------------------------------------------

/// 已知的效果类型 -> 允许的参数名（type 之外）。
QHash<QString, QStringList> knownEffectParams()
{
    return {
        {QStringLiteral("heal"),                  {QStringLiteral("amount"), QStringLiteral("maxHealth")}},
        {QStringLiteral("moveSpeedMultiplier"),    {QStringLiteral("multiplier"), QStringLiteral("durationMs")}},
        // 过渡期类型：M2 忠实外置现有行为；M3 引入弹药系统后由 reloadSpeedMultiplier 取代。
        {QStringLiteral("reduceShootDelay"),       {QStringLiteral("amount"), QStringLiteral("minShootDelay")}},
        {QStringLiteral("shield"),                 {QStringLiteral("hits"), QStringLiteral("durationMs")}},
        {QStringLiteral("refillAmmo"),             {}},
        {QStringLiteral("reloadSpeedMultiplier"),  {QStringLiteral("multiplier"), QStringLiteral("durationMs")}},
        {QStringLiteral("overrideBullet"),         {QStringLiteral("bullet"), QStringLiteral("shots"),
                                                    QStringLiteral("durationMs")}},
    };
}

void parseItems(const QString &file, Config &out)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("spawn"),
                             QStringLiteral("items")},
                      file, QString());

    const QJsonObject spawn = requireObject(root, QStringLiteral("spawn"), file, QString());
    rejectUnknownKeys(spawn, {QStringLiteral("intervalTicks"), QStringLiteral("maxOnField")},
                      file, QStringLiteral("spawn"));
    out.itemSpawn.intervalTicks =
        requireInt(spawn, QStringLiteral("intervalTicks"), file, QStringLiteral("spawn"));
    out.itemSpawn.maxOnField =
        requireInt(spawn, QStringLiteral("maxOnField"), file, QStringLiteral("spawn"));
    if (out.itemSpawn.intervalTicks <= 0)
        fail(file, QStringLiteral("spawn.intervalTicks"), QStringLiteral("必须为正整数"));
    if (out.itemSpawn.maxOnField <= 0)
        fail(file, QStringLiteral("spawn.maxOnField"), QStringLiteral("必须为正整数"));

    const QJsonObject items = requireObject(root, QStringLiteral("items"), file, QString());
    if (items.isEmpty())
        fail(file, QStringLiteral("items"), QStringLiteral("至少要定义一种道具"));

    const QHash<QString, QStringList> effectParams = knownEffectParams();

    for (auto it = items.constBegin(); it != items.constEnd(); ++it) {
        const QString id = it.key();
        const QString path = childPath(QStringLiteral("items"), id);
        if (!it->isObject())
            fail(file, path, QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(*it)));
        const QJsonObject o = it->toObject();

        rejectUnknownKeys(o, {QStringLiteral("weight"), QStringLiteral("effects")}, file, path);

        ItemDef def;
        def.id = id;
        def.weight = optionalDouble(o, QStringLiteral("weight"), 1.0, file, path);
        if (def.weight < 0.0)
            fail(file, childPath(path, QStringLiteral("weight")), QStringLiteral("不能为负数"));

        const QJsonArray effects = requireArray(o, QStringLiteral("effects"), file, path);
        if (effects.isEmpty())
            fail(file, childPath(path, QStringLiteral("effects")),
                 QStringLiteral("至少要声明一条效果"));

        for (int i = 0; i < effects.size(); ++i) {
            const QString epath = indexPath(childPath(path, QStringLiteral("effects")), i);
            if (!effects.at(i).isObject())
                fail(file, epath, QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(effects.at(i))));
            const QJsonObject eo = effects.at(i).toObject();

            const QString type = requireString(eo, QStringLiteral("type"), file, epath);
            const auto pit = effectParams.constFind(type);
            if (pit == effectParams.constEnd())
                fail(file, childPath(epath, QStringLiteral("type")),
                     QStringLiteral("未知效果类型 \"%1\"（可用：%2）")
                         .arg(type, joinIds(effectParams.keys())));

            QStringList allowed = pit.value();
            allowed.prepend(QStringLiteral("type"));
            rejectUnknownKeys(eo, allowed, file, epath);

            EffectDef effect;
            effect.type = type;
            effect.params = eo;  // 参数原样保留，新增参数不需要改 C++
            def.effects.append(effect);
        }

        out.items.append(def);
    }

    // spawn 权重至少有一个 > 0，否则永远不会刷出道具
    double totalWeight = 0.0;
    for (const ItemDef &item : out.items)
        totalWeight += item.weight;
    if (totalWeight <= 0.0)
        fail(file, QStringLiteral("items"),
             QStringLiteral("全部道具的 weight 之和为 0，将永远不会刷出道具"));
}

// ---------------------------------------------------------------------------
// difficulty.json
// ---------------------------------------------------------------------------

void parseDifficulty(const QString &file, Config &out)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("order"),
                             QStringLiteral("difficulties")},
                      file, QString());

    const QJsonArray order = requireArray(root, QStringLiteral("order"), file, QString());
    const QJsonObject defs = requireObject(root, QStringLiteral("difficulties"), file, QString());
    if (order.isEmpty())
        fail(file, QStringLiteral("order"), QStringLiteral("至少要声明一个难度"));

    const QStringList overridableKeys = {QStringLiteral("health"), QStringLiteral("speed"),
                                        QStringLiteral("shootDelayTicks"),
                                        QStringLiteral("muzzleOffset"), QStringLiteral("bullet")};

    QSet<QString> seen;
    for (int i = 0; i < order.size(); ++i) {
        const QString opath = indexPath(QStringLiteral("order"), i);
        if (!order.at(i).isString())
            fail(file, opath, QStringLiteral("应为难度 id 字符串"));
        const QString id = order.at(i).toString();
        if (seen.contains(id))
            fail(file, opath, QStringLiteral("难度 \"%1\" 重复出现").arg(id));
        if (!defs.contains(id))
            fail(file, opath, QStringLiteral("难度 \"%1\" 未在 difficulties 中定义（已定义：%2）")
                                  .arg(id, joinIds(defs.keys())));
        seen.insert(id);

        const QString path = childPath(QStringLiteral("difficulties"), id);
        if (!defs.value(id).isObject())
            fail(file, path, QStringLiteral("应为对象"));
        const QJsonObject o = defs.value(id).toObject();

        rejectUnknownKeys(o, {QStringLiteral("enemyTank"), QStringLiteral("overrides"),
                              QStringLiteral("spawn"), QStringLiteral("ai")},
                          file, path);

        DifficultyDef def;
        def.id = id;
        def.enemyTank = requireString(o, QStringLiteral("enemyTank"), file, path);
        if (!out.tanks.contains(def.enemyTank))
            fail(file, childPath(path, QStringLiteral("enemyTank")),
                 QStringLiteral("未知坦克原型 \"%1\"（可用：%2）")
                     .arg(def.enemyTank, joinIds(out.tanks.keys())));

        QJsonObject ov;
        if (optionalObject(o, QStringLiteral("overrides"), ov, file, path)) {
            const QString ovpath = childPath(path, QStringLiteral("overrides"));
            // 键名必须是 TankDef 上真实存在的字段 —— 防拼写错误静默失效
            rejectUnknownKeys(ov, overridableKeys, file, ovpath);
            if (ov.contains(QStringLiteral("bullet"))) {
                const QString bulletId = ov.value(QStringLiteral("bullet")).toString();
                if (!out.bullets.contains(bulletId))
                    fail(file, childPath(ovpath, QStringLiteral("bullet")),
                         QStringLiteral("未定义的子弹 \"%1\"（可用：%2）")
                             .arg(bulletId, joinIds(out.bullets.keys())));
            }
            def.overrides = ov;
        }

        const QJsonObject spawn = requireObject(o, QStringLiteral("spawn"), file, path);
        const QString spath = childPath(path, QStringLiteral("spawn"));
        rejectUnknownKeys(spawn, {QStringLiteral("intervalTicks"), QStringLiteral("maxAlive")},
                          file, spath);
        def.spawn.intervalTicks = requireInt(spawn, QStringLiteral("intervalTicks"), file, spath);
        def.spawn.maxAlive = requireInt(spawn, QStringLiteral("maxAlive"), file, spath);
        if (def.spawn.intervalTicks <= 0)
            fail(file, childPath(spath, QStringLiteral("intervalTicks")),
                 QStringLiteral("必须为正整数"));
        if (def.spawn.maxAlive <= 0)
            fail(file, childPath(spath, QStringLiteral("maxAlive")),
                 QStringLiteral("必须为正整数"));

        QJsonObject ai;
        if (optionalObject(o, QStringLiteral("ai"), ai, file, path)) {
            const QString aipath = childPath(path, QStringLiteral("ai"));
            rejectUnknownKeys(ai, {QStringLiteral("repathIntervalTicks"),
                                   QStringLiteral("stuckThresholdTicks")},
                              file, aipath);
            def.ai.repathIntervalTicks =
                optionalInt(ai, QStringLiteral("repathIntervalTicks"), 60, file, aipath);
            def.ai.stuckThresholdTicks =
                optionalInt(ai, QStringLiteral("stuckThresholdTicks"), 15, file, aipath);
        }

        out.difficulties.append(def);
    }

    for (auto it = defs.constBegin(); it != defs.constEnd(); ++it) {
        if (!seen.contains(it.key()))
            fail(file, childPath(QStringLiteral("difficulties"), it.key()),
                 QStringLiteral("已定义但未出现在 order 中"));
    }
}

// ---------------------------------------------------------------------------
// levels/*.json
// ---------------------------------------------------------------------------

QString rectText(const QRect &r)
{
    return QStringLiteral("[%1, %2, %3, %4]").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

/// 解析 [x, y, w, h] 形式的矩形。
QRect requireRect(const QJsonObject &o, const QString &key,
                  const QString &file, const QString &path)
{
    const QString rpath = childPath(path, key);
    const QJsonArray arr = requireArray(o, key, file, path);
    if (arr.size() != 4)
        fail(file, rpath,
             QStringLiteral("应为 [x, y, w, h] 四个整数，实际有 %1 项").arg(arr.size()));

    int v[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        if (!arr.at(i).isDouble())
            fail(file, indexPath(rpath, i),
                 QStringLiteral("应为整数，实际为%1").arg(jsonTypeName(arr.at(i))));
        v[i] = arr.at(i).toInt();
    }
    if (v[0] < 0 || v[1] < 0)
        fail(file, rpath, QStringLiteral("左上角坐标不能为负"));
    if (v[2] <= 0 || v[3] <= 0)
        fail(file, rpath, QStringLiteral("宽高必须为正整数"));
    return QRect(v[0], v[1], v[2], v[3]);
}

/// 把 `format: "grid"` 的字符网格展开为矩形（1 格 = 1 个矩形）。
///
/// 不做横向合并：合并虽然能减少物块数，却会丢掉"每格"的信息，而 M3 的 tile 渲染
/// 正需要逐格绘制。用于 M3 造关卡；M2 的旧关卡数据走 `rects`。
QVector<LevelRect> expandGrid(const QJsonObject &o, const QString &path,
                              const QString &file, int worldW, int worldH)
{
    const int cell = requireInt(o, QStringLiteral("cellSize"), file, path);
    if (cell <= 0)
        fail(file, childPath(path, QStringLiteral("cellSize")), QStringLiteral("必须为正整数"));

    QHash<QString, QString> legend;
    const QJsonObject legendObj = requireObject(o, QStringLiteral("legend"), file, path);
    for (auto it = legendObj.constBegin(); it != legendObj.constEnd(); ++it) {
        const QString keyPath = childPath(childPath(path, QStringLiteral("legend")), it.key());
        if (it.key().size() != 1)
            fail(file, keyPath, QStringLiteral("图例的键必须是单个字符"));
        if (!it->isString())
            fail(file, keyPath,
                 QStringLiteral("图例的值应为物块 id 字符串，实际为%1").arg(jsonTypeName(*it)));
        legend.insert(it.key(), it->toString());
    }

    const QJsonArray rows = requireArray(o, QStringLiteral("rows"), file, path);
    if (rows.isEmpty())
        fail(file, childPath(path, QStringLiteral("rows")), QStringLiteral("至少要有一行"));

    QVector<LevelRect> rects;
    int cols = -1;
    for (int r = 0; r < rows.size(); ++r) {
        const QString rpath = indexPath(childPath(path, QStringLiteral("rows")), r);
        if (!rows.at(r).isString())
            fail(file, rpath,
                 QStringLiteral("应为字符串（一行字符），实际为%1").arg(jsonTypeName(rows.at(r))));
        const QString row = rows.at(r).toString();
        if (cols < 0) {
            cols = row.size();
            if (cols == 0)
                fail(file, rpath, QStringLiteral("行不能为空"));
        } else if (row.size() != cols) {
            fail(file, rpath,
                 QStringLiteral("每行长度必须一致（首行 %1 个字符，本行 %2 个）")
                     .arg(cols).arg(row.size()));
        }

        for (int c = 0; c < row.size(); ++c) {
            const QString blockId = legend.value(QString(row.at(c)));
            if (blockId.isEmpty())
                continue;  // 未出现在图例中 = 空格
            LevelRect lr;
            lr.block = blockId;
            lr.rect = QRect(c * cell, r * cell, cell, cell);
            rects.append(lr);
        }
    }

    if (cols * cell > worldW || rows.size() * cell > worldH)
        fail(file, path,
             QStringLiteral("网格超出世界范围：%1x%2 格 × %3px = %4x%5，世界为 %6x%7")
                 .arg(cols).arg(rows.size()).arg(cell)
                 .arg(cols * cell).arg(rows.size() * cell).arg(worldW).arg(worldH));

    return rects;
}

LevelData parseLevel(const QString &file, const Config &config)
{
    const QJsonObject root = readRootObject(file);
    checkVersion(root, file);
    rejectUnknownKeys(root, {QStringLiteral("version"), QStringLiteral("id"), QStringLiteral("name"),
                             QStringLiteral("note"), QStringLiteral("world"),
                             QStringLiteral("boundary"), QStringLiteral("layers")},
                      file, QString());

    LevelData level;
    level.version = requireInt(root, QStringLiteral("version"), file, QString());
    level.id = requireString(root, QStringLiteral("id"), file, QString());
    level.name = optionalString(root, QStringLiteral("name"), level.id, file, QString());
    level.note = optionalString(root, QStringLiteral("note"), QString(), file, QString());

    const QJsonObject world = requireObject(root, QStringLiteral("world"), file, QString());
    rejectUnknownKeys(world, {QStringLiteral("width"), QStringLiteral("height")},
                      file, QStringLiteral("world"));
    level.worldWidth = requireInt(world, QStringLiteral("width"), file, QStringLiteral("world"));
    level.worldHeight = requireInt(world, QStringLiteral("height"), file, QStringLiteral("world"));
    if (level.worldWidth <= 0 || level.worldHeight <= 0)
        fail(file, QStringLiteral("world"), QStringLiteral("宽高必须为正整数"));

    QJsonObject boundary;
    if (optionalObject(root, QStringLiteral("boundary"), boundary, file, QString())) {
        rejectUnknownKeys(boundary, {QStringLiteral("enabled"), QStringLiteral("thickness")},
                          file, QStringLiteral("boundary"));
        level.boundaryEnabled =
            optionalBool(boundary, QStringLiteral("enabled"), true, file, QStringLiteral("boundary"));
        level.boundaryThickness =
            optionalInt(boundary, QStringLiteral("thickness"), config.game.boundaryThickness,
                        file, QStringLiteral("boundary"));
    } else {
        level.boundaryEnabled = true;
        level.boundaryThickness = config.game.boundaryThickness;
    }

    if (level.boundaryEnabled) {
        const int t = level.boundaryThickness;
        if (t <= 0)
            fail(file, QStringLiteral("boundary.thickness"),
                 QStringLiteral("启用边界时必须为正整数"));
        if (level.worldWidth <= 2 * t || level.worldHeight <= 2 * t)
            fail(file, QStringLiteral("boundary.thickness"),
                 QStringLiteral("边界厚度 %1 过大：世界仅 %2x%3，内部区域会被完全占满")
                     .arg(t).arg(level.worldWidth).arg(level.worldHeight));
    }

    // ---- layers ----
    const QJsonArray sections = requireArray(root, QStringLiteral("layers"), file, QString());
    QHash<QString, QVector<LevelRect>> byLayer;
    QSet<QString> seenLayers;

    for (int i = 0; i < sections.size(); ++i) {
        const QString path = indexPath(QStringLiteral("layers"), i);
        if (!sections.at(i).isObject())
            fail(file, path,
                 QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(sections.at(i))));
        const QJsonObject o = sections.at(i).toObject();

        const QString layerId = requireString(o, QStringLiteral("layer"), file, path);
        if (config.layer(layerId) == nullptr)
            fail(file, childPath(path, QStringLiteral("layer")),
                 QStringLiteral("未知图层 \"%1\"（可用：%2）")
                     .arg(layerId, joinIds(config.layerIds())));
        if (seenLayers.contains(layerId))
            fail(file, childPath(path, QStringLiteral("layer")),
                 QStringLiteral("图层 \"%1\" 在同一个关卡中只能出现一次").arg(layerId));
        seenLayers.insert(layerId);

        const QString format = optionalString(o, QStringLiteral("format"),
                                              QStringLiteral("rects"), file, path);
        QVector<LevelRect> rects;

        if (format == QLatin1String("rects")) {
            rejectUnknownKeys(o, {QStringLiteral("layer"), QStringLiteral("format"),
                                  QStringLiteral("rects")},
                              file, path);
            const QJsonArray arr = requireArray(o, QStringLiteral("rects"), file, path);
            for (int j = 0; j < arr.size(); ++j) {
                const QString rpath = indexPath(childPath(path, QStringLiteral("rects")), j);
                if (!arr.at(j).isObject())
                    fail(file, rpath,
                         QStringLiteral("应为对象，实际为%1").arg(jsonTypeName(arr.at(j))));
                const QJsonObject ro = arr.at(j).toObject();
                rejectUnknownKeys(ro, {QStringLiteral("block"), QStringLiteral("rect")}, file, rpath);

                LevelRect lr;
                lr.block = requireString(ro, QStringLiteral("block"), file, rpath);
                lr.rect = requireRect(ro, QStringLiteral("rect"), file, rpath);
                rects.append(lr);
            }
        } else if (format == QLatin1String("grid")) {
            rejectUnknownKeys(o, {QStringLiteral("layer"), QStringLiteral("format"),
                                  QStringLiteral("cellSize"), QStringLiteral("legend"),
                                  QStringLiteral("rows")},
                              file, path);
            rects = expandGrid(o, path, file, level.worldWidth, level.worldHeight);
        } else {
            fail(file, childPath(path, QStringLiteral("format")),
                 QStringLiteral("未知格式 \"%1\"（可用：rects / grid）").arg(format));
        }

        // ---- 逐块校验：物块存在、图层一致、矩形在世界内 ----
        for (const LevelRect &lr : rects) {
            const BlockDef *block = config.block(lr.block);
            if (block == nullptr)
                fail(file, path,
                     QStringLiteral("未知物块 \"%1\"（可用：%2）")
                         .arg(lr.block, joinIds(config.blockIds())));
            // 物块的图层由 blocks.json 决定；关卡文件把它放到别的小节 = 两份配置对不上
            if (block->layer != layerId)
                fail(file, path,
                     QStringLiteral("物块 \"%1\" 属于图层 \"%2\"，却写在图层 \"%3\" 小节中")
                         .arg(lr.block, block->layer, layerId));
            if (lr.rect.x() + lr.rect.width() > level.worldWidth
                || lr.rect.y() + lr.rect.height() > level.worldHeight)
                fail(file, path,
                     QStringLiteral("物块 \"%1\" 的矩形 %2 超出世界范围 %3x%4")
                         .arg(lr.block, rectText(lr.rect))
                         .arg(level.worldWidth).arg(level.worldHeight));
        }

        byLayer[layerId] = rects;
    }

    // 统一按 layers.json 的 renderOrder 排列（文件里的书写次序不影响结果）
    for (const LayerDef &l : config.layers) {
        const auto it = byLayer.constFind(l.id);
        if (it == byLayer.constEnd() || it->isEmpty())
            continue;
        LevelLayer layer;
        layer.layer = l.id;
        layer.rects = it.value();
        level.layers.append(layer);
    }

    return level;
}

// ---------------------------------------------------------------------------
// 交叉文件校验
// ---------------------------------------------------------------------------

void validateCrossReferences(Config &config,
                             const QString &blocksFile,
                             const QString &entitiesFile,
                             const QString &itemsFile)
{
    // 1) 坦克引用的子弹必须存在
    for (auto it = config.tanks.constBegin(); it != config.tanks.constEnd(); ++it) {
        if (!config.bullets.contains(it->bullet))
            fail(entitiesFile, childPath(childPath(QStringLiteral("tanks"), it.key()),
                                         QStringLiteral("bullet")),
                 QStringLiteral("未定义的子弹 \"%1\"（可用：%2）")
                     .arg(it->bullet, joinIds(config.bullets.keys())));
    }

    // 2) 弹药默认子弹必须存在
    for (auto it = config.tanks.constBegin(); it != config.tanks.constEnd(); ++it) {
        if (it->ammo.present && !config.bullets.contains(it->ammo.defaultBullet))
            fail(entitiesFile,
                 childPath(childPath(childPath(QStringLiteral("tanks"), it.key()),
                                     QStringLiteral("ammo")),
                           QStringLiteral("defaultBullet")),
                 QStringLiteral("未定义的子弹 \"%1\"（可用：%2）")
                     .arg(it->ammo.defaultBullet, joinIds(config.bullets.keys())));
    }

    // 3) 弹射目标必须是已定义的物块 —— **外加保留 id "boundary"（M4 步 2）**
    //
    // 边界不是物块（`想-I10`：最外周是关卡属性），所以它不在 blocks.json 里；
    // 但它的墙体 id 就是 "boundary"（`Wall::makeBoundary`），而"子弹撞边界要弹"
    // 是手稿里写明的规则（弹射仅限钢材与边界）。
    // 所以这里给它一条**显式豁免**：既保住"边界不是物块"的决议，
    // 又让 bounceOn 能引用它 —— 比往 blocks.json 里塞一个假物块干净得多。
    const QString kBoundaryId = QStringLiteral("boundary");
    for (auto it = config.bullets.constBegin(); it != config.bullets.constEnd(); ++it) {
        for (const QString &target : it->ricochet.bounceOn) {
            if (target != kBoundaryId && !config.blocks.contains(target))
                fail(entitiesFile,
                     childPath(childPath(childPath(QStringLiteral("bullets"), it.key()),
                                         QStringLiteral("ricochet")),
                               QStringLiteral("bounceOn")),
                     QStringLiteral("未知物块 \"%1\"（可用：%2）")
                         .arg(target, joinIds(config.blocks.keys())));
        }
    }

    // 4) 物块要求的子弹标签，必须至少有一种子弹能提供
    //    否则该物块在游戏里永远无法被破坏 —— 属于典型"配置写错但静默失效"
    QSet<QString> providedTags;
    for (auto it = config.bullets.constBegin(); it != config.bullets.constEnd(); ++it) {
        for (const QString &tag : it->tags)
            providedTags.insert(tag);
    }
    for (auto it = config.blocks.constBegin(); it != config.blocks.constEnd(); ++it) {
        for (const QString &tag : it->requiredBulletTags) {
            if (!providedTags.contains(tag))
                fail(blocksFile,
                     childPath(childPath(childPath(QStringLiteral("blocks"), it.key()),
                                         QStringLiteral("requiredBulletTags")),
                               tag),
                     QStringLiteral("标签 \"%1\" 没有任何子弹提供，该物块将永远无法被破坏").arg(tag));
        }
    }

    // 5) 道具里 overrideBullet 引用的子弹必须存在
    for (const ItemDef &item : config.items) {
        for (const EffectDef &effect : item.effects) {
            if (effect.type != QLatin1String("overrideBullet"))
                continue;
            const QString bulletId = effect.params.value(QStringLiteral("bullet")).toString();
            if (bulletId.isEmpty())
                fail(itemsFile,
                     childPath(childPath(QStringLiteral("items"), item.id),
                               QStringLiteral("effects")),
                     QStringLiteral("overrideBullet 缺少 bullet 参数"));
            if (!config.bullets.contains(bulletId))
                fail(itemsFile,
                     childPath(childPath(QStringLiteral("items"), item.id),
                               QStringLiteral("effects")),
                     QStringLiteral("overrideBullet 引用了未定义的子弹 \"%1\"（可用：%2）")
                         .arg(bulletId, joinIds(config.bullets.keys())));
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

const LayerDef *Config::layer(const QString &id) const
{
    for (const LayerDef &l : layers) {
        if (l.id == id)
            return &l;
    }
    return nullptr;
}

const BlockDef *Config::block(const QString &id) const
{
    const auto it = blocks.constFind(id);
    return it == blocks.constEnd() ? nullptr : &it.value();
}

const BulletDef *Config::bullet(const QString &id) const
{
    const auto it = bullets.constFind(id);
    return it == bullets.constEnd() ? nullptr : &it.value();
}

const TankDef *Config::tank(const QString &id) const
{
    const auto it = tanks.constFind(id);
    return it == tanks.constEnd() ? nullptr : &it.value();
}

const DifficultyDef *Config::difficulty(const QString &id) const
{
    for (const DifficultyDef &d : difficulties) {
        if (d.id == id)
            return &d;
    }
    return nullptr;
}

QStringList Config::layerIds() const
{
    QStringList ids;
    for (const LayerDef &l : layers)
        ids.append(l.id);
    return ids;
}

QStringList Config::blockIds() const
{
    QStringList ids = blocks.keys();
    ids.sort();
    return ids;
}

QStringList Config::bulletIds() const
{
    QStringList ids = bullets.keys();
    ids.sort();
    return ids;
}

QStringList Config::tankIds() const
{
    QStringList ids = tanks.keys();
    ids.sort();
    return ids;
}

// ---------------------------------------------------------------------------
// ConfigLoader
// ---------------------------------------------------------------------------

Config ConfigLoader::loadFiles(const QString &gameFile,
                              const QString &layersFile,
                              const QString &blocksFile,
                              const QString &entitiesFile,
                              const QString &itemsFile,
                              const QString &difficultyFile)
{
    Config config;
    config.game = parseGame(gameFile);
    config.layers = parseLayers(layersFile);
    config.blocks = parseBlocks(blocksFile, config.layers);
    parseEntities(entitiesFile, config);
    parseItems(itemsFile, config);
    parseDifficulty(difficultyFile, config);

    validateCrossReferences(config, blocksFile, entitiesFile, itemsFile);
    return config;
}

Config ConfigLoader::loadFromDirectory(const QString &directory)
{
    const QDir dir(directory);
    if (!dir.exists())
        throw ConfigError(directory, QString(), QStringLiteral("配置目录不存在"));

    return loadFiles(dir.filePath(QStringLiteral("game.json")),
                     dir.filePath(QStringLiteral("layers.json")),
                     dir.filePath(QStringLiteral("blocks.json")),
                     dir.filePath(QStringLiteral("entities.json")),
                     dir.filePath(QStringLiteral("items.json")),
                     dir.filePath(QStringLiteral("difficulty.json")));
}

LevelData ConfigLoader::loadLevel(const QString &file, const Config &config)
{
    return parseLevel(file, config);
}

LevelData ConfigLoader::loadLevelByIndex(const QString &levelsDir, int index, const Config &config)
{
    if (index < 0)
        throw ConfigError(levelsDir, QStringLiteral("index"),
                          QStringLiteral("关卡序号不能为负：%1").arg(index));

    const QDir dir(levelsDir);
    // 关号 = index + 1：index 0 -> level_01.json（与旧 Map::loadMap(index) 的 0 起序号对应）
    const QString name = QStringLiteral("level_%1.json").arg(index + 1, 2, 10, QLatin1Char('0'));
    const QString file = dir.filePath(name);
    if (!QFile::exists(file))
        throw ConfigError(file, QString(),
                          QStringLiteral("关卡文件不存在（序号 %1，约定文件名 level_NN.json）").arg(index));

    return parseLevel(file, config);
}

QVector<LevelEntry> ConfigLoader::listLevels(const QString &levelsDir)
{
    // 约定文件名 level_NN.json（关号 = 序号 + 1），与 loadLevelByIndex() 一致。
    static const QRegularExpression namePattern(QStringLiteral("^level_(\\d+)\\.json$"));

    const QDir dir(levelsDir);
    if (!dir.exists())
        return {};

    QVector<LevelEntry> entries;
    const QStringList names = dir.entryList({QStringLiteral("level_*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : names) {
        const QRegularExpressionMatch match = namePattern.match(name);
        if (!match.hasMatch())
            continue;  // 形如 level_1_backup.json 的文件不参与列表

        const int number = match.captured(1).toInt();
        if (number <= 0) {
            qWarning() << "关卡文件名不合法，已跳过：" << dir.filePath(name);
            continue;
        }

        LevelEntry entry;
        entry.index = number - 1;
        entry.id = QStringLiteral("level_%1").arg(number, 2, 10, QLatin1Char('0'));

        // 只读顶层 id / name；坏文件跳过而不是整份列表失败。
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "关卡文件读不出来，列表中已跳过：" << file.fileName();
            continue;
        }
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        file.close();
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning() << "关卡文件 JSON 解析失败，列表中已跳过：" << file.fileName()
                       << error.errorString();
            continue;
        }

        const QJsonObject root = doc.object();
        entry.id = root.value(QStringLiteral("id")).toString(entry.id);
        entry.name = root.value(QStringLiteral("name")).toString(entry.id);
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(),
              [](const LevelEntry &a, const LevelEntry &b) { return a.index < b.index; });
    return entries;
}

} // namespace tankcity::config
