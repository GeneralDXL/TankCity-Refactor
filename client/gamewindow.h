#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QTcpSocket>
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

    QTimer *gameTimer;
    MapPainter *gameMap;
    QPoint mousePos;
    QSet<int> pressedKeys; // 存储按下的键

    QMap<int, std::shared_ptr<PlayerPainter>> players;

    /// 地图贴图缓存（每个文件只加载一次）。
    /// 资源根目录仍是相对当前工作目录 —— 与搬迁前完全一致（客户端必须在 build/bin 下启动），
    /// M1.1 后面的 engine/asset 会把它改成按可执行文件定位。
    engine::render::TextureCache textureCache_{QStringLiteral("./../../assets")};
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
