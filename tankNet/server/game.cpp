#include "game.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDateTime>
#include "server.h"

// "No such method"

Game::Game()
{
    gameMap = new Map();
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Game::gameLoop);
    connect(gameMap, &Map::broadcastMessage, this, &Game::onWallDelete);
    
    gameRunning = false;
    gamePaused = false;
}

Game::~Game()
{
    gameTimer->stop();
    
    qDeleteAll(players);
    players.clear();
    
    qDeleteAll(enemies);
    enemies.clear();
    
    bullets.clear();
    
    delete gameMap;
}

void Game::startGame(int mapIndex, int difficulty)
{
    this->mapIndex = mapIndex;
    this->currentDifficulty = difficulty;
    
    // 加载地图
    gameMap->loadMap(mapIndex);
    
    // 设置游戏参数
    score = 0;
    enemySpawnTimer = 0;
    
    switch (difficulty) {
    case 0: // Easy
        enemySpawnInterval = 150;
        maxEnemies = 3;
        break;
    case 1: // Medium
        enemySpawnInterval = 100;
        maxEnemies = 4;
        break;
    case 2: // Hard
        enemySpawnInterval = 70;
        maxEnemies = 5;
        break;
    }
    
    gameRunning = true;
    gamePaused = false;
    gameTimer->start(16); // 60 FPS

    emit broadcastData(QJsonDocument{{"type", "game_start"}}.toJson());
    
    // 为所有玩家发送初始状态
    for (int clientId : players.keys()) {
        sendInitialState(clientId);
    }
}

void Game::addPlayer(int clientId)
{
    if (players.contains(clientId)) return;
    
    // 生成随机位置
    int playerX, playerY;
    bool validPosition = false;
    int maxAttempts = 100;
    
    for (int i = 0; i < maxAttempts; i++) {
        playerX = 150 + QRandomGenerator::global()->bounded(500);
        playerY = 150 + QRandomGenerator::global()->bounded(300);
        
        QRect testRect(playerX - 20, playerY - 20, 40, 40);
        if (!gameMap->checkCollision(testRect)) {
            validPosition = true;
            break;
        }
    }
    
    if (!validPosition) {
        playerX = 400;
        playerY = 500;
    }
    
    // 创建玩家坦克
    Player *player = new Player(gameMap);
    player->init(playerX, playerY);
    players.insert(clientId, player);
    
    // 发送初始状态给新玩家
    if (gameRunning) {
        sendInitialState(clientId);
    }
    
    // 通知所有玩家有新玩家加入
    qDebug() << "new new new " << playerX << ' ' << playerY << '\n';
    QJsonObject json;
    json["type"] = "player_init";
    json["id"] = clientId;
    json["position"] = QJsonObject{{"x", playerX}, {"y", playerY}};
    json["bodyAngle"] = player->getBodyAngle();
    json["turretAngle"] = player->getTurretAngle();
    json["health"] = player->getHealth();
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson());
}

void Game::removePlayer(int clientId)
{
    if (!players.contains(clientId)) return;
    
    delete players[clientId];
    players.remove(clientId);
    
    // 通知所有玩家有玩家离开
    QJsonObject json;
    json["type"] = "player_left";
    json["id"] = clientId;
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson());
    
    // 如果没有玩家了，结束游戏
    if (players.isEmpty() && gameRunning) {
        gameOver(false);
    }
}

