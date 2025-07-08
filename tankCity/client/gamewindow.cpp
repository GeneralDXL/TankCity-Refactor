#include "gamewindow.h"
#include <QPainter>
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QtEndian>
//加
#include<QDateTime>
#include <memory>


GameWindow::GameWindow(QWidget *parent) : QWidget(parent)
{

    players.clear();
    enemies.clear();
    bullets.clear();
    items.clear();

    setFixedSize(1200, 900);

    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::disconnected, this, &GameWindow::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &GameWindow::onReadyRead);

    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &GameWindow::sendKey);

    gameMap = new MapPainter(); // 先创建地图

    // 设置默认值
    score = 0;
    enemySpawnTimer = 0;
    gameRunning = false;
    gamePaused = false;

    // 设置鼠标跟踪
    setMouseTracking(true);

    // 创建返回按钮
    QPushButton *backButton = new QPushButton("返回菜单", this);
    backButton->setGeometry(10, 10, 100, 30);
    backButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; border-radius: 5px; }"
                              "QPushButton:hover { background-color: #2980b9; }");
    connect(backButton, &QPushButton::clicked, this, [this]() {
        gameTimer->stop();
        QJsonObject json;
        json["type"] = "out";
        enemies.clear();
        items.clear();
        players.clear();
        bullets.clear();
        sendToServer(json);
        emit backToMenu();
        // emit disconnect();
    });
}


void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);



    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!gameRunning) {

        qDebug() << "游戏未开始"; //
        return;
    }


    gameMap->draw(painter);

    
    for (auto player : players) {
        if (player) {
            player->draw(painter);
        }
    }


    for (const std::shared_ptr<EnemyPainter>& enemy : enemies) {
        enemy->draw(painter);
    }

    for (auto& bullet : bullets) {
        if (bullet) {
            bullet->draw(painter);
            qDebug() << "success\n";
        }
    }


    drawHud(painter);


    if (gamePaused) {
        painter.setPen(Qt::white);

        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "游戏暂停\n按P继续");
    }

    //--------------------------加
    // 绘制道具
    for (const auto& item : items) {
        drawItem(painter, *item);
    }
    //-------------------------加
}
//------------------------------------------------------------------------------加
void GameWindow::drawItem(QPainter &painter, const Item &item) {
    painter.save();

    QColor color;
    QString symbol;

    switch (item.type) {
    case ItemType::HealthPack:
        color = QColor(0, 200, 0); // 绿色
        symbol = "H";
        break;
    case ItemType::AmmoBoost:
        color = QColor(255, 215, 0); // 金色
        symbol = "A";
        break;
    case ItemType::SpeedBoost:
        color = QColor(0, 100, 255); // 蓝色
        symbol = "S";
        break;
    case ItemType::Invincibility:
        color = QColor(200, 0, 200); // 紫色
        symbol = "I";
        break;
    }

    // 绘制闪烁效果
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if ((currentTime / 500) % 2 == 0) {
        painter.setBrush(color.lighter(150));
    } else {
        painter.setBrush(color);
    }

    painter.setPen(Qt::NoPen);
    painter.drawEllipse(item.x - 10, item.y - 10, 20, 20);

    // 绘制道具图标
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(item.x - 5, item.y + 5, symbol);

    painter.restore();
}

// gamewindow.cpp
void GameWindow::setPlayerInvincible(int playerId, bool invincible) {
    if (players.contains(playerId)) {
        players[playerId]->setInvincible(invincible);
    }
}
//---------------------------------------------------------------加

