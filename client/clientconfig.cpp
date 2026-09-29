#include "clientconfig.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include <memory>

namespace client {

QString assetsRoot()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../assets"));
}

const tankcity::config::Config *config()
{
    // 函数内 static：只加载一次，且魔法静态本身是线程安全的。
    static const std::unique_ptr<tankcity::config::Config> cached = [] {
        const QString dir = QDir(assetsRoot()).filePath(QStringLiteral("config"));
        try {
            return std::make_unique<tankcity::config::Config>(
                tankcity::config::ConfigLoader::loadFromDirectory(dir));
        } catch (const tankcity::config::ConfigError &e) {
            qCritical() << "客户端配置加载失败（" << dir << "）：" << e.what();
            return std::unique_ptr<tankcity::config::Config>();
        }
    }();
    return cached.get();
}

tankcity::config::TankStats playerStats()
{
    const tankcity::config::Config *cfg = config();
    if (!cfg)
        return {};

    try {
        return tankcity::config::resolveTankStats(*cfg, QStringLiteral("player"));
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "客户端解析玩家数值失败：" << e.what();
        return {};
    }
}

tankcity::config::TankStats enemyStats(int difficulty)
{
    const tankcity::config::Config *cfg = config();
    if (!cfg)
        return {};

    if (difficulty < 0 || difficulty >= cfg->difficulties.size()) {
        qCritical() << "客户端难度档" << difficulty << "越界，配置里只有"
                    << cfg->difficulties.size() << "档";
        return {};
    }

    try {
        return tankcity::config::resolveEnemyStats(*cfg, cfg->difficulties.at(difficulty));
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "客户端解析敌人数值失败：" << e.what();
        return {};
    }
}

QVector<tankcity::config::LevelEntry> levels()
{
    return tankcity::config::ConfigLoader::listLevels(
        QDir(assetsRoot()).filePath(QStringLiteral("levels")));
}

} // namespace client
