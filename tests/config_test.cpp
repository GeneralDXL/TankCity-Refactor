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

namespace {

using tankcity::config::Config;
using tankcity::config::ConfigError;
using tankcity::config::ConfigLoader;
using tankcity::config::EffectDef;
using tankcity::config::ItemDef;
using tankcity::config::MovementModel;

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
    // M2 等价值：旧代码里钢材耐久 200000，实际不可破坏
    EXPECT_FALSE(steel->destructible);

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
    // 属同一地形两套数值的不一致，M2 起统一按此字段取值（有意修正）；M3 改为 0.7。
    EXPECT_DOUBLE_EQ(forest->moveSpeedFactor, 0.5);
    EXPECT_FALSE(forest->hidesTankFromEnemyAI);       // M3 启用

    const auto *ice = cfg.block(QStringLiteral("ice"));
    ASSERT_NE(ice, nullptr);
    EXPECT_EQ(ice->layer, QStringLiteral("ground"));
    EXPECT_DOUBLE_EQ(ice->moveSpeedFactor, 1.5);
    EXPECT_EQ(ice->movement.model, MovementModel::Normal);  // M3 改为 Inertial
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
    EXPECT_EQ(health->effects.at(0).params.value(QStringLiteral("amount")).toInt(), 1);
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
    tmp.patch(QStringLiteral("blocks.json"), QStringLiteral("\"requiredBulletTags\": [],"),
              QStringLiteral("\"requiredBulletTags\": [\"armorPiercing\"],"));
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
              QStringLiteral("\"type\": \"heal\", \"amount\": 1"),
              QStringLiteral("\"type\": \"heal\", \"amout\": 1"));
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
    // forest 属于 overlay 图层（blocksSight 默认 true）；去掉它的显式声明后应继承为 true。
    TempConfig tmp;
    tmp.patch(QStringLiteral("blocks.json"),
              QStringLiteral("\"blocksSight\": false,\n      \"moveSpeedFactor\": 0.5,"),
              QStringLiteral("\"moveSpeedFactor\": 0.5,"));

    const Config cfg = tmp.load();
    const auto *forest = cfg.block(QStringLiteral("forest"));
    ASSERT_NE(forest, nullptr);
    EXPECT_TRUE(forest->blocksSight);      // 继承自 overlay
    EXPECT_FALSE(forest->blocksTank);      // 同样继承自 overlay
}
