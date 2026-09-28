#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QVector>
#include <QSet>
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "map.h"
#include <QSharedPointer>

class Game : public QObject
{
    Q_OBJECT

public:
    explicit Game();
    ~Game();
public slots:
    void startGame(int mapIndex, int difficulty);
    void handlePlayerInput(int clientId, const QJsonObject &json);

    void addPlayer(int clientId);
    void removePlayer(int clientId);

signals:
    void broadcastData(const QByteArray &data);

private slots:
    void gameLoop();
    void onWallDelete(const QJsonObject &json); // 处理墙删除事件

private:
    void updateGame();
    void checkCollisions();
    void spawnEnemy();
    void gameOver(bool win);

    void broadcastGameState();
    void sendInitialState(int clientId);

    QMap<int, Player*> players; // 存储所有玩家
    
    QVector<Enemy*> enemies;
    Map *gameMap;
    QTimer *gameTimer;


    int currentDifficulty;
    int mapIndex;
    int score;
    int enemySpawnTimer;
    int enemySpawnInterval;
    int maxEnemies;

    bool gameRunning;
    bool gamePaused;
    std::vector<std::shared_ptr<Bullet>> bullets;
    QSet<Enemy*> enemiesToRemove; // 用于存储待删除的敌人
};

#endif
