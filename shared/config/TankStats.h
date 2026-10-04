#ifndef TANKCITY_TANK_STATS_H
#define TANKCITY_TANK_STATS_H

/**
 * @file  TankStats.h
 * @brief 把「坦克原型 + 难度覆盖 + 子弹原型 + 数值基准」解析成可直接用的运行时数值。
 *
 * 为什么放在 shared：客户端要画血条、服务端要判伤，两边必须用同一套换算。
 * 配置里的速度是相对值（`tuning.baseMoveSpeed` / `baseBulletSpeed` 的倍率），
 * 血量与伤害则是同一个尺度上的小整数 —— 例如敌人 4 血、子弹 1 伤 ⇒ 4 下打爆，
 * 等价于旧代码 100 血 ÷ 25 伤。差一个尺度，血条就会显示成 4%。
 *
 * 这里的换算结果必须与「M2 零行为变化」的目标一致，tests/config_test.cpp 的
 * `ConfigResolve.*` 把每个等价值钉死在断言里。
 */

#include "ConfigError.h"
#include "ConfigLoader.h"
#include "ConfigTypes.h"

#include <QJsonObject>
#include <QString>

namespace tankcity::config {

/**
 * 一发子弹的运行时"身份"（M4 步 2）。
 *
 * 打包成结构体是为了让**命中判定只需要一个参数**：`Map::checkBulletCollision(rect, profile)`。
 * 子弹自己持有一份（来自 entities.json 的子弹原型），于是"这是哪一发"跟着子弹走 ——
 * M4.5 的**弹药队列**（同一个弹夹混着普通/穿甲/弹性弹）正是靠这一点才能实现，
 * 那时开火方只需换一个 profile，判定方一行都不用改。
 */
struct BulletProfile {
    int damage = 1;             ///< 命中伤害
    QStringList tags;           ///< 标签（如 armorPiercing）—— 物块可用 requiredBulletTags 挑弹种
    QStringList bounceOn;       ///< 可弹的物块 id（来自 `ricochet.bounceOn`；空 = 不弹）
    int maxBounces = 0;         ///< 最多弹几次（0 = 不弹）
};

/** 坦克的运行时数值：速度已展开为像素 / 帧。 */
struct TankStats {
    int health = 0;
    double moveSpeed = 0.0;      ///< 像素 / 帧
    int shootDelayTicks = 0;     ///< 连续射击的最小间隔（帧）
    double bulletSpeed = 0.0;    ///< 像素 / 帧
    int bulletDamage = 1;
    int muzzleOffset = 0;        ///< 炮口相对车体中心的距离（像素）

    /// 默认弹种的 id 与它的标签（M4 步 1）。
    ///
    /// 标签决定「这发子弹打不打得动某物块」：钢材要求 `armorPiercing`，普通弹打上去不掉血。
    /// 子弹在开火时把它们带上，命中判定因此不必回查配置 —— 也为 M4.5 的「弹药队列」
    /// （同一个弹夹里混着不同弹种）留好了位置。
    QString bulletId;
    QStringList bulletTags;

    /// 弹射（M4 步 2，来自子弹原型的 `ricochet`）：可弹的物块 id 与最大弹数。
    /// 空列表 / 0 表示这发子弹不弹 —— 与 tags 一样，**纯配置差别**，代码里没有弹种特判。
    QStringList bulletBounceOn;
    int bulletMaxBounces = 0;

    /// 移动探测盒（像素），默认 40×40。来自 entities.json 每辆坦克的 `collisionBox`。
    /// M3 决议 D4：手感相关的数值一律只存在于 JSON 里，改数值不需要重编译 ——
    /// 在此之前 `Tank::move()` 把 40 写死在代码里，配置里的 collisionBox 是死数据。
    int collisionBoxW = 40;
    int collisionBoxH = 40;

    /// 打爆它需要命中几发（向上取整）。旧代码的等价值靠它来核对。
    int hitsToDestroy() const
    {
        return bulletDamage > 0 ? (health + bulletDamage - 1) / bulletDamage : 0;
    }

    /// 把默认弹种的属性打包给子弹（M4 步 2）。开火方一行调用，子弹自己带着走。
    BulletProfile bulletProfile() const
    {
        BulletProfile profile;
        profile.damage = bulletDamage;
        profile.tags = bulletTags;
        profile.bounceOn = bulletBounceOn;
        profile.maxBounces = bulletMaxBounces;
        return profile;
    }
};

/**
 * 应用难度档的 overrides。
 *
 * 键名合法性（必须是 TankDef 上真实存在的字段）已在加载期由 ConfigLoader 校验，
 * 这里只负责取值；未出现的字段保持原型值。
 */
TankDef applyTankOverrides(const TankDef &base, const QJsonObject &overrides);

/**
 * 解析某个坦克原型的数值。
 *
 * @throws ConfigError 原型的 `bullet` 在 bullets 中不存在 —— 加载期已校验过，
 *         这里再兜一次，因为直接构造 TankDef（不经加载器）也是合法用法。
 */
TankStats resolveTankStats(const Config &config, const TankDef &tank);

/** 便捷重载：按 id 查原型。id 不存在时抛 ConfigError。 */
TankStats resolveTankStats(const Config &config, const QString &tankId);

/** 解析一个难度档的敌人坦克数值（原型 + overrides）。 */
TankStats resolveEnemyStats(const Config &config, const DifficultyDef &difficulty);

} // namespace tankcity::config

#endif // TANKCITY_TANK_STATS_H