void Game::handlePlayerInput(int clientId, const QJsonObject &input)
{
    if (!gameRunning || gamePaused || !players.contains(clientId)) return;
    
    Player *player = players[clientId];
    qDebug() << "Handling input for player" << clientId << ": " << input << '\n';
    
    // 处理按键输入
    QJsonObject keys = input["keys"].toObject();
    player->setMoveForward(keys["w"].toBool());
    player->setMoveBackward(keys["s"].toBool());
    player->setTurnLeft(keys["a"].toBool());
    player->setTurnRight(keys["d"].toBool());
    
    // 处理鼠标位置
    QJsonObject mouse = input["mousePos"].toObject();
    QPoint mousePos(mouse["x"].toInt(), mouse["y"].toInt());
    player->setTurretTarget(mousePos);
    
    // 处理射击
    if (input["shoot"].toBool() && player->canShoot()) {
        Bullet *bullet = player->shoot();
        if (bullet) {
            bullets.push_back(std::shared_ptr<Bullet>(bullet));
            
            // 广播子弹创建
            QJsonObject bulletJson;
            bulletJson["type"] = "bullet_created";
            bulletJson["owner"] = clientId;
            bulletJson["position"] = QJsonObject{{"x", bullet->getPosition().x()}, {"y", bullet->getPosition().y()}};
            bulletJson["angle"] = bullet->getAngle();
            bulletJson["type"] = (bullet->getType() == BulletType::Player) ? "player" : "enemy";
            
            QJsonDocument doc(bulletJson);
            emit broadcastData(doc.toJson());
        }
    }
}

void Game::gameLoop()
{
    if (!gameRunning || gamePaused) return;
    
    updateGame();
    checkCollisions();
    broadcastGameState();
}

void Game::updateGame()
{
    // 更新所有玩家
    for (Player* player : players) {
        player->update();
    }
    
    // 更新敌人
    for (Enemy *enemy : enemies) {
        if (enemiesToRemove.contains(enemy)) continue;
        
        // 找到最近的玩家作为目标
        QPoint targetPos(400, 300); // 默认目标
        if (!players.isEmpty()) {
            int minDistance = INT_MAX;
            for (Tank* player : players) {
                int dist = QLineF(enemy->getPosition(), player->getPosition()).length();
                if (dist < minDistance) {
                    minDistance = dist;
                    targetPos = player->getPosition();
                }
            }
        }
        
        enemy->update(targetPos, gameMap);
        
        // 敌人射击
        if (enemy->canShoot()) {
            Bullet *bullet = enemy->shoot();
            if (bullet) {
                bullets.push_back(std::shared_ptr<Bullet>(bullet));
                
                // 广播敌人子弹
                QJsonObject bulletJson;
                bulletJson["type"] = "bullet_created";
                bulletJson["owner"] = -1; // 表示敌人
                bulletJson["position"] = QJsonObject{{"x", bullet->getPosition().x()}, {"y", bullet->getPosition().y()}};
                bulletJson["angle"] = bullet->getAngle();
                bulletJson["type1"] = static_cast<int>(bullet->getType());
                
                QJsonDocument doc(bulletJson);
                emit broadcastData(doc.toJson());
            }
        }
    }
    
    // 更新子弹
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        auto& bullet = *it;
        bullet->move();
        
        if (bullet->isOutOfBounds(800, 600)) {
            it = bullets.erase(it);
        } else {
            ++it;
        }
    }
    
    // 生成敌人
    enemySpawnTimer++;
    if (enemySpawnTimer >= enemySpawnInterval && enemies.size() < maxEnemies) {
        spawnEnemy();
        enemySpawnTimer = 0;
    }
}

