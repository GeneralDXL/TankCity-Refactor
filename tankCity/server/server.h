#ifndef GAMESERVER_H
#define GAMESERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QThread>
#include <QDateTime>
#include "game.h"

class GameServer : public QTcpServer
{
    Q_OBJECT

public:
    enum GameMode {
        SINGLE_MODE = 0,  // 单人模式
        ENDLESS_MODE,
        MULTI_MODE       // 多人竞技模式
    };

    explicit GameServer(QObject *parent = nullptr);
    ~GameServer();

    bool startServer(quint16 port);

public slots:
    void stopServer();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();
    void onReadyRead();

private:
    // 核心数据结构
    QMap<QTcpSocket*, int> clients;      // 客户端Socket -> ID
    QMap<int, QTcpSocket*> clientIds;     // ID -> 客户端Socket
    QMap<int, GameMode> gameModes;        // 客户端ID -> 游戏模式
    QMap<int, bool> isMultiplayer;        // 客户端ID -> 是否为多人模式

    // 多人游戏共享资源
    Game* multiGame;                      // 多人模式共享的游戏对象
    QThread* multiGameThread;             // 多人模式的游戏线程

    // 单人游戏资源
    QMap<int, Game*> singleGames;         // 客户端ID -> 专属游戏对象
    QMap<int, QThread*> singleThreads;    // 客户端ID -> 专属游戏线程

    int nextClientId;                     // 下一个客户端ID

    void processClientData(QTcpSocket* client, const QByteArray& data);
    void setGameMode(int clientId, GameMode mode);
    void initMultiplayerGame();
    void createSinglePlayerGame(int clientId);
    void cleanupSinglePlayerGame(int clientId);
    void cleanupMultiplayerGame();
    void handlePlayerInput(int clientId, const QJsonObject& input);
    void handleMultiplayerBroadcast(const QByteArray& data);
    void handleSinglePlayerBroadcast(int clientId, const QByteArray& data);
    Q_INVOKABLE void sendToClient(QTcpSocket *client, const QByteArray &data);
};

#endif // GAMESERVER_H