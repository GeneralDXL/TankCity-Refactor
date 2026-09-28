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
    nextClientId = 0;
    
    // 先不创建多人游戏实例，等第一个多人客户端连接时再创建
    multiGame = nullptr;
    multiGameThread = nullptr;
}

GameServer::~GameServer()
{
    stopServer();
    
    // 清理多人游戏
    if (multiGame) {
        multiGame->disconnect();
        multiGameThread->quit();
        if (!multiGameThread->wait(2000)) {
            multiGameThread->terminate();
            multiGameThread->wait();
        }
        delete multiGame;
        delete multiGameThread;
    }
    
    // 清理所有单人游戏
    for (int clientId : singleGames.keys()) {
        cleanupSinglePlayerGame(clientId);
    }
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
    
    // 清空游戏模式记录
    gameModes.clear();
    isMultiplayer.clear();
}

void GameServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *client = new QTcpSocket(this);
    client->setSocketDescriptor(socketDescriptor);
    
    connect(client, &QTcpSocket::readyRead, this, &GameServer::onReadyRead, Qt::QueuedConnection);
    connect(client, &QTcpSocket::disconnected, this, &GameServer::onClientDisconnected, Qt::QueuedConnection);
    
    int clientId = nextClientId++;
    clients.insert(client, clientId);
    clientIds.insert(clientId, client);
    client->setProperty("clientId", clientId);
    
    // 默认设置客户端为单人模式
    gameModes[clientId] = SINGLE_MODE;
    isMultiplayer[clientId] = false;
    
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
    
    // 根据模式移除玩家
    if (isMultiplayer.value(clientId, false) && multiGame) {
        // 多人模式：从共享游戏移除
        QMetaObject::invokeMethod(multiGame, "removePlayer", 
                                 Qt::QueuedConnection, 
                                 Q_ARG(int, clientId));
    } else if (gameModes.contains(clientId) && gameModes[clientId] != MULTI_MODE) {
        // 单人模式：销毁专属游戏
        cleanupSinglePlayerGame(clientId);
    }
    
    // 清除模式记录
    gameModes.remove(clientId);
    isMultiplayer.remove(clientId);
    
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
    QString type = json["type"].toString();


    if (type == "key_input") {
        // 处理按键输入
        handlePlayerInput(clientId, json);
    }
    else if (type == "start_game") {
        // 启动游戏
        int mapIndex = json["mapIndex"].toInt();
        int difficulty = json["difficulty"].toInt();
        QString modeStr = json["mode"].toString();
        GameMode mode = SINGLE_MODE;
        
        if (modeStr == "Mul") {
            mode = MULTI_MODE;
        } else if (modeStr == "End") {
            mode = ENDLESS_MODE;
        }
        
        setGameMode(clientId, mode);
        
        qDebug() << "Client" << clientId << "set mode to" << modeStr;
        
        if (gameModes.contains(clientId)) {
            if (gameModes[clientId] == MULTI_MODE) {
                // 多人/协作模式：启动多人游戏
                if (multiGame) {
                    multiGame->setId(-1);
                    QMetaObject::invokeMethod(multiGame, "startMultiGame", 
                                            Qt::QueuedConnection,
                                            Q_ARG(int, mapIndex));
                }
            } else {
                // 单人模式：启动专属游戏
                if (singleGames.contains(clientId)) {
                    Game* game = singleGames[clientId];
                    game->setId(clientId);
                    if (mode == ENDLESS_MODE) {
                        QMetaObject::invokeMethod(game, "startEndlessGame", 
                                                Qt::QueuedConnection,
                                                Q_ARG(int, mapIndex),
                                                Q_ARG(int, difficulty));
                    } else {
                        QMetaObject::invokeMethod(game, "startGame", 
                                                Qt::QueuedConnection,
                                                Q_ARG(int, mapIndex),
                                                Q_ARG(int, difficulty));                        
                    }

                }
            }
        }
    }
    else if (type == "out") {
        cleanupSinglePlayerGame(clientId);
        
    }
    else if (type == "game_over") {
        cleanupSinglePlayerGame(clientId);
    } else if (type == "multi_game_over") {

    }
    else {
        qWarning() << "Unknown message type:" << type;
    }
}

void GameServer::setGameMode(int clientId, GameMode mode)
{
    // 如果原来是多人模式，先从多人游戏中移除
    if (isMultiplayer.value(clientId, false) && multiGame) {
        QMetaObject::invokeMethod(multiGame, "removePlayer", 
                                 Qt::QueuedConnection, 
                                 Q_ARG(int, clientId));
    }
    
    // 处理单人游戏（如果存在）
    if (singleGames.contains(clientId)) {
        cleanupSinglePlayerGame(clientId);
    }
    
    // 更新模式记录
    gameModes[clientId] = mode;
    
    if (mode == MULTI_MODE) {
        // 多人/协作模式
        isMultiplayer[clientId] = true;
        
        // 初始化多人游戏（如果需要）
        initMultiplayerGame();
        
        // 添加玩家到多人游戏
        if (multiGame) {
            QMetaObject::invokeMethod(multiGame, "addPlayer", 
                                     Qt::QueuedConnection, 
                                     Q_ARG(int, clientId));
            
        }
    } else {
        // 单人模式
        isMultiplayer[clientId] = false;
        
        // 创建专属游戏
        createSinglePlayerGame(clientId);
    }
}

