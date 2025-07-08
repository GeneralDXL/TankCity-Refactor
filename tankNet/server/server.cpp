#include "server.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDebug>
#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

GameServer::GameServer(QObject *parent) : QTcpServer(parent)
{
    // 创建游戏线程
    gameThread = new QThread();
    game = new Game();
    game->moveToThread(gameThread);
    
    // 连接游戏广播信号
    connect(game, &Game::broadcastData, this, &GameServer::handleGameBroadcast);
    
    // 启动游戏线程
    gameThread->start();
}

GameServer::~GameServer()
{
    stopServer();
    
    // 清理游戏线程
    gameThread->quit();
    gameThread->wait();
    delete game;
    delete gameThread;
}

bool GameServer::startServer(quint16 port)
{
    if (!listen(QHostAddress::Any, port)) {
        qCritical() << "Could not start server:" << errorString();
        return false;
    }
    qInfo() << "Server started on port" << port;
    return true;
}

void GameServer::stopServer()
{
    close();
    
    // 关闭所有客户端连接
    for (QTcpSocket *client : clients.keys()) {
        client->disconnectFromHost();
        if (client->state() != QAbstractSocket::UnconnectedState) {
            client->waitForDisconnected();
        }
        client->deleteLater();
    }
    clients.clear();
    clientIds.clear();
    
    emit closed();
}

void GameServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *client = new QTcpSocket(this);
    client->setSocketDescriptor(socketDescriptor);
    
    connect(client, &QTcpSocket::readyRead, this, &GameServer::onReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &GameServer::onClientDisconnected);
    
    int clientId = nextClientId++;
    clients.insert(client, clientId);
    clientIds.insert(clientId, client);
    client->setProperty("clientId", clientId);
    
    // 通知游戏有新玩家加入
    QMetaObject::invokeMethod(game, "addPlayer", Qt::QueuedConnection, Q_ARG(int, clientId));
    
    qDebug() << "Client connected. ID:" << clientId;
}

void GameServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    
    int clientId = clients.value(client, -1);
    if (clientId == -1) return;
    
    clients.remove(client);
    clientIds.remove(clientId);
    
    // 通知游戏玩家离开
    QMetaObject::invokeMethod(game, "removePlayer", Qt::QueuedConnection, Q_ARG(int, clientId));
    
    qDebug() << "Client disconnected. ID:" << clientId;
    client->deleteLater();
}

void GameServer::onReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    
    while (client->bytesAvailable() >= static_cast<qint64>(sizeof(quint32))) {
        // 读取消息长度
        quint32 messageLength;
        if (client->peek(reinterpret_cast<char*>(&messageLength), sizeof(quint32)) < sizeof(quint32))
            return;
        
        messageLength = ntohl(messageLength);
        if (client->bytesAvailable() < static_cast<qint64>(sizeof(quint32) + messageLength))
            return;
        
        // 跳过长度头
        client->read(sizeof(quint32));
        
        // 读取消息体
        QByteArray data = client->read(messageLength);
        if (data.size() != static_cast<int>(messageLength)) {
            qWarning() << "Incomplete message received";
            continue;
        }
        
        // 处理客户端数据
        processClientData(client, data);
    }
}

void GameServer::processClientData(QTcpSocket *client, const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
    if (jsonDoc.isNull()) {
        qWarning() << "JSON parse error:" << error.errorString();
        return;
    }
    
    if (!jsonDoc.isObject()) {
        qWarning() << "Received non-object JSON data";
        return;
    }
    
    QJsonObject json = jsonDoc.object();
    int clientId = client->property("clientId").toInt();
    qDebug() << "Received key input from client" << clientId << ": " << json;
    if (json["type"].toString() == "key_input") {
        
        QMetaObject::invokeMethod(game, "handlePlayerInput", Qt::QueuedConnection,
                                  Q_ARG(int, clientId),
                                  Q_ARG(QJsonObject, json));
    }
    else if (json["type"].toString() == "start_game") {
        int mapIndex = json["mapIndex"].toInt();
        int difficulty = json["difficulty"].toInt();
        QMetaObject::invokeMethod(game, "startGame", Qt::QueuedConnection,
                                  Q_ARG(int, mapIndex),
                                  Q_ARG(int, difficulty));
    }
}

void GameServer::handleGameBroadcast(const QByteArray &data)
{
    // 广播给所有客户端
    for (QTcpSocket *client : clients.keys()) {
        sendToClient(client, data);
    }
}

void GameServer::sendToClient(QTcpSocket *client, const QByteArray &data)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState)
        return;
    
    // 添加长度头
    quint32 length = htonl(data.size());
    QByteArray packet(reinterpret_cast<char*>(&length), sizeof(quint32));
    packet.append(data);
    
    if (client->write(packet) == -1) {
        qWarning() << "Failed to send data to client:" << client->errorString();
    }
}