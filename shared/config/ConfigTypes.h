#ifndef TANKCITY_CONFIG_TYPES_H
#define TANKCITY_CONFIG_TYPES_H

/**
 * @file  ConfigTypes.h
 * @brief 配置数据模型 —— 与 assets/config/*.json 一一对应。
 *
 * 设计约定：
 *  - 纯数据，不含 Qt 依赖以外的逻辑；解析与校验在 ConfigLoader。
 *  - 字符串 id：不使用魔法数字（见 assets/config/blocks.json）。
 *  - 逻辑时长一律用「帧」（*Ticks），只有涉及真实时间的道具时长用毫秒（*Ms）。
 */

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace tankcity::config {

// ---------------------------------------------------------------------------
// game.json
// ---------------------------------------------------------------------------

/** 数值基准：让 speed = 1.0 这种"小而直观"的写法有实际含义。 */
struct Tuning {
    double baseMoveSpeed = 5.0;    ///< speed = 1.0 对应的像素 / 帧
    double baseBulletSpeed = 8.0;  ///< 子弹 speed = 1.0 对应的像素 / 帧
};

struct GameConfig {
    int version = 0;
    int tileSize = 40;
    int boundaryThickness = 10;
    int ticksPerSecond = 60;
    Tuning tuning;
};

// ---------------------------------------------------------------------------
// layers.json
// ---------------------------------------------------------------------------

struct LayerDef {
    QString id;
    bool blocksTank = false;
    bool blocksBullet = false;
    bool blocksSight = false;
    int renderOrder = 0;  ///< 由 renderOrder 数组的次序决定，越小越先绘制
};

// ---------------------------------------------------------------------------
// blocks.json
// ---------------------------------------------------------------------------

/** 移动模型：普通（输入即速度） / 惯性（冰面）。 */
enum class MovementModel {
    Normal,
    Inertial,
};

struct MovementDef {
    MovementModel model = MovementModel::Normal;
    double frictionPerTick = 1.0;      ///< 无输入时每帧速度衰减系数
    double accelScale = 1.0;           ///< 有输入时加速度相对基准速度的比例
    double reverseAccelScale = 1.0;    ///< 与当前速度夹角接近 180° 时的额外削弱
};

/** 受损表现：底图 + 分级裂纹叠加（见 docs/plans/M2-配置Schema草案.md §10-Q1）。 */
struct DamageVisualDef {
    QString base;                      ///< 底图（相对 assets/）
    QStringList crackOverlays;         ///< 裂纹层，按 thresholds 依次叠加
    QVector<double> thresholds;        ///< 血量比例阈值，降序（如 0.75 / 0.50 / 0.25）
    double tintDarken = 0.0;           ///< 血量归零前整体压暗的最大比例
};

struct BlockDef {
    QString id;
    QString layer;

    bool destructible = false;
    int maxHealth = 0;
    /** 破坏此物块所需的子弹标签；为空表示任何子弹都可破坏。 */
    QStringList requiredBulletTags;

    /** 三项碰撞开关：JSON 中未写则继承所属图层的默认值。 */
    bool blocksTank = false;
    bool blocksBullet = false;
    bool blocksSight = false;

    double moveSpeedFactor = 0.0;      ///< 0 表示不影响移动
    bool hidesTankFromEnemyAI = false; ///< 森林：进入后不被敌怪检测

    MovementDef movement;
    DamageVisualDef damageVisual;
};

// ---------------------------------------------------------------------------
// entities.json
// ---------------------------------------------------------------------------

struct RicochetDef {
    bool enabled = false;
    int maxBounces = 0;
    QStringList bounceOn;  ///< 可弹射的物块 id（如 steel）
};

struct HomingDef {
    bool enabled = false;
    double radius = 0.0;           ///< 锁定半径（像素）
    double turnRatePerTick = 0.0;  ///< 每帧转向比例
};

struct BulletDef {
    QString id;
    double speed = 1.0;   ///< 相对 tuning.baseBulletSpeed 的倍率
    int damage = 1;
    /** 穿甲等属性标签；物块用 requiredBulletTags 与之匹配。 */
    QStringList tags;
    RicochetDef ricochet;
    HomingDef homing;
};

/** 弹药栈。M2 尚未启用弹药系统，JSON 中不出现该字段（present = false）。 */
struct AmmoDef {
    bool present = false;
    QString defaultBullet;
    int capacity = 0;
    int reloadMs = 0;
};

struct TankDef {
    QString id;
    int health = 0;
    double speed = 1.0;        ///< 相对 tuning.baseMoveSpeed 的倍率
    int shootDelayTicks = 0;   ///< 连续射击的最小间隔（帧）
    int collisionBoxW = 40;
    int collisionBoxH = 40;
    int muzzleOffset = 0;
    QString bullet;            ///< 默认发射的子弹原型 id
    AmmoDef ammo;
};

// ---------------------------------------------------------------------------
// items.json
// ---------------------------------------------------------------------------

/**
 * 一条效果。type 之外的参数原样保留在 params 中 ——
 * 这样新增/调整已有 effect 类型的参数不需要改 C++。
 */
struct EffectDef {
    QString type;
    QJsonObject params;
};

struct ItemDef {
    QString id;
    double weight = 1.0;  ///< 生成权重（缺省 1.0）
    QVector<EffectDef> effects;
};

struct ItemSpawnDef {
    int intervalTicks = 600;
    int maxOnField = 5;
};

// ---------------------------------------------------------------------------
// difficulty.json
// ---------------------------------------------------------------------------

struct EnemySpawnDef {
    int intervalTicks = 0;
    int maxAlive = 0;
};

struct AiDef {
    int repathIntervalTicks = 60;
    int stuckThresholdTicks = 15;
};

/**
 * 一个难度档：以 enemyTank 为原型 + 浅覆盖。
 * overrides 的键必须是 TankDef 上真实存在的字段（加载期校验，防拼写错误静默失效）。
 */
struct DifficultyDef {
    QString id;
    QString enemyTank;
    QJsonObject overrides;
    EnemySpawnDef spawn;
    AiDef ai;
};

} // namespace tankcity::config

#endif // TANKCITY_CONFIG_TYPES_H
