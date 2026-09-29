#include "TankStats.h"

namespace tankcity::config {

namespace {

/// 报错时用的文件名：坦克与子弹都定义在 entities.json。
const char *kEntitiesFile = "entities.json";

} // namespace

TankDef applyTankOverrides(const TankDef &base, const QJsonObject &overrides)
{
    TankDef def = base;
    if (overrides.contains(QStringLiteral("health")))
        def.health = overrides.value(QStringLiteral("health")).toInt(def.health);
    if (overrides.contains(QStringLiteral("speed")))
        def.speed = overrides.value(QStringLiteral("speed")).toDouble(def.speed);
    if (overrides.contains(QStringLiteral("shootDelayTicks")))
        def.shootDelayTicks =
            overrides.value(QStringLiteral("shootDelayTicks")).toInt(def.shootDelayTicks);
    if (overrides.contains(QStringLiteral("muzzleOffset")))
        def.muzzleOffset =
            overrides.value(QStringLiteral("muzzleOffset")).toInt(def.muzzleOffset);
    if (overrides.contains(QStringLiteral("bullet")))
        def.bullet = overrides.value(QStringLiteral("bullet")).toString(def.bullet);
    return def;
}

TankStats resolveTankStats(const Config &config, const TankDef &tank)
{
    const auto *bullet = config.bullet(tank.bullet);
    if (!bullet) {
        throw ConfigError(QLatin1String(kEntitiesFile),
                          QStringLiteral("tanks.%1.bullet").arg(tank.id),
                          QStringLiteral("未定义的子弹 \"%1\"（可用：%2）")
                              .arg(tank.bullet, config.bulletIds().join(QStringLiteral(", "))));
    }

    TankStats stats;
    stats.health = tank.health;
    stats.moveSpeed = tank.speed * config.game.tuning.baseMoveSpeed;
    stats.shootDelayTicks = tank.shootDelayTicks;
    stats.bulletSpeed = bullet->speed * config.game.tuning.baseBulletSpeed;
    stats.bulletDamage = bullet->damage;
    stats.muzzleOffset = tank.muzzleOffset;
    return stats;
}

TankStats resolveTankStats(const Config &config, const QString &tankId)
{
    const auto *tank = config.tank(tankId);
    if (!tank) {
        throw ConfigError(QLatin1String(kEntitiesFile),
                          QStringLiteral("tanks.%1").arg(tankId),
                          QStringLiteral("未定义的坦克原型（可用：%1）")
                              .arg(config.tankIds().join(QStringLiteral(", "))));
    }
    return resolveTankStats(config, *tank);
}

TankStats resolveEnemyStats(const Config &config, const DifficultyDef &difficulty)
{
    const auto *proto = config.tank(difficulty.enemyTank);
    if (!proto) {
        throw ConfigError(QLatin1String(kEntitiesFile),
                          QStringLiteral("difficulties.%1.enemyTank").arg(difficulty.id),
                          QStringLiteral("未定义的坦克原型 \"%1\"（可用：%2）")
                              .arg(difficulty.enemyTank,
                                   config.tankIds().join(QStringLiteral(", "))));
    }
    return resolveTankStats(config, applyTankOverrides(*proto, difficulty.overrides));
}

} // namespace tankcity::config
