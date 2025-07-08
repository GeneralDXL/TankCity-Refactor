#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QTcpSocket>      // 客户端套接字类
#include <QTcpServer>      // 服务器类
#include <QAbstractSocket> // 抽象套接字基类，提供信号和错误码
#include "painter.h"

//--------------------加
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
//--------------------加

class GameWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr);
    void startGame(int mapIndex, int difficulty, int mode);
    void connectToServer(const QString &host, quint16 port);

signals:
    void backToMenu();
    void gameFinished(int score);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;    

private slots:
    void onDisconnected();
    void onReadyRead();
    void sendKey();

private:
    void drawHud(QPainter &painter);
    void sendToServer(const QJsonObject &json);
    

    QTcpSocket *socket;

    QTimer *gameTimer;
    MapPainter *gameMap;
    QPoint mousePos;
    QSet<int> pressedKeys; // 存储按下的键

    QMap<int, std::shared_ptr<PlayerPainter>> players;
    QList<std::shared_ptr<EnemyPainter>> enemies;
    std::vector<std::shared_ptr<BulletPainter>> bullets;

    bool gameRunning;
    bool gamePaused;
    int score;
    int currentDifficulty;
    int currentMapIndex;
    int enemySpawnTimer;

    //-------------------------------------------------------加
    // 添加道具相关
    QList<std::shared_ptr<Item>> items;
    void drawItem(QPainter &painter, const Item &item);
    void setPlayerInvincible(int playerId, bool invincible);

    //-------------------------------------------------------加
};

#endif // GAMEWINDOW_H
