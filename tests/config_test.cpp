/**
 * @file  config_test.cpp
 * @brief 配置 schema 的加载与校验测试（M2）。
 *
 * 分两类：
 *  1. 正向 —— 仓库内真实的 assets/config/*.json 必须能加载出预期数值；
 *  2. 反向 —— 各类**配置错误必须被拦下**，且错误信息要指到「文件 + 字段路径」。
 *     反向用例靠"复制真实配置 + 打一处补丁"构造，避免手写整份 JSON 造成维护负担。
 */

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

#include "config/ConfigLoader.h"
#include "config/TankStats.h"

namespace {

using tankcity::config::Config;
using tankcity::config::ConfigError;
using tankcity::config::ConfigLoader;
using tankcity::config::EffectDef;
using tankcity::config::ItemDef;
using tankcity::config::MovementModel;
using tankcity::config::resolveEnemyStats;
using tankcity::config::resolveTankStats;
using tankcity::config::TankStats;

const QString kConfigDir = QString::fromUtf8(TANKCITY_CONFIG_DIR);

const char *const kFileNames[] = {"game.json",   "layers.json", "blocks.json",
                                  "entities.json", "items.json", "difficulty.json"};

QString readUtf8(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll());
}

void writeUtf8(const QString &path, const QString &text)
{
    QFile f(path);
    ASSERT_TRUE(f.open(QIODevice::WriteOnly | QIODevice::Truncate)) << path.toStdString();
    f.write(text.toUtf8());
}

/// 真实配置的一份可改副本：用于构造非法输入。
class TempConfig
{
public:
    TempConfig()
        : m_dir(std::make_unique<QTemporaryDir>())
    {
        for (const char *name : kFileNames) {
            const QString n = QString::fromLatin1(name);
            writeUtf8(m_dir->filePath(n), readUtf8(kConfigDir + QLatin1Char('/') + n));
        }
    }

    /// 把 fileName 中**第一处** from 替换成 to —— 用于精准打一处补丁。
    void patch(const QString &fileName, const QString &from, const QString &to)
    {
        const QString path = m_dir->filePath(fileName);
        QString text = readUtf8(path);
        const int idx = text.indexOf(from);
        ASSERT_GE(idx, 0) << "补丁锚点未找到：" << from.toStdString() << " @" << fileName.toStdString();
        if (idx < 0)
            return;
        text.replace(idx, from.size(), to);
        writeUtf8(path, text);
    }

    QString dir() const { return m_dir->path(); }
    QString file(const QString &name) const { return m_dir->filePath(name); }
    Config load() const { return ConfigLoader::loadFromDirectory(dir()); }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
};

/// 执行 action，断言它抛出的 ConfigError 命中指定的字段路径与原因片段。
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

/// 文件级错误（目录不存在 / JSON 语法错误）：定位信息在 file() 而非 path()。
void expectConfigFileError(const std::function<void()> &action,
                           const QString &fileFragment,
                           const QString &reasonFragment)
{
    try {
        action();
    } catch (const ConfigError &e) {
        EXPECT_TRUE(e.file().contains(fileFragment)) << "文件 = " << e.file().toStdString();
        EXPECT_TRUE(QString::fromStdString(e.what()).contains(reasonFragment))
            << "错误信息 = " << e.what();
        return;
    } catch (const std::exception &e) {
        ADD_FAILURE() << "抛出了非 ConfigError 的异常：" << e.what();
        return;
    }
    ADD_FAILURE() << "应当抛出 ConfigError，但调用正常返回了";
}

const ItemDef *findItem(const Config &cfg, const QString &id)
{
    for (const ItemDef &item : cfg.items) {
        if (item.id == id)
            return &item;
    }
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// 正向：仓库真实配置
// ---------------------------------------------------------------------------

TEST(ConfigLoad, ShippedConfigParsesWithoutError)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    EXPECT_EQ(cfg.game.version, 1);
    EXPECT_EQ(cfg.game.tileSize, 40);
    EXPECT_EQ(cfg.game.boundaryThickness, 10);
    EXPECT_EQ(cfg.game.ticksPerSecond, 60);
    EXPECT_DOUBLE_EQ(cfg.game.tuning.baseMoveSpeed, 5.0);
    EXPECT_DOUBLE_EQ(cfg.game.tuning.baseBulletSpeed, 8.0);
}