void GameServer::initMultiplayerGame()
{
    if (!multiGame) {
        qDebug() << "Initializing multiplayer game";
        
        multiGameThread = new QThread();
        multiGame = new Game();
        multiGame->moveToThread(multiGameThread);
        
        // 连接多人广播信号
        connect(multiGame, &Game::broadcastData, 
                this, &GameServer::handleMultiplayerBroadcast, Qt::QueuedConnection);
        
        // 启动多人游戏线程
        multiGameThread->start();
    }
}

void GameServer::createSinglePlayerGame(int clientId)
{
    if (singleGames.contains(clientId)) return;
    
    qDebug() << "Creating single-player game for client" << clientId;
    
    QThread* thread = new QThread();
    Game* game = new Game();
    game->moveToThread(thread);
    
    // 连接广播信号，使用lambda捕获clientId
    connect(game, &Game::broadcastData, 
            [this, clientId](const QByteArray& data) {
                handleSinglePlayerBroadcast(clientId, data);
            });
    
    // 启动游戏线程
    thread->start();
    
    singleGames[clientId] = game;
    singleThreads[clientId] = thread;
    
    // 添加玩家到专属游戏
    QMetaObject::invokeMethod(game, "addPlayer", 
                             Qt::QueuedConnection, 
                             Q_ARG(int, clientId));
}


void GameServer::cleanupSinglePlayerGame(int clientId) {
    if (!singleGames.contains(clientId)) return;

    qDebug() << 1;
    
    Game* game = singleGames.take(clientId);
    QThread* thread = singleThreads.take(clientId);
    
    qDebug() << 1;
    
    // 连接终止信号
    connect(game, &Game::gameTerminated, this, [this, clientId, game, thread]() {
        // 1. 断开所有连接
        game->disconnect();
        
        // 2. 停止线程
        thread->quit();
        if (!thread->wait(1000)) {
            thread->terminate();
            thread->wait(500);
        }
        
        // 3. 删除对象
        delete game;
        delete thread;
        
        qDebug() << "Game cleaned for client" << clientId;
    }, Qt::QueuedConnection);

    qDebug() << 1;
    
    // 触发游戏终止
    QMetaObject::invokeMethod(game, "safeStop", Qt::QueuedConnection);
    qDebug() << 1;
}

void GameServer::cleanupMultiplayerGame() {
    // 检查多人游戏实例是否存在
    if (!multiGame || !multiGameThread) return;

    qDebug() << "Starting cleanup for multiplayer game";

    // 连接游戏终止信号，确保线程和对象安全清理
    connect(multiGame, &Game::gameTerminated, this, [this]() {
        // 1. 断开游戏对象的所有信号连接
        multiGame->disconnect();

        // 2. 停止并清理线程
        multiGameThread->quit();
        if (!multiGameThread->wait(2000)) {  // 等待2秒正常退出
            qWarning() << "Multiplayer game thread did not exit normally, terminating";
            multiGameThread->terminate();
            multiGameThread->wait();
        }

        // 3. 删除游戏实例和线程
        delete multiGame;
        delete multiGameThread;

        // 4. 重置指针（避免野指针）
        multiGame = nullptr;
        multiGameThread = nullptr;

        qDebug() << "Multiplayer game cleaned up successfully";
    }, Qt::QueuedConnection);

    // 触发多人游戏安全停止（通过队列连接确保线程安全）
    QMetaObject::invokeMethod(multiGame, "safeStop", Qt::QueuedConnection);

    qDebug() << "Cleanup signal sent to multiplayer game";
}

void GameServer::handlePlayerInput(int clientId, const QJsonObject& input)
{
    if (gameModes.contains(clientId)) {
        if (gameModes[clientId] == MULTI_MODE) {
            // 多人/协作模式：发送到多人游戏
            if (multiGame) {
                QMetaObject::invokeMethod(multiGame, "handlePlayerInput", 
                                         Qt::QueuedConnection,
                                         Q_ARG(int, clientId),
                                         Q_ARG(QJsonObject, input));
            }
        } else {
            // 单人模式：发送到专属游戏
            if (singleGames.contains(clientId)) {
                Game* game = singleGames[clientId];
                QMetaObject::invokeMethod(game, "handlePlayerInput", 
                                         Qt::QueuedConnection,
                                         Q_ARG(int, clientId),
                                         Q_ARG(QJsonObject, input));
            }
        }
    }
}

void GameServer::handleMultiplayerBroadcast(const QByteArray &data)
{
    // 广播给所有多人模式客户端
    for (int clientId : gameModes.keys()) {
        if ((gameModes[clientId] == MULTI_MODE) && clientIds.contains(clientId)) {
            if (QTcpSocket* client = clientIds.value(clientId, nullptr)) {
                sendToClient(client, data);
            }
        }
    }
}

void GameServer::handleSinglePlayerBroadcast(int clientId, const QByteArray &data)
{
    // 只发送给特定客户端
    if (QTcpSocket* client = clientIds.value(clientId, nullptr)) {
        sendToClient(client, data);
    }
}

void GameServer::sendToClient(QTcpSocket *client, const QByteArray &data)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState)
        return;
    
    // 检查当前线程是否与socket的线程相同
    if (client->thread() != QThread::currentThread()) {
        // 通过事件队列调度到socket的线程执行
        QMetaObject::invokeMethod(this, [this, client, data]() {
            sendToClient(client, data);
        }, Qt::QueuedConnection);
        return;
    }

    // 在socket所属线程中执行实际写操作
    quint32 length = htonl(data.size());
    QByteArray packet(reinterpret_cast<char*>(&length), sizeof(quint32));
    packet.append(data);
    
    if (client->write(packet) == -1) {
        qWarning() << "Failed to send data to client:" << client->errorString();
    }
}