void GameWindow::drawHud(QPainter &painter)
{
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 12));

    // for (auto player : players) {
        // painter.drawText(10, 30, QString("玩家血量: %1").arg(player->getHealth()));
    // }
    // painter.drawText(10, 30, QString("玩家血量: %1").arg(players.first()->getHealth()));


    painter.drawText(width() - 150, 30, QString("分数: %1").arg(score));


    QString difficultyStr;
    switch (currentDifficulty) {
    case 0: difficultyStr = "简单"; break;
    case 1: difficultyStr = "中等"; break;
    case 2: difficultyStr = "困难"; break;
    }
    painter.drawText(width() / 2 - 50, 30, QString("难度: %1").arg(difficultyStr));


    painter.drawText(10, 50, QString("敌人数量: %1").arg(enemies.size()));

    for (auto player : players) {
        QPoint pos = player->getPosition();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(200, 30, 30));
        painter.drawRect(pos.x() - 25, pos.y() - 40, 50, 5);

        painter.setBrush(QColor(30, 200, 30));
        int healthWidth = player->getHealth() * 50 / 100;
        painter.drawRect(pos.x() - 25, pos.y() - 40, healthWidth, 5);

        painter.setPen(Qt::white);
        painter.drawText(pos.x() - 25, pos.y() - 42, QString("%1%").arg(player->getHealth()));
    }


    int yPos = 100;
    for (const auto& enemy : enemies) {
        QPoint pos = enemy->getPosition();
        painter.setPen(Qt::red);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(pos.x() - 25, pos.y() - 40, 50, 5);

        painter.setBrush(Qt::red);
        int enemyHealthWidth = enemy->getHealth() * 50 / 100;
        painter.drawRect(pos.x() - 25, pos.y() - 40, enemyHealthWidth, 5);

        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 8));
        painter.drawText(pos.x() - 25, pos.y() - 42, QString("%1").arg(enemy->getHealth()));
    }

    //---------------------------------------------加

    //---------------------------------------------加
}

void GameWindow::onDisconnected()
{
    qDebug() << "Disconnected from server";
}