TEST(ConfigLoad, LayerRenderOrderIsTopologicallySorted)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    ASSERT_EQ(cfg.layers.size(), 5);
    EXPECT_EQ(cfg.layers.at(0).id, QStringLiteral("background"));
    EXPECT_EQ(cfg.layers.at(1).id, QStringLiteral("ground"));
    EXPECT_EQ(cfg.layers.at(2).id, QStringLiteral("blocks"));
    EXPECT_EQ(cfg.layers.at(3).id, QStringLiteral("entity"));
    EXPECT_EQ(cfg.layers.at(4).id, QStringLiteral("overlay"));

    for (int i = 0; i < cfg.layers.size(); ++i)
        EXPECT_EQ(cfg.layers.at(i).renderOrder, i);

    // 森林所在的 overlay 应当绘制在实体之上、且遮挡视线
    const auto *overlay = cfg.layer(QStringLiteral("overlay"));
    ASSERT_NE(overlay, nullptr);
    EXPECT_TRUE(overlay->blocksSight);
    EXPECT_FALSE(overlay->blocksTank);
}

TEST(ConfigLoad, BlocksMatchLegacyEquivalentValues)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    ASSERT_EQ(cfg.blocks.size(), 5);

    const auto *brick = cfg.block(QStringLiteral("brick"));
    ASSERT_NE(brick, nullptr);
    EXPECT_TRUE(brick->destructible);
    EXPECT_EQ(brick->maxHealth, 4);            // 旧 100 血 ÷ 旧 25 伤害 = 4 下
    EXPECT_TRUE(brick->requiredBulletTags.isEmpty());
    EXPECT_EQ(brick->layer, QStringLiteral("blocks"));
    EXPECT_EQ(brick->damageVisual.base, QStringLiteral("images/textures/brick.jpg"));

    const auto *steel = cfg.block(QStringLiteral("steel"));
    ASSERT_NE(steel, nullptr);
    // **M4 步 1 生效**：钢材改为「可破坏，但需穿甲弹」—— M2 α 表里写明的第二批目标值
    //（M2 的等价值曾是 destructible=false；旧代码用 200000 耐久假装不可破）。
    // 数值律：8 血 ÷ 穿甲 2 伤 = **4 下**，与砖块（4 ÷ 1）同级同速。
    EXPECT_TRUE(steel->destructible);
    EXPECT_EQ(steel->maxHealth, 8);
    EXPECT_EQ(steel->requiredBulletTags, QStringList{QStringLiteral("armorPiercing")});

    const auto *sea = cfg.block(QStringLiteral("sea"));
    ASSERT_NE(sea, nullptr);
    EXPECT_TRUE(sea->blocksTank);
    // M2 等价值：旧 checkBulletCollision() 显式跳过 SEA，子弹本来就是穿过的。
    // （初版表把它写成 true，接线时会让海洋突然变成掩体 —— 属数据错误，已修正）
    EXPECT_FALSE(sea->blocksBullet);

    const auto *forest = cfg.block(QStringLiteral("forest"));
    ASSERT_NE(forest, nullptr);
    EXPECT_EQ(forest->layer, QStringLiteral("overlay"));
    // M2 等价值：玩家侧旧值为 ×0.5。敌人侧旧代码另有 ×0.75（enemy.cpp），
    // 属同一地形两套数值的不一致，M2 起统一按此字段取值（有意修正）。
    // **M4 步 3 生效**：目标值 0.7（M2 α 表的第二批）。
    EXPECT_DOUBLE_EQ(forest->moveSpeedFactor, 0.7);
    // 森林"隐身"的**唯一机制**是挡住视线 —— 不是那个已被退役的
    // `hidesTankFromEnemyAI` 开关（它语义被 blocksSight 完全覆盖，见台账 §4C-C9）。
    EXPECT_TRUE(forest->blocksSight);

    const auto *ice = cfg.block(QStringLiteral("ice"));
    ASSERT_NE(ice, nullptr);
    EXPECT_EQ(ice->layer, QStringLiteral("ground"));
    EXPECT_DOUBLE_EQ(ice->moveSpeedFactor, 1.5);
    // **M4 步 4 生效**：冰改为惯性模型（M2 的等价值是 normal，M3 那行注释预告了这一步）
    EXPECT_EQ(ice->movement.model, MovementModel::Inertial);
    EXPECT_LT(ice->movement.frictionPerTick, 1.0);   // 松手会滑行
    EXPECT_LT(ice->movement.accelScale, 1.0);        // 转向迟钝
}