void Game::checkCollisions()
{
    // 清空待删除敌人列表
    enemiesToRemove.clear();
    
    // 子弹碰撞检测
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        auto& bullet = *it;
        if (!bullet) {
            it = bullets.erase(it);
            continue;
        }
        
        // 检查墙壁碰撞
        if (gameMap->checkBulletCollision(bullet->getRect())) {
            it = bullets.erase(it);
            continue;
        }
        
        // 检查玩家碰撞 (敌人子弹)
        if (bullet->getType() == BulletType::Enemy) {
            bool hitPlayer = false;
            
            for (auto playerIt = players.begin(); playerIt != players.end(); ++playerIt) {
                Player* player = playerIt.value();
                
                if (bullet->getRect().intersects(player->getRect())) {
                    player->takeDamage(10);
                    hitPlayer = true;
                    
                    if (player->isDestroyed()) {
                        // 玩家死亡处理
                        int clientId = playerIt.key();
                        QJsonObject json;
                        json["type"] = "player_died";
                        json["id"] = clientId;
                        QJsonDocument doc(json);
                        emit broadcastData(doc.toJson());
                        removePlayer(clientId);
                    }
                    break;
                }
            }
            
            if (hitPlayer) {
                it = bullets.erase(it);
                continue;
            }
        }
        
        // 检查敌人碰撞 (玩家子弹)
        if (bullet->getType() == BulletType::Player) {
            bool hitEnemy = false;
            
            for (Enemy* enemy : enemies) {
                if (enemiesToRemove.contains(enemy)) continue;
                
                if (bullet->getRect().intersects(enemy->getRect())) {
                    enemy->takeDamage(25);
                    hitEnemy = true;
                    
                    if (enemy->isDestroyed()) {
                        score += 100;
                        enemiesToRemove.insert(enemy);
                        
                        // 广播敌人死亡
                        QJsonObject json;
                        json["type"] = "enemy_died";
                        json["id"] = static_cast<qint64>(reinterpret_cast<quintptr>(enemy));
                        QJsonDocument doc(json);
                        emit broadcastData(doc.toJson());
                    }
                    break;
                }
            }
            
            if (hitEnemy) {
                it = bullets.erase(it);
                continue;
            }
        }
        
        ++it;
    }
    
    // 删除被击败的敌人
    for (Enemy* enemy : enemiesToRemove) {
        enemies.removeOne(enemy);
        delete enemy;
    }
    enemiesToRemove.clear();
}

void Game::spawnEnemy()
{
    int borderMargin = 50;
    int x = 0, y = 0;
    bool validPosition = false;
    int maxAttempts = 20;
    
    for (int attempt = 0; attempt < maxAttempts; attempt++) {
        int side = QRandomGenerator::global()->bounded(4);
        
        switch (side) {
        case 0: // 上边
            x = borderMargin + QRandomGenerator::global()->bounded(800 - 2 * borderMargin);
            y = borderMargin;
            break;
        case 1: // 右边
            x = 800 - borderMargin;
            y = borderMargin + QRandomGenerator::global()->bounded(600 - 2 * borderMargin);
            break;
        case 2: // 下边
            x = borderMargin + QRandomGenerator::global()->bounded(800 - 2 * borderMargin);
            y = 600 - borderMargin;
            break;
        case 3: // 左边
            x = borderMargin;
            y = borderMargin + QRandomGenerator::global()->bounded(600 - 2 * borderMargin);
            break;
        }
        
        QRect spawnRect(x - 15, y - 15, 30, 30);
        if (!gameMap->checkCollision(spawnRect)) {
            validPosition = true;
            break;
        }
    }
    
    if (!validPosition) {
        x = 400;
        y = 300;
    }
    
    Enemy *enemy = new Enemy(gameMap, QPoint(x, y), currentDifficulty);
    enemies.append(enemy);
    
    // 广播新敌人
    QJsonObject json;
    json["type"] = "enemy_init";
    json["id"] = static_cast<qint64>(reinterpret_cast<quintptr>(enemy));
    json["position"] = QJsonObject{{"x", x}, {"y", y}};
    json["difficulty"] = currentDifficulty;
    json["bodyAngle"] = enemy->getBodyAngle();
    json["turretAngle"] = enemy->getTurretAngle();
    json["health"] = enemy->getHealth();
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson());
}