void GameWindow::onReadyRead()
{
    while (socket->bytesAvailable()) {
        // 1. 读取消息长度头
        QByteArray lengthData = socket->read(sizeof(quint32));
        if (lengthData.size() != sizeof(quint32)) break;
        
        quint32 messageLength = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(lengthData.constData()));
        
        // 2. 等待完整消息到达
        while (socket->bytesAvailable() < messageLength) {
            if (!socket->waitForReadyRead(100)) break;
        }
        if (socket->bytesAvailable() < messageLength) break;

        QByteArray data = socket->read(messageLength);
        
        // 使用QJson处理数据
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &parseError);
        
        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
            continue;  // 继续处理下一个可用数据
        }
        
        if (!jsonDoc.isObject()) {
            qWarning() << "Invalid JSON structure, expected object";
            continue;
        }
        
        QJsonObject json = jsonDoc.object();
        
        QString type = json["type"].toString();
        if (type == "player_init")
        {
            qDebug() << "Received player tank update\n";
            // 更新玩家坦克状态
            PlayerPainter *player = new PlayerPainter();
            player->setPosition(QPoint(json["position"].toObject()["x"].toInt(),
                                       json["position"].toObject()["y"].toInt()));
            qDebug() << "Player position:" << player->getPosition() << '\n';
            player->setBodyAngle(json["bodyAngle"].toDouble());
            player->setTurretAngle(json["turretAngle"].toDouble());
            player->setHealth(json["health"].toInt());
            player->setId(json["id"].toInt());

            players.insert(player->getId(), std::shared_ptr<PlayerPainter>(player));

        }
 		else if (type == "player_left" || type == "player_died")
 		{
            qDebug() << "Received player left message\n";
            // 移除玩家坦克
            int playerId = json["id"].toInt();
            if (players.contains(playerId)) {
                players.remove(playerId);
            }
        } 
        else if (type == "enemy_init")
        {
            qDebug() << "Received enemy tank update\n";
            // 更新敌人坦克状态
            int enemyId = json["id"].toInt();
            EnemyPainter *enemy = new EnemyPainter(json["difficulty"].toInt());
            enemy->setPosition(QPoint(json["position"].toObject()["x"].toInt(),
                                       json["position"].toObject()["y"].toInt()));
            
            enemy->setBodyAngle(json["bodyAngle"].toDouble());
            enemy->setTurretAngle(json["turretAngle"].toDouble());
            enemy->setHealth(json["health"].toInt());
            enemy->setId(enemyId);

            enemies.push_back(std::shared_ptr<EnemyPainter>(enemy));

        }
        else if (type == "enemy_died")
        {
            qDebug() << "Received enemy died message\n";
            int enemyId = json["id"].toInt();
            
            // 安全移除敌人
            enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
                        [enemyId](const std::shared_ptr<EnemyPainter>& enemy) {
                            return enemy->getId() == enemyId;
                        }), enemies.end());

        } 
		else if (type == "bullet_created") 
		{
            BulletPainter *bullet = new BulletPainter(QPoint(json["position"].toObject()["x"].toInt(),
                                                             json["position"].toObject()["y"].toInt()),
                                                       json["angle"].toDouble(),
                                                       json["type1"].toString() == "player" ? BulletPainterType::Player : BulletPainterType::Enemy);
            bullets.push_back(std::make_shared<BulletPainter>(*bullet));                                           
        }
         else if (type == "delete_wall")
        {
            qDebug() << "Received delete wall command\n";
            gameMap->removeWall(json["wallId"].toInt());

        }
        else if (type == "game_start")
        {
            qDebug() << "Received game started\n";
            enemies.clear();
            players.clear();
            bullets.clear(); // 智能指针会自动释放内存
            items.clear();
            score = 0;
            enemySpawnTimer = 0;
            gameRunning = true;
            gamePaused = false;
            gameTimer->start(16); // 每秒60帧

        }
        else if (type == "map_init")
        {
            qDebug() << "Received map initialization data\n";
            gameMap->clearWalls();
            QJsonArray wallArray = json["walls"].toArray();

            for (const QJsonValue &wallValue : wallArray)
            {
                QJsonObject wallObj = wallValue.toObject();
                WallPainter wall(wallObj["position"].toObject()["x"].toInt(),
                                 wallObj["position"].toObject()["y"].toInt(),
                                 wallObj["size"].toObject()["width"].toInt(),
                                 wallObj["size"].toObject()["height"].toInt(),
                                 wallObj["type"].toInt(),
                                 wallObj["id"].toInt());
                gameMap->addWall(wall);
            }

        }
        else if (type == "game_over")
        {
            qDebug() << "Received game over message\n";
            gameTimer->stop();
            players.clear();
            enemies.clear();
            items.clear();
            qDebug() << enemies.size();
            gameRunning = false;
            QMessageBox::information(this, "游戏结束", "游戏结束！您的分数是: " + QString::number(score));
            emit gameFinished(score); // 发出游戏结束信号
            score = 0; // 重置分数
            QJsonObject json;
            json["type"] = "game_over";
            sendToServer(json);
            // emit disconnect();
        } else if (type == "multi_game_over") {
            gameTimer->stop();
            players.clear();
            enemies.clear();
            items.clear();
            gameRunning = false;
            QMessageBox::information(this, "游戏结束", "游戏结束！您的分数是: " + QString::number(score));
            emit gameFinished(score); // 发出游戏结束信号
            score = 0; // 重置分数
            QJsonObject json;
            json["type"] = "multi_game_over";
            sendToServer(json);            
        }
        else if (type == "game_state") {
            QJsonArray playerArray = json["players"].toArray();

            for (const QJsonValue &playerValue : playerArray)
            {
                QJsonObject playerObj = playerValue.toObject();
                int playerId = playerObj["id"].toInt();

                if (players.contains(playerId))
                {
                    PlayerPainter *player = players[playerId].get();
                    player->setPosition(QPoint(playerObj["position"].toObject()["x"].toInt(),
                                               playerObj["position"].toObject()["y"].toInt()));
                    player->setBodyAngle(playerObj["bodyAngle"].toDouble());
                    player->setTurretAngle(playerObj["turretAngle"].toDouble());
                    player->setHealth(playerObj["health"].toInt());

                }else
                {
                    PlayerPainter *newPlayer = new PlayerPainter();
                    newPlayer->setPosition(QPoint(playerObj["position"].toObject()["x"].toInt(),
                                                  playerObj["position"].toObject()["y"].toInt()));
                    newPlayer->setBodyAngle(playerObj["bodyAngle"].toDouble());
                    newPlayer->setTurretAngle(playerObj["turretAngle"].toDouble());
                    newPlayer->setHealth(playerObj["health"].toInt());
                    newPlayer->setId(playerId);
                    players.insert(playerId, std::shared_ptr<PlayerPainter>(newPlayer));
                }
            }
            QJsonArray enemyArray = json["enemies"].toArray();
            enemies.clear();
            for (const QJsonValue &enemyValue : enemyArray)
            {
                QJsonObject enemyObj = enemyValue.toObject();
                
                int enemyId = enemyObj["id"].toInt();
                EnemyPainter *newEnemy = new EnemyPainter(enemyObj["difficulty"].toInt());
                newEnemy->setPosition(QPoint(enemyObj["position"].toObject()["x"].toInt(),
                                                enemyObj["position"].toObject()["y"].toInt()));
                newEnemy->setBodyAngle(enemyObj["bodyAngle"].toDouble());
                newEnemy->setTurretAngle(enemyObj["turretAngle"].toDouble());
                newEnemy->setHealth(enemyObj["health"].toInt());
                newEnemy->setId(enemyId);
                enemies.push_back(std::shared_ptr<EnemyPainter>(newEnemy));
                
                // auto it = std::find_if(enemies.begin(), enemies.end(),
                //                        [enemyId](const std::shared_ptr<EnemyPainter>& enemy) { return enemy->getId() == enemyId; });

                // if (it != enemies.end())
                // {
                //     EnemyPainter *enemy = it->get();
                //     enemy->setPosition(QPoint(enemyObj["position"].toObject()["x"].toInt(),
                //                                enemyObj["position"].toObject()["y"].toInt()));
                //     enemy->setBodyAngle(enemyObj["bodyAngle"].toDouble());
                //     enemy->setTurretAngle(enemyObj["turretAngle"].toDouble());
                //     enemy->setHealth(enemyObj["health"].toInt());

                // }
                // else
                // {
                //     EnemyPainter *newEnemy = new EnemyPainter(enemyObj["difficulty"].toInt());
                //     newEnemy->setPosition(QPoint(enemyObj["position"].toObject()["x"].toInt(),
                //                                   enemyObj["position"].toObject()["y"].toInt()));
                //     newEnemy->setBodyAngle(enemyObj["bodyAngle"].toDouble());
                //     newEnemy->setTurretAngle(enemyObj["turretAngle"].toDouble());
                //     newEnemy->setHealth(enemyObj["health"].toInt());
                //     newEnemy->setId(enemyId);
                //     enemies.push_back(std::shared_ptr<EnemyPainter>(newEnemy));
                // }
            }
            QJsonArray bulletArray = json["bullets"].toArray();
            bullets.clear();

            for (const QJsonValue &bulletValue : bulletArray)
            {
                QJsonObject bulletObj = bulletValue.toObject();
                auto bullet = std::make_shared<BulletPainter>(QPoint(bulletObj["position"].toObject()["x"].toInt(),
                                                bulletObj["position"].toObject()["y"].toInt()),
                                                bulletObj["angle"].toDouble(),
                                                bulletObj["type1"].toString() == "player" ? BulletPainterType::Player : BulletPainterType::Enemy);
                bullets.push_back(bullet);
            }
            score = json["score"].toInt();

            QJsonArray itemArray = json["items"].toArray();
            items.clear();
            for (const QJsonValue &itemValue : itemArray) {
                QJsonObject itemObj = itemValue.toObject();
                int itemId = itemObj["id"].toInt();
                Item *newitem = new Item;
                newitem->id = itemId;
                newitem->x = itemObj["x"].toInt();
                newitem->y = itemObj["y"].toInt();
                newitem->type = static_cast<ItemType>(itemObj["item_type"].toInt());
                items.push_back(std::shared_ptr<Item>(newitem));
                // auto it = std::find_if(items.begin(), items.end(),
                //                        [itemId](const std::shared_ptr<Item>& item) { return item->id == itemId; });
                // if (it == items.end()) {
                //     Item *newitem = new Item;
                //     newitem->id = itemId;
                //     newitem->x = itemObj["x"].toInt();
                //     newitem->y = itemObj["y"].toInt();
                //     newitem->type = static_cast<ItemType>(itemObj["type"].toInt());
                //     items.push_back(std::shared_ptr<Item>(newitem));
                // }
            }
        }
        //--------------------------------------------------------------------------加
        else if (type == "player_invincible")
        {
            int playerId = json["player_id"].toInt();
            bool active = json["active"].toBool();

            if (players.contains(playerId)) {
                players[playerId]->setInvincible(active);

                // 添加无敌状态特效
                if (active) {
                    QTimer::singleShot(10000, [this, playerId]() {
                        if (players.contains(playerId)) {
                            players[playerId]->setInvincible(false);
                        }
                    });
                }
            }
        }
        else if (type == "item_spawned") {
            std::shared_ptr<Item> item = std::make_shared<Item>();
            item->id = json["id"].toInt();
            item->x = json["x"].toInt();
            item->y = json["y"].toInt();
            item->type = static_cast<ItemType>(json["item_type"].toInt());
            items.append(item);
        }
        else if (type == "item_picked") {
            int itemId = json["item_id"].toInt();

            for (auto it = items.begin(); it != items.end(); /* 不在这里递增 */) {
                if ((*it)->id == itemId) {
                    // erase 返回下一个有效的迭代器，直接赋值给 it
                    it = items.erase(it); 
                    break; 
                } else {
                    ++it; 
                }
            }
        }
        //--------------------------------------------------------------------------加
        else {
            qWarning() << "Unknown message type:" << type;
            qDebug() << json << '\n';
        }

        update();
    }
}