TEST(ConfigLoad, EntitiesUseTuningRelativeValues)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    const auto *bullet = cfg.bullet(QStringLiteral("bulletBasic"));
    ASSERT_NE(bullet, nullptr);
    EXPECT_DOUBLE_EQ(bullet->speed, 1.0);
    EXPECT_EQ(bullet->damage, 1);
    EXPECT_TRUE(bullet->tags.isEmpty());
    EXPECT_FALSE(bullet->ricochet.enabled);
    EXPECT_FALSE(bullet->homing.enabled);

    const auto *player = cfg.tank(QStringLiteral("player"));
    ASSERT_NE(player, nullptr);
    // 旧 100 血 ÷ 旧 10 伤害（敌人子弹打玩家）= 10 下。注意这与敌人那侧的
    // 100 ÷ 25 = 4 下不是同一个数 —— 玩家挨打次数必须按 10 换算才等价。
    EXPECT_EQ(player->health, 10);
    EXPECT_DOUBLE_EQ(player->speed, 1.0);
    EXPECT_EQ(player->shootDelayTicks, 10);
    EXPECT_EQ(player->collisionBoxW, 40);
    EXPECT_EQ(player->muzzleOffset, 25);
    EXPECT_EQ(player->bullet, QStringLiteral("bulletBasic"));
    EXPECT_FALSE(player->ammo.present);  // M2 无弹药系统
}

TEST(ConfigLoad, ItemsKeepDesignDecisionToDropInvincibility)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    EXPECT_EQ(cfg.itemSpawn.intervalTicks, 600);
    EXPECT_EQ(cfg.itemSpawn.maxOnField, 5);
    ASSERT_EQ(cfg.items.size(), 3);

    EXPECT_NE(findItem(cfg, QStringLiteral("healthPack")), nullptr);
    EXPECT_NE(findItem(cfg, QStringLiteral("ammoBoost")), nullptr);
    EXPECT_NE(findItem(cfg, QStringLiteral("speedBoost")), nullptr);
    // 设计决定：无敌道具会破坏平衡性，已移除
    EXPECT_EQ(findItem(cfg, QStringLiteral("invincibility")), nullptr);

    const ItemDef *health = findItem(cfg, QStringLiteral("healthPack"));
    ASSERT_NE(health, nullptr);
    ASSERT_EQ(health->effects.size(), 1);
    EXPECT_EQ(health->effects.at(0).type, QStringLiteral("heal"));
    // 旧代码是「+30，上限 100」，即补满 30% 血。玩家满血由 100 改为 10 之后，
    // 同比例值就是 3（见 game.cpp 的 kHealthPackHeal，两处取值必须一致）。
    EXPECT_EQ(health->effects.at(0).params.value(QStringLiteral("amount")).toInt(), 3);
}

TEST(ConfigLoad, DifficultiesReferenceTankPrototype)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    ASSERT_EQ(cfg.difficulties.size(), 3);
    EXPECT_EQ(cfg.difficulties.at(0).id, QStringLiteral("easy"));
    EXPECT_EQ(cfg.difficulties.at(1).id, QStringLiteral("normal"));
    EXPECT_EQ(cfg.difficulties.at(2).id, QStringLiteral("hard"));

    const auto *easy = cfg.difficulty(QStringLiteral("easy"));
    ASSERT_NE(easy, nullptr);
    EXPECT_EQ(easy->enemyTank, QStringLiteral("enemyBasic"));
    EXPECT_EQ(easy->spawn.intervalTicks, 150);
    EXPECT_EQ(easy->spawn.maxAlive, 3);
    EXPECT_EQ(easy->ai.repathIntervalTicks, 60);
    EXPECT_EQ(easy->ai.stuckThresholdTicks, 15);
    EXPECT_EQ(easy->overrides.value(QStringLiteral("health")).toInt(), 4);

    const auto *hard = cfg.difficulty(QStringLiteral("hard"));
    ASSERT_NE(hard, nullptr);
    EXPECT_EQ(hard->spawn.intervalTicks, 70);
    EXPECT_EQ(hard->spawn.maxAlive, 5);
}

// ---------------------------------------------------------------------------
// 反向：配置错误必须被拦下
// ---------------------------------------------------------------------------

TEST(ConfigErrors, MissingDirectoryReportsDirectory)
{
    expectConfigFileError([] { ConfigLoader::loadFromDirectory(QStringLiteral("no/such/config/dir")); },
                          QStringLiteral("no/such/config/dir"),
                          QStringLiteral("配置目录不存在"));
}

TEST(ConfigErrors, MalformedJsonReportsSyntaxError)
{
    TempConfig tmp;
    writeUtf8(tmp.file(QStringLiteral("game.json")), QStringLiteral("{ \"version\": 1, }"));
    expectConfigFileError([&tmp] { tmp.load(); }, QStringLiteral("game.json"),
                          QStringLiteral("JSON 语法错误"));
}

TEST(ConfigErrors, UnknownLayerIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("blocks.json"), QStringLiteral("\"layer\": \"blocks\","),
              QStringLiteral("\"layer\": \"block\","));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("blocks.brick.layer"),
                      QStringLiteral("未知图层"));
}

TEST(ConfigErrors, TypoInFieldNameIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("blocks.json"), QStringLiteral("\"maxHealth\": 4,"),
              QStringLiteral("\"maxHealthh\": 4,"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("blocks.brick.maxHealthh"),
                      QStringLiteral("未知字段"));
}

TEST(ConfigErrors, DestructibleWithoutHealthIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("blocks.json"),
              QStringLiteral("\"maxHealth\": 4,\n      \"requiredBulletTags\": [],"),
              QStringLiteral("\"requiredBulletTags\": [],"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("blocks.brick.maxHealth"),
                      QStringLiteral("可破坏物块必须给出正的 maxHealth"));
}

TEST(ConfigErrors, TagNoBulletProvidesIsRejected)
{
    TempConfig tmp;
    // 用一个**没有任何子弹提供**的标签。不能用 armorPiercing 了 —— M4 步 1 加了
    // bulletAP（tags:["armorPiercing"]）之后，那个标签已经有人提供，本条校验不再触发。
    tmp.patch(QStringLiteral("blocks.json"), QStringLiteral("\"requiredBulletTags\": [],"),
              QStringLiteral("\"requiredBulletTags\": [\"tagNobodyProvides\"],"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("requiredBulletTags"),
                      QStringLiteral("没有任何子弹提供"));
}

TEST(ConfigErrors, ArmorPiercingAndRicochetAreMutuallyExclusive)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("entities.json"), QStringLiteral("\"damage\": 1\n    }"),
              QStringLiteral("\"damage\": 1,\n      \"tags\": [\"armorPiercing\"],\n"
                             "      \"ricochet\": { \"enabled\": true, \"maxBounces\": 1, "
                             "\"bounceOn\": [\"steel\"] }\n    }"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("bullets.bulletBasic"),
                      QStringLiteral("穿甲与弹射互斥"));
}

TEST(ConfigErrors, HomingAndRicochetAreMutuallyExclusive)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("entities.json"), QStringLiteral("\"damage\": 1\n    }"),
              QStringLiteral("\"damage\": 1,\n"
                             "      \"ricochet\": { \"enabled\": true, \"maxBounces\": 2 }\n    }"));
    tmp.patch(QStringLiteral("entities.json"), QStringLiteral("\"damage\": 1,\n"
                                                             "      \"ricochet\""),
              QStringLiteral("\"damage\": 1,\n"
                             "      \"homing\": { \"enabled\": true, \"radius\": 300 },\n"
                             "      \"ricochet\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("bullets.bulletBasic"),
                      QStringLiteral("追踪与弹射互斥"));
}

TEST(ConfigErrors, UnknownEffectTypeIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("items.json"), QStringLiteral("\"type\": \"heal\""),
              QStringLiteral("\"type\": \"heel\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("items.healthPack.effects[0].type"),
                      QStringLiteral("未知效果类型"));
}

TEST(ConfigErrors, UnknownEffectParameterIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("items.json"),
              QStringLiteral("\"type\": \"heal\", \"amount\": 3"),
              QStringLiteral("\"type\": \"heal\", \"amout\": 3"));
    expectConfigError([&tmp] { tmp.load(); },
                      QStringLiteral("items.healthPack.effects[0].amout"),
                      QStringLiteral("未知字段"));
}

TEST(ConfigErrors, TankBulletMustExist)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("entities.json"), QStringLiteral("\"bullet\": \"bulletBasic\""),
              QStringLiteral("\"bullet\": \"bulletX\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("tanks.player.bullet"),
                      QStringLiteral("未定义的子弹"));
}

TEST(ConfigErrors, DifficultyMustReferenceExistingTank)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("difficulty.json"), QStringLiteral("\"enemyTank\": \"enemyBasic\""),
              QStringLiteral("\"enemyTank\": \"enemyX\""));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("difficulties.easy.enemyTank"),
                      QStringLiteral("未知坦克原型"));
}

