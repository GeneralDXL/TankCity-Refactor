#include "gamewindow.h"

#include "core/GameLoop.h"   // engine/core：kTickMs
#include "net/MessageParser.h"
#include "render/QPainterBackend.h"
#include <QPainter>
#include <QMessageBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QKeyEvent>
#include <QtEndian>
#include<QDateTime>
#include <memory>

#include "clientconfig.h"

namespace {

/// 画玩家坦克用的原型数值。配置本身已缓存，解析也只在首次做一次。
const tankcity::config::TankStats &playerTankStats()
{
    static const tankcity::config::TankStats stats = client::playerStats();
    return stats;
}

/// 画第 difficulty 档敌人用的数值（难度只影响配色与初值，血量随后由服务端覆盖）。
tankcity::config::TankStats enemyTankStats(int difficulty)
{
    return client::enemyStats(difficulty);
}

} // namespace

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
    });
}


void GameWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);



    painter.fillRect(rect(), QColor(30, 30, 30));

    if (!gameRunning) {

        qDebug() << "游戏未开始";
        return;
    }


    // 地图层走 engine/render 的原语（贴图缓存 + 绘制后端）；
    // 坦克/子弹这些美术仍是客户端自己的 QPainter 代码（决议 D3-A）。
    engine::render::QPainterBackend backend(painter);
    gameMap->draw(backend, textureCache_);

    
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
        }
    }


    drawHud(painter);


    if (gamePaused) {
        painter.setPen(Qt::white);

        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(rect(), Qt::AlignCenter, "游戏暂停\n按P继续");
    }

    // 绘制道具
    for (const auto& item : items) {
        drawItem(painter, *item);
    }
}

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
}

void GameWindow::onDisconnected()
{
    qDebug() << "Disconnected from server";

    // 半条消息不能漏进下一次连接
    frameBuffer_.clear();
}