void GameWindow::sendToServer(const QJsonObject &json)
{
    if (!socket->isOpen()) {
        qWarning() << "Socket is not open, cannot send data";
        return;
    }

    QJsonDocument jsonDoc(json);
    QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);

    // 发送消息长度
    quint32 messageLength = qToBigEndian(static_cast<quint32>(jsonData.size()));
    socket->write(reinterpret_cast<const char*>(&messageLength), sizeof(messageLength));

    // 发送JSON数据
    socket->write(jsonData);
    
}

void GameWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W || event->key() == Qt::Key_A ||
        event->key() == Qt::Key_S || event->key() == Qt::Key_D ) {
        pressedKeys.insert(event->key());
    }
    sendKey();
}

void GameWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_W || event->key() == Qt::Key_A ||
        event->key() == Qt::Key_S || event->key() == Qt::Key_D ) {
        pressedKeys.remove(event->key());
    }
    sendKey();
}

void GameWindow::mouseMoveEvent(QMouseEvent *event)
{
    mousePos = event->pos();
    sendKey();
}

void GameWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        pressedKeys.insert(Qt::LeftButton);
    }
    sendKey();
}


void GameWindow::sendKey()
{
    QJsonObject input;

    input["type"] = "key_input";
    
    QJsonObject keys;
    keys["w"] = pressedKeys.contains(Qt::Key_W);
    keys["a"] = pressedKeys.contains(Qt::Key_A);
    keys["s"] = pressedKeys.contains(Qt::Key_S);
    keys["d"] = pressedKeys.contains(Qt::Key_D);
    input["keys"] = keys;

    QJsonObject mousePosObj;
    mousePosObj["x"] = mousePos.x();
    mousePosObj["y"] = mousePos.y();
    input["mousePos"] = mousePosObj;
    
    input["shoot"] = pressedKeys.contains(Qt::LeftButton);
    pressedKeys.remove(Qt::LeftButton); // 清除左键按下状态，避免重复发送

    sendToServer(input);
    // qDebug() << "Sent key input to server\n" ;
    // qDebug() << input << '\n';
}