TEST(ConfigErrors, DifficultyOverrideTypoIsRejected)
{
    TempConfig tmp;
    tmp.patch(QStringLiteral("difficulty.json"), QStringLiteral("\"shootDelayTicks\": 60"),
              QStringLiteral("\"shootDelayTick\": 60"));
    expectConfigError([&tmp] { tmp.load(); }, QStringLiteral("difficulties.easy.overrides"),
                      QStringLiteral("未知字段"));
}

// ---------------------------------------------------------------------------
// 继承：物块未声明的字段应继承所属图层
// ---------------------------------------------------------------------------

TEST(ConfigInheritance, BlockFallsBackToLayerDefaults)
{
    // 用 **sea** 而不是 forest：M4 步 3 之后 forest 的显式值（可行走、不挡弹、挡视线）
    // 与 overlay 层的默认值**完全相同**，拿它举例会退化成"无论继承与否都通过"的空断言。
    // sea 属于 `blocks` 图层（三个开关默认都是 true），而它**显式关掉了挡子弹** ——
    // 把那行删掉后，blocksBullet 应当回到图层的 true，这才真正测到"继承"。
    TempConfig tmp;
    tmp.patch(QStringLiteral("blocks.json"),
              QStringLiteral("\"blocksBullet\": false,\n      \"blocksSight\": false"),
              QStringLiteral("\"blocksSight\": false"));

    const Config cfg = tmp.load();
    const auto *sea = cfg.block(QStringLiteral("sea"));
    ASSERT_NE(sea, nullptr);
    EXPECT_TRUE(sea->blocksBullet);    // 删掉显式声明后继承自 blocks 图层
    EXPECT_TRUE(sea->blocksTank);      // 自己的显式声明
    EXPECT_FALSE(sea->blocksSight);    // 自己的显式声明（M4 步 3：海不挡视线）
}

// ---------------------------------------------------------------------------
// 数值解析：把「M2 零行为变化」的等价值钉死。
//
// 这一组是 DoD 2「数值只在 JSON 里定义」的可执行凭据 —— JSON 允许改，但一旦改出
// 与旧代码不等价的值（例如玩家满血变回 4、子弹伤害变成 25），这里立刻变红。
// 旧值来源见 docs/plans/M2-配置Schema草案.md 的 §8 D0 表。
// ---------------------------------------------------------------------------

TEST(ConfigResolve, PlayerStatsMatchLegacyRuntimeValues)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    const TankStats stats = resolveTankStats(cfg, QStringLiteral("player"));

    // 旧：health = 100，挨的是敌人子弹 10 伤 -> 10 下
    EXPECT_EQ(stats.health, 10);
    EXPECT_EQ(stats.hitsToDestroy(), 10);
    // 旧：Player 构造参数 5.0f（= speed 1.0 × baseMoveSpeed 5.0）
    EXPECT_DOUBLE_EQ(stats.moveSpeed, 5.0);
    // 旧：shootDelay = 10
    EXPECT_EQ(stats.shootDelayTicks, 10);
    // 旧：Bullet::speed = 8（= speed 1.0 × baseBulletSpeed 8.0）
    EXPECT_DOUBLE_EQ(stats.bulletSpeed, 8.0);
    // 旧：打敌人 25、打砖墙 25 -> 统一成每发 1 伤
    EXPECT_EQ(stats.bulletDamage, 1);
    // 旧：炮口偏移 25
    EXPECT_EQ(stats.muzzleOffset, 25);
}

TEST(ConfigResolve, EnemyTiersMatchLegacyRuntimeValues)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    ASSERT_EQ(cfg.difficulties.size(), 3);

    // 旧 enemy.cpp 那个三档 switch：speed / shootDelay / health
    struct Expectation {
        const char *id;
        int health;
        int hits;
        double speed;
        int delay;
    };
    const Expectation expected[] = {
        {"easy", 4, 4, 2.5, 60},
        {"normal", 6, 6, 3.0, 45},
        {"hard", 8, 8, 4.0, 30},
    };

    for (int i = 0; i < 3; ++i) {
        SCOPED_TRACE(expected[i].id);
        ASSERT_EQ(cfg.difficulties.at(i).id, QString::fromLatin1(expected[i].id));

        const TankStats stats = resolveEnemyStats(cfg, cfg.difficulties.at(i));
        // 旧：100 / 150 / 200 血，挨的是玩家子弹 25 伤 -> 4 / 6 / 8 下
        EXPECT_EQ(stats.health, expected[i].health);
        EXPECT_EQ(stats.hitsToDestroy(), expected[i].hits);
        EXPECT_DOUBLE_EQ(stats.moveSpeed, expected[i].speed);
        EXPECT_EQ(stats.shootDelayTicks, expected[i].delay);
        // 敌人子弹与玩家同源，同样是 8 像素/帧、1 伤
        EXPECT_DOUBLE_EQ(stats.bulletSpeed, 8.0);
        EXPECT_EQ(stats.bulletDamage, 1);
    }
}

