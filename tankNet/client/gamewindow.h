#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QWidget>
#include <QTimer>
#include <QTcpSocket>      // 客户端套接字类
#include <QTcpServer>      // 服务器类
#include <QAbstractSocket> // 抽象套接字基类，提供信号和错误码
#include "painter.h"

class GameWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget *parent = nullptr);
    void startGame(int mapIndex, int difficulty);
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
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void sendKey();

private:
    void drawHud(QPainter &painter);
    void sendToServer(const QJsonObject &json);
    

    QTcpSocket *socket;

    QTimer *gameTimer;
    MapPainter *gameMap;
    QMap <int, PlayerPainter*> players;
    QPoint mousePos;
    QSet<int> pressedKeys; // 存储按下的键

    QVector<EnemyPainter*> enemies;
    std::vector<std::shared_ptr<BulletPainter>> bullets;

    bool gameRunning;
    bool gamePaused;
    int score;
    int currentDifficulty;
    int currentMapIndex;
    int enemySpawnTimer;
};

#endif // GAMEWINDOW_H