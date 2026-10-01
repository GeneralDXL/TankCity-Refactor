#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include "core/GameLoop.h"   // engine/core：固定步长循环（kTickMs 的唯一来源）
#include <QVector>
#include <QSet>
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "map.h"
#include "config/ConfigLoader.h"
#include "config/TankStats.h"
#include <QSharedPointer>

// 说明：下面的道具数值目前仍是代码里的固定值，尚未接线到 assets/config/items.json
// （需要先定下 ItemType ↔ item id 的映射与权重生成，属 M3）。其中「生命恢复」一项
// 已随血量尺度的变化按新口径对齐（见 game.cpp 的 kHealthPackHeal）。
//
// 生命恢复 (HealthPack):
//     恢复满血的 30%（旧数值为 +30、上限 100）
//     上限 = 玩家满血值（配置里为 10）
//     客户端显示为绿色圆圈
// 弹药增强 (AmmoBoost):
//     减少射击冷却时间2ms
//     最小冷却时间5ms
//     客户端显示为金色圆圈
//速度提升 (SpeedBoost):
//     移动速度提升50%
//     效果持续10秒
//     客户端显示为蓝色圆圈
// 无敌状态 (Invincibility):
//     10秒内不受伤害
//     客户端表现为坦克闪烁效果
//     道具显示为紫色圆圈


enum class ItemType {
    HealthPack,     // 生命恢复
    AmmoBoost,      // 弹药增强
    SpeedBoost,     // 速度提升
    Invincibility   // 无敌
};

struct Item {
    int id;
    ItemType type;
    int x;
    int y;
    int duration;   // 效果持续时间(ms)
};
    
class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game();
    ~Game();
    void setId(int id) {id = id;}
public slots:
    void startGame(int mapIndex, int difficulty);
    void startEndlessGame(int mapIndex, int difficulty);
    void startMultiGame(int mapIndex);
    // void addBullet(Bullet* bullet); // 添加子弹到游戏
    void handlePlayerInput(int clientId, const QJsonObject &json);

    void addPlayer(int clientId);
    void removePlayer(int clientId);

    void safeStop(); // 安全停止游戏

signals:
    void broadcastData(const QByteArray &data, const int id);
    void gameTerminated(); // 新增终止信号

private slots:
    void gameLoop();
    void onWallDelete(const QJsonObject &json); // 处理墙删除事件

private:
    /**
     * 装载第 index 张关卡（0 起，对应 assets/levels/level_NN.json）。
     *
     * 配置只加载一次并缓存；关卡文件或配置有问题时返回 false 且不留下半张地图，
     * 调用方据此放弃开局 —— 旧代码遇到坏数据是静默地跑一张空地图。
     */
    bool loadLevelForIndex(int index);

    /// 首次使用时加载六份配置；失败返回 false 并已记录日志。
    bool ensureConfig();

    /**
     * 解析玩家 / 当前难度敌人的运行时数值（血量、速度、射击间隔、子弹速度与伤害）。
     *
     * 数值全部来自 entities.json + difficulty.json + game.json 的 tuning，
     * 换算规则见 shared/config/TankStats.h。失败时记录日志并返回 false，
     * 调用方应当放弃创建实体而不是用 0 值凑合。
     */
    bool resolvePlayerStats(tankcity::config::TankStats &stats);
    bool resolveCurrentEnemyStats(tankcity::config::TankStats &stats);

    void updateGame();
    void checkCollisions();
    void spawnEnemy();
    void gameOver(bool win);
    void endlessGameOver();
    void multiGameOver();
    void terminateGame(); // 提取清理逻辑到单独方法

    void broadcastGameState();
    void sendInitialState(int clientId);

    // 声明时统一使用 shared_ptr
    QMap<int, std::shared_ptr<Player>> players;
    int oneOfId;
    QList<std::shared_ptr<Enemy>> enemies;
    std::vector<std::shared_ptr<Bullet>> bullets;
    QSet<std::shared_ptr<Enemy>> enemiesToRemove; // 用于存储待删除的敌人
    
    Map *gameMap;

    /// 逻辑循环：固定 kTickMs；内部 timer 归属本对象，随 moveToThread 一起迁移
    engine::core::GameLoop gameLoop_;

    // 六份配置合一（game/layers/blocks/entities/items/difficulty），首次开局时加载
    tankcity::config::Config m_config;
    bool m_configReady = false;


    int currentDifficulty;
    int mapIndex;
    int score;
    int enemySpawnTimer;
    int enemySpawnInterval;
    int maxEnemies;

    bool gameRunning;
    bool gamePaused;

    int gameModle = 0;
    int id;
    

    int generateNum = 0;
    int overNum = 0;
    

    // 添加道具相关
    QVector<Item> items;
    int nextItemId = 1;
    int itemSpawnTimer = 0;
    const int ITEM_SPAWN_INTERVAL = 600; // 10秒 * 60帧/秒
    const int MAX_ITEMS = 5;

    void spawnItem();
    void checkItemCollisions();
    void applyItemEffect(int playerId, ItemType type);

    bool isEnd = false;
    bool gameS = false;

};

#endif