void GameWindow::onReadyRead()
{
    // 分帧交给 client/net：这里只负责「喂字节 → 逐条取消息 → 分发」。
    // 搬迁前是「读长度头 + waitForReadyRead 阻塞等待」，既会在 GUI 线程上卡最长 100ms，
    // 又会在等不到正文时把长度头吃掉、导致流错位；两件事都由 MessageFramer 修掉。
    frameBuffer_.append(socket->readAll());

    QJsonObject json;
    while (true) {
        switch (frameBuffer_.take(json)) {
        case client::net::MessageFramer::Status::Ok:
            break;
        case client::net::MessageFramer::Status::NeedMoreData:
            return;   // 剩下的等下一次 readyRead
        case client::net::MessageFramer::Status::Malformed:
            qWarning() << "丢弃一条无法解析的消息";
            continue;
        }

        // ↓↓ 以下分发与搬迁前逐行一致，只改了「消息是怎么取出来的」 ↓↓
        const QString type = json["type"].toString();
        if (type == "player_init")
        {
            qDebug() << "Received player tank update\n";
            // 更新玩家坦克状态（字段名集中在 client/net/MessageParser）
            const client::net::TankState state = client::net::parseTank(json);
            PlayerPainter *player = new PlayerPainter(playerTankStats());
            player->setPosition(state.position);
            qDebug() << "Player position:" << player->getPosition() << '\n';
            player->setBodyAngle(state.bodyAngle);
            player->setTurretAngle(state.turretAngle);
            player->setHealth(state.health);
            player->setId(state.id);

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
            const client::net::TankState state = client::net::parseTank(json);
            EnemyPainter *enemy = new EnemyPainter(state.difficulty,
                                                    enemyTankStats(state.difficulty));
            enemy->setPosition(state.position);
            enemy->setBodyAngle(state.bodyAngle);
            enemy->setTurretAngle(state.turretAngle);
            enemy->setHealth(state.health);
            enemy->setId(state.id);

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
            const client::net::BulletState state = client::net::parseBullet(json);
            BulletPainter *bullet = new BulletPainter(
                state.position, state.angle,
                state.fromPlayer ? BulletPainterType::Player : BulletPainterType::Enemy);
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
            gameRunning = true;
            gamePaused = false;
            gameTimer->start(engine::core::kTickMs); // 上行频率 = 逻辑帧长（唯一定义在 engine/core）

        }
        else if (type == "map_init")
        {
            qDebug() << "Received map initialization data\n";
            gameMap->clearWalls();
            QJsonArray wallArray = json["walls"].toArray();

            for (const QJsonValue &wallValue : wallArray)
            {
                const client::net::WallState state = client::net::parseWall(wallValue.toObject());
                gameMap->addWall(WallPainter(state.rect.x(), state.rect.y(),
                                             state.rect.width(), state.rect.height(),
                                             state.type, state.id));
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
                const client::net::TankState state = client::net::parseTank(playerValue.toObject());

                if (players.contains(state.id))
                {
                    PlayerPainter *player = players[state.id].get();
                    player->setPosition(state.position);
                    player->setBodyAngle(state.bodyAngle);
                    player->setTurretAngle(state.turretAngle);
                    player->setHealth(state.health);

                }else
                {
                    PlayerPainter *newPlayer = new PlayerPainter(playerTankStats());
                    newPlayer->setPosition(state.position);
                    newPlayer->setBodyAngle(state.bodyAngle);
                    newPlayer->setTurretAngle(state.turretAngle);
                    newPlayer->setHealth(state.health);
                    newPlayer->setId(state.id);
                    players.insert(state.id, std::shared_ptr<PlayerPainter>(newPlayer));
                }
            }
            QJsonArray enemyArray = json["enemies"].toArray();
            enemies.clear();
            for (const QJsonValue &enemyValue : enemyArray)
            {
                const client::net::TankState state = client::net::parseTank(enemyValue.toObject());

                EnemyPainter *newEnemy = new EnemyPainter(state.difficulty,
                                                          enemyTankStats(state.difficulty));
                newEnemy->setPosition(state.position);
                newEnemy->setBodyAngle(state.bodyAngle);
                newEnemy->setTurretAngle(state.turretAngle);
                newEnemy->setHealth(state.health);
                newEnemy->setId(state.id);
                enemies.push_back(std::shared_ptr<EnemyPainter>(newEnemy));
            }
            QJsonArray bulletArray = json["bullets"].toArray();
            bullets.clear();

            for (const QJsonValue &bulletValue : bulletArray)
            {
                const client::net::BulletState state = client::net::parseBullet(bulletValue.toObject());
                bullets.push_back(std::make_shared<BulletPainter>(
                    state.position, state.angle,
                    state.fromPlayer ? BulletPainterType::Player : BulletPainterType::Enemy));
            }
            score = json["score"].toInt();

            QJsonArray itemArray = json["items"].toArray();
            items.clear();
            for (const QJsonValue &itemValue : itemArray) {
                const client::net::ItemState state = client::net::parseItem(itemValue.toObject());
                Item *newitem = new Item;
                newitem->id = state.id;
                newitem->x = state.x;
                newitem->y = state.y;
                newitem->type = static_cast<ItemType>(state.type);
                items.push_back(std::shared_ptr<Item>(newitem));
            }
        }
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
            const client::net::ItemState state = client::net::parseItem(json);
            std::shared_ptr<Item> item = std::make_shared<Item>();
            item->id = state.id;
            item->x = state.x;
            item->y = state.y;
            item->type = static_cast<ItemType>(state.type);
            items.append(item);
        }
        else if (type == "item_picked") {
            int itemId = json["item_id"].toInt();

            for (auto it = items.begin(); it != items.end();) {
                if ((*it)->id == itemId) {
                    it = items.erase(it); 
                    break; 
                } else {
                    ++it; 
                }
            }
        }
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
    // 容器不作键位解释，原样记录；上行时只取用 W/A/S/D（键位表在 sendKey() 里）。
    // 相比搬迁前"只记录 WASD"，这里记录任意键，但上行的 JSON 字段完全没变。
    inputTracker_.keyPressed(event->key());
    sendKey();
}

void GameWindow::keyReleaseEvent(QKeyEvent *event)
{
    // 对任意键都记录松开：只记录 WASD 的话，其它键会永远留在按下集合里。
    inputTracker_.keyReleased(event->key());
    sendKey();
}

void GameWindow::mouseMoveEvent(QMouseEvent *event)
{
    inputTracker_.setPointer(event->pos());
    sendKey();
}

void GameWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 边沿事件：按下记一次，而不是"左键按着"这个状态 —— 这是长按不连发的前提。
        inputTracker_.primaryButtonPressed();
    }
    sendKey();
}


void GameWindow::sendKey()
{
    QJsonObject input;

    input["type"] = "key_input";
    
    // 取一次快照：主键的待发标记在这一步被取走 —— 这就是「长按不连发」的实现。
    const engine::input::InputTracker::Snapshot snapshot = inputTracker_.takeSnapshot();

    QJsonObject keys;
    keys["w"] = snapshot.keysDown.contains(Qt::Key_W);
    keys["a"] = snapshot.keysDown.contains(Qt::Key_A);
    keys["s"] = snapshot.keysDown.contains(Qt::Key_S);
    keys["d"] = snapshot.keysDown.contains(Qt::Key_D);
    input["keys"] = keys;

    QJsonObject mousePosObj;
    mousePosObj["x"] = snapshot.pointer.x();
    mousePosObj["y"] = snapshot.pointer.y();
    input["mousePos"] = mousePosObj;
    
    // 边沿触发：待发标记已在取快照时清除，所以一次点击只发一次 true（长按不连发）。
    input["shoot"] = snapshot.primaryPressed;

    sendToServer(input);
}



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
    inputTracker_.reset();   // 等价于搬迁前"清空按键集合"：清掉残留按键
}
