#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QTcpSocket>
#include "asset/AssetPaths.h"
#include "input/InputTracker.h"
#include "net/MessageFramer.h"
#include "render/painter.h"
#include "render/TextureCache.h"

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
    /// 收包分帧缓冲（client/net）：把 TCP 字节流切成一条条完整消息
    client::net::MessageFramer frameBuffer_;

    QTimer *gameTimer;
    MapPainter *gameMap;
    /// 输入状态容器（engine/input）：按键集合 + 指针位置 + 边沿触发的主键。
    /// 容器不认识「哪个键算什么」——那张键位表在 sendKey() 里（属客户端的事）。
    engine::input::InputTracker inputTracker_;

    QMap<int, std::shared_ptr<PlayerPainter>> players;

    /// 地图贴图缓存（每个文件只加载一次）。
    /// 资源根由 engine/asset 按**可执行文件目录**解析 —— 从任何工作目录启动都能找到 assets/。
    engine::render::TextureCache textureCache_{engine::asset::assetRoot()};
    QList<std::shared_ptr<EnemyPainter>> enemies;
    std::vector<std::shared_ptr<BulletPainter>> bullets;

    bool gameRunning;
    bool gamePaused;
    int score;
    int currentDifficulty;
    int currentMapIndex;

    // 道具相关
    QList<std::shared_ptr<Item>> items;
    void drawItem(QPainter &painter, const Item &item);
    void setPlayerInvincible(int playerId, bool invincible);
};

#endif // GAMEWINDOW_H