// 在 gamewindow.cpp 文件中实现

void GameWindow::connectToServer(const QString &host, quint16 port)
{
        qDebug() << "1\n";
    if (socket->state() == QTcpSocket::ConnectedState) {
        qDebug() << "Already connected to server";
        return;
    }
        qDebug() << "1\n";
    
    // 重置连接状态
    if (socket->state() != QTcpSocket::UnconnectedState) {
        socket->abort(); // 终止现有连接
    }
    
    qDebug() << "Connecting to" << host << "on port" << port;
    socket->connectToHost(host, port);
    
    // 可选：添加超时处理
    if (!socket->waitForConnected(3000)) { // 等待3秒连接
        qWarning() << "Connection failed:" << socket->errorString();
        // 可以发出信号通知UI连接失败
    }
}

void GameWindow::startGame(int mapIndex, int difficulty, int mode)
{
    currentMapIndex = mapIndex;
    currentDifficulty = difficulty;
    gameRunning = true;
    connectToServer("127.0.0.1", 12345);

    qDebug() << "Starting game\n";

    QJsonObject json;
    json["type"] = "start_game";
    json["mapIndex"] = mapIndex;
    json["difficulty"] = difficulty;
    switch (mode) {
        case 0:
            json["mode"] = "Sig";
            break;
        case 1:
            json["mode"] = "End";
            break;
        case 2:
            json["mode"] = "Mul";
            break;
    }
    sendToServer(json);
    pressedKeys.clear();
}