TEST(ConfigResolve, PlayerAndEnemyHitCountsAreIntentionallyDifferent)
{
    // 玩家 10 下才死、敌人 4 下就被打爆 —— 这是旧代码的真实手感，也是换算时
    // 最容易被「两边都是 4」这种直觉带错的地方，所以单独钉一条。
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    const TankStats player = resolveTankStats(cfg, QStringLiteral("player"));
    const TankStats easy = resolveEnemyStats(cfg, cfg.difficulties.at(0));

    EXPECT_EQ(player.hitsToDestroy(), 10);
    EXPECT_EQ(easy.hitsToDestroy(), 4);
}

TEST(ConfigResolve, BlockBehaviourMatchesLegacyCollisionRules)
{
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);

    const auto *brick = cfg.block(QStringLiteral("brick"));
    ASSERT_NE(brick, nullptr);
    // 旧：100 血 ÷ 25 伤 = 4 下，且挡坦克、挡子弹
    EXPECT_TRUE(brick->destructible);
    EXPECT_EQ(brick->maxHealth, 4);
    EXPECT_TRUE(brick->blocksTank);
    EXPECT_TRUE(brick->blocksBullet);

    const auto *steel = cfg.block(QStringLiteral("steel"));
    ASSERT_NE(steel, nullptr);
    // M4 步 1 起钢材可破坏（需穿甲弹）；旧代码的 200000 耐久已由 maxHealth 表达。
    EXPECT_TRUE(steel->destructible);
    EXPECT_EQ(steel->maxHealth, 8);
    EXPECT_TRUE(steel->blocksTank);
    EXPECT_TRUE(steel->blocksBullet);

    const auto *sea = cfg.block(QStringLiteral("sea"));
    ASSERT_NE(sea, nullptr);
    // 旧：checkBulletCollision 跳过 SEA（子弹穿过），但挡坦克
    EXPECT_TRUE(sea->blocksTank);
    EXPECT_FALSE(sea->blocksBullet);
    // **M4 步 3 起 blocksSight 被真正接线**：海不挡视线 —— 隔海看得见、但要绕行
    EXPECT_FALSE(sea->blocksSight);

    const auto *forest = cfg.block(QStringLiteral("forest"));
    ASSERT_NE(forest, nullptr);
    // 旧：坦克可穿过，降速 ×0.5（敌人侧 0.75 的不一致已在 M2 统一）
    EXPECT_FALSE(forest->blocksTank);
    EXPECT_FALSE(forest->blocksBullet);
    // **M4 步 3 生效**：森林**挡视线**（这就是"待在林子里隐身"的全部机制，零特例），
    // 且降速目标是 0.7（M2 α 表写明的第二批；0.5 是 M2 的等价值）。
    EXPECT_TRUE(forest->blocksSight);
    EXPECT_DOUBLE_EQ(forest->moveSpeedFactor, 0.7);

    const auto *ice = cfg.block(QStringLiteral("ice"));
    ASSERT_NE(ice, nullptr);
    EXPECT_FALSE(ice->blocksTank);
    EXPECT_FALSE(ice->blocksBullet);
    EXPECT_DOUBLE_EQ(ice->moveSpeedFactor, 1.5);
}

TEST(ConfigResolve, BulletDamageWasCollapsedIntoOneValue)
{
    // 旧代码三条判伤路径各写各的：打墙 25、打玩家 10、打敌人 25。
    // 现在都取子弹自带的 damage，所以必须仍是每发 1 伤，才能让上述命中次数不变。
    const Config cfg = ConfigLoader::loadFromDirectory(kConfigDir);
    const auto *bullet = cfg.bullet(QStringLiteral("bulletBasic"));
    ASSERT_NE(bullet, nullptr);
    EXPECT_EQ(bullet->damage, 1);
    EXPECT_DOUBLE_EQ(bullet->speed, 1.0); // 相对值，展开后为 8 像素/帧
}