void Game::broadcastGameState()
{
    QJsonObject state;
    state["type"] = "game_state";
    state["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // 玩家状态
    QJsonArray playersArray;
    for (auto it = players.begin(); it != players.end(); ++it) {
        Tank* player = it.value();
        QJsonObject playerObj;
        playerObj["id"] = it.key();
        playerObj["position"] = QJsonObject{{"x", player->getPosition().x()}, {"y", player->getPosition().y()}};
        playerObj["bodyAngle"] = player->getBodyAngle();
        playerObj["turretAngle"] = player->getTurretAngle();
        playerObj["health"] = player->getHealth();
        playersArray.append(playerObj);
    }
    state["players"] = playersArray;
    
    // 敌人状态
    QJsonArray enemiesArray;
    for (Enemy* enemy : enemies) {
        if (enemiesToRemove.contains(enemy)) continue;
        
        QJsonObject enemyObj;
        enemyObj["id"] = static_cast<qint64>(reinterpret_cast<quintptr>(enemy));
        enemyObj["position"] = QJsonObject{{"x", enemy->getPosition().x()}, {"y", enemy->getPosition().y()}};
        enemyObj["bodyAngle"] = enemy->getBodyAngle();
        enemyObj["turretAngle"] = enemy->getTurretAngle();
        enemyObj["health"] = enemy->getHealth();
        enemiesArray.append(enemyObj);
    }
    state["enemies"] = enemiesArray;
    
    // 子弹状态
    QJsonArray bulletsArray;
    for (const auto& bullet : bullets) {
        if (!bullet) continue;
        
        QJsonObject bulletObj;
        bulletObj["position"] = QJsonObject{{"x", bullet->getPosition().x()}, {"y", bullet->getPosition().y()}};
        bulletsArray.append(bulletObj);
    }
    state["bullets"] = bulletsArray;
    
    // 游戏状态
    state["score"] = score;
    
    QJsonDocument doc(state);
    emit broadcastData(doc.toJson());
}

void Game::sendInitialState(int clientId)
{
    if (!players.contains(clientId)) return;
    
    // 发送地图信息
    QJsonObject mapJson;
    mapJson["type"] = "map_init";
    mapJson["mapIndex"] = mapIndex;
    
    QJsonArray wallArray;
    for (const Wall &wall : gameMap->getWalls()) {
        QJsonObject wallObj;
        QRect w = wall.getRect();
        wallObj["position"] = QJsonObject{{"x", w.x()}, {"y", w.y()}};
        wallObj["size"] = QJsonObject{{"width", w.width()}, {"height", w.height()}};
        wallObj["id"] = wall.getId();
        wallObj["type"] = wall.getType();
        wallArray.append(wallObj);
    }
    mapJson["walls"] = wallArray;
    
    QJsonDocument mapDoc(mapJson);
    emit broadcastData(mapDoc.toJson());
    
    // 发送玩家初始状态
    Player* player = players[clientId];
    QJsonObject playerJson;
    playerJson["type"] = "player_init";
    playerJson["id"] = clientId;
    playerJson["position"] = QJsonObject{{"x", player->getPosition().x()}, {"y", player->getPosition().y()}};
    playerJson["bodyAngle"] = player->getBodyAngle();
    playerJson["turretAngle"] = player->getTurretAngle();
    playerJson["health"] = player->getHealth();
    
    QJsonDocument playerDoc(playerJson);
    emit broadcastData(playerDoc.toJson());
    
    // 发送所有现有敌人状态
    for (Enemy* enemy : enemies) {
        QJsonObject enemyJson;
        enemyJson["type"] = "enemy_init";
        enemyJson["id"] = static_cast<qint64>(reinterpret_cast<quintptr>(enemy));
        enemyJson["position"] = QJsonObject{{"x", enemy->getPosition().x()}, {"y", enemy->getPosition().y()}};
        enemyJson["difficulty"] = enemy->getDifficulty();
        enemyJson["bodyAngle"] = enemy->getBodyAngle();
        enemyJson["turretAngle"] = enemy->getTurretAngle();
        enemyJson["health"] = enemy->getHealth();

        QJsonDocument enemyDoc(enemyJson);
        emit broadcastData(enemyDoc.toJson());
    }
}

void Game::gameOver(bool win)
{
    gameRunning = false;
    gameTimer->stop();
    
    if (win) {
        score += 500;
    }
    
    // 清理游戏对象
    qDeleteAll(enemies);
    enemies.clear();
    bullets.clear();
    
    // 广播游戏结束
    QJsonObject json;
    json["type"] = "game_over";
    json["win"] = win;
    json["score"] = score;
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson());
}

void Game::onWallDelete(const QJsonObject &json)
{
    emit broadcastData(QJsonDocument(json).toJson());
}