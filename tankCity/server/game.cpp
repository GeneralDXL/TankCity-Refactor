#include "game.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDateTime>
//------------------加
#include <QTimer>
#include "server.h"

// "No such method"

Game::Game()
{
    players.clear();
    items.clear();
    enemies.clear();
    bullets.clear();

    gameMap = new Map();
    gameTimer = new QTimer(this);
    connect(gameTimer, &QTimer::timeout, this, &Game::gameLoop);
    connect(gameMap, &Map::broadcastMessage, this, &Game::onWallDelete);
    
    gameRunning = false;
    gamePaused = false;
}

Game::~Game()
{
    safeStop();  // 确保停止所有活动
    delete gameMap;  // 同步删除地图
}

void Game::startEndlessGame(int mapIndex, int difficulty)
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
    gameModle = 1;

    enemies.clear();
    items.clear();
    bullets.clear();

    emit broadcastData(QJsonDocument{{"type", "game_start"}}.toJson(), id);
    
    // 为所有玩家发送初始状态
    for (int clientId : players.keys()) {
        sendInitialState(clientId);
    }
}

void Game::startMultiGame(int mapIndex)
{
    this->mapIndex = mapIndex;
    gameMap->loadMap(mapIndex);

    score = 0;
    
    gameModle = 2;
    gameRunning = true;
    gamePaused = false;
    gameTimer->start(16); // 60 FPS

    enemies.clear();
    items.clear();
    bullets.clear();

    emit broadcastData(QJsonDocument{{"type", "game_start"}}.toJson(), id);
    
    // 为所有玩家发送初始状态
    for (int clientId : players.keys()) {
        sendInitialState(clientId);
    }
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
    gameModle = 0;
    
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

    enemies.clear();
    items.clear();
    bullets.clear();

    emit broadcastData(QJsonDocument{{"type", "game_start"}}.toJson(), id);
    
    // 为所有玩家发送初始状态
    for (int clientId : players.keys()) {
        sendInitialState(clientId);
    }
}

void Game::addPlayer(int clientId)
{
    if (players.contains(clientId)) return;
    
    int playerX=80, playerY=400;
    int type = gameMap->getTerrainType(QPoint(playerX,playerY));
    
    // 创建玩家坦克
    auto player = std::make_shared<Player>(gameMap);
    player->init(playerX, playerY);
    players.insert(clientId, player);
    oneOfId = clientId;
    
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
    emit broadcastData(doc.toJson(), id);
}

void Game::removePlayer(int clientId) {
    if (!players.contains(clientId)) return;
    
    // 先移除引用再触发析构
    auto player = players.take(clientId);  // 从map移除

    qDebug() << "remove";

    if (gameModle == 0) gameOver(false);
    else if (gameModle == 1) endlessGameOver();
    
    // player对象在此作用域结束时自动析构
}

void Game::handlePlayerInput(int clientId, const QJsonObject &input)
{
    if (!gameRunning || gamePaused || !players.contains(clientId)) return;
    
    auto player = players[clientId];
    
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
            emit broadcastData(doc.toJson(), id);
        }
    }
}

void Game::gameLoop()
{
    if (isEnd) return;

    if (!gameRunning || gamePaused) return;
    
    updateGame();

    if (players.size() > 1) gameS = true;

    if (gameModle == 2 && gameS && players.size() == 1) {
        isEnd = true;
        if (gameTimer) {
            gameTimer->stop();
            disconnect(gameTimer, nullptr, this, nullptr);
        }
        gameS = false;
        multiGameOver();
    }
    
    checkCollisions();
    if(!gameRunning) return;

    broadcastGameState();
    
    //------------------加
    // 道具系统
    itemSpawnTimer++;
    if (itemSpawnTimer >= ITEM_SPAWN_INTERVAL && items.size() < MAX_ITEMS) {
        spawnItem();
        itemSpawnTimer = 0;
    }
    checkItemCollisions();
    //------------------加
}

//------------------------------------------------------------------------------------------------------加
void Game::spawnItem() {
    if (isEnd) return;

    int x = QRandomGenerator::global()->bounded(100, 700);
    int y = QRandomGenerator::global()->bounded(100, 500);

    // 确保不会生成在墙上
    if (gameMap->checkCollision(QRect(x-15, y-15, 30, 30))) {
        return;
    }

    ItemType type = static_cast<ItemType>(QRandomGenerator::global()->bounded(4));
    Item item{nextItemId++, type, x, y, 10000}; // 10秒持续时间

    items.append(item);

    // 广播新道具
    QJsonObject json;
    json["type"] = "item_spawned";
    json["id"] = item.id;
    json["x"] = x;
    json["y"] = y;
    json["item_type"] = static_cast<int>(type);
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson(), id);
}

void Game::checkItemCollisions() {
    if (isEnd) return;
    if (players.isEmpty()) return;

    for (auto it = items.begin(); it != items.end();) {
        bool collected = false;
        QRect itemRect(it->x - 10, it->y - 10, 20, 20);

        for (auto playerIt = players.begin(); playerIt != players.end(); ++playerIt) {
            if (playerIt.value()->getRect().intersects(itemRect)) {
                applyItemEffect(playerIt.key(), it->type);

                // 广播道具拾取
                QJsonObject json;
                json["type"] = "item_picked";
                json["item_id"] = it->id;
                json["player_id"] = playerIt.key();
                QJsonDocument doc(json);
                emit broadcastData(doc.toJson(), id);

                collected = true;
                break;
            }
        }

        if (collected) {
            it = items.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::applyItemEffect(int playerId, ItemType type) {
    auto player = players[playerId];
    if (!player) return;

    switch (type) {
    case ItemType::HealthPack:
        player->setHealth(qMin(player->getHealth() + 30, 100));
        break;
    case ItemType::AmmoBoost:
        player->setShootDelay(qMax(5, player->getShootDelay() - 2));
        break;
    case ItemType::SpeedBoost: {
        float originalSpeed = player->getSpeed();
        player->setSpeed(originalSpeed * 1.5);

        // 10秒后恢复原始速度
        QTimer::singleShot(10000, [player, originalSpeed]() {
            player->setSpeed(originalSpeed);
        });
        break;
    }
    case ItemType::Invincibility:
        player->setInvincible(true);

        // 广播无敌状态激活
        QJsonObject invincibleJson;
        invincibleJson["type"] = "player_invincible";
        invincibleJson["player_id"] = playerId;
        invincibleJson["active"] = true;
        QJsonDocument invDoc(invincibleJson);
        emit broadcastData(invDoc.toJson(), id);

        // 10秒后取消无敌状态
        QTimer::singleShot(10000, [this, playerId, player]() {
            player->setInvincible(false);

            // 广播无敌状态结束
            QJsonObject invEndJson;
            invEndJson["type"] = "player_invincible";
            invEndJson["player_id"] = playerId;
            invEndJson["active"] = false;
            QJsonDocument invEndDoc(invEndJson);
            emit broadcastData(invEndDoc.toJson(), id);
        });
        break;
    }
}
//------------------------------------------------------------------------------------------------------------------加
void Game::updateGame()
{
    if (isEnd) return;

    // 更新所有玩家
    for (auto player : players) {
        player->update();
    }
    
    // 更新敌人
    if (gameModle != 2) {
        for (auto enemy : enemies) {
            if (enemiesToRemove.contains(enemy)) continue;
            
            QPoint oldEnemyPos = enemy->getPosition();

            enemy->update(players[oneOfId]->getPosition(), gameMap);

            // 检测敌人与玩家坦克的碰撞
            if (enemy->getRect().intersects(players[oneOfId]->getRect())) {
                enemy->setPosition(oldEnemyPos);
            }
            
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
                    emit broadcastData(doc.toJson(), id);
                }
            }
        }
    }


    // 更新子弹
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        auto& bullet = *it;
        bullet->move();
        
        if (bullet->isOutOfBounds(1200, 900)) {
            it = bullets.erase(it);
        } else {
            ++it;
        }
    }

    // 生成敌人
    if (gameModle == 1) {
        enemySpawnTimer++;
        if (enemySpawnTimer >= enemySpawnInterval && enemies.size() < maxEnemies) {
            spawnEnemy();
            enemySpawnTimer = 0;
        }
        if (players.isEmpty()) {
            
            isEnd = true;
            if (gameTimer) {
                gameTimer->stop();
                disconnect(gameTimer, nullptr, this, nullptr);
            }            
            endlessGameOver();
            return;
        }
    } else if (gameModle == 0) {
        enemySpawnTimer++;
        if (enemySpawnTimer >= enemySpawnInterval && generateNum < maxEnemies) {
            spawnEnemy();
            enemySpawnTimer = 0;
            generateNum++;
        }
        if (enemies.empty() && generateNum == maxEnemies) {
            qDebug() << "winwinwin";
            
            isEnd = true;
            if (gameTimer) {
                gameTimer->stop();
                disconnect(gameTimer, nullptr, this, nullptr);
            }
            gameOver(true);
            return;
        }
        if (players.isEmpty()) {
            qDebug() << "NoNoNo";
            
            isEnd = true;
            if (gameTimer) {
                gameTimer->stop();
                disconnect(gameTimer, nullptr, this, nullptr);
            }            
            gameOver(false);
            return;
        }
    }
}

void Game::checkCollisions()
{

    if (isEnd) return;
    // 清空待删除敌人列表
    enemiesToRemove.clear();
    
    // 子弹碰撞检测
    for (auto it = bullets.begin(); it != bullets.end();) {
        if(!gameRunning) return;
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
        // if (bullet->getType() == BulletType::Enemy) {
            bool hitPlayer = false;
            for (auto playerIt = players.begin(); playerIt != players.end(); ++playerIt) {
                auto player = playerIt.value();
                
                //----------------------------------------------加
                // 检查无敌状态 - 新增
                if (player->isInvincible()) {
                    // 无敌状态下不受伤，但子弹消失
                    qDebug() << "Player" << playerIt.key() << "is invincible, bullet ignored";
                    continue;
                }
                //------------------------------------------------加

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
                        // emit broadcastData(doc.toJson(), id);
                        // removePlayer(clientId);
                        if (players.size() > 1) {
                            removePlayer(clientId);
                        }
                        else if (players.size() == 1) {
                            if(gameModle == 0) gameOver(false);
                            else if (gameModle == 1) endlessGameOver();
                        }
                    }
                    break;
                }

                if (players.isEmpty()) break;
            }

            if (hitPlayer) {
                qDebug() << "hit\n";
                it = bullets.erase(it);
                continue;
            }
        // }
        
        if (!gameRunning) return;
        if (players.isEmpty()) return;

        // 检查敌人碰撞 (玩家子弹)
        if (bullet->getType() == BulletType::Player) {
            bool hitEnemy = false;
            
            for (auto enemy : enemies) {
                if (enemiesToRemove.contains(enemy)) continue;
                
                if (bullet->getRect().intersects(enemy->getRect())) {
                    enemy->takeDamage(25);
                    hitEnemy = true;

                    
                    if (enemy->isDestroyed()) {
                        score += 100;
                        enemiesToRemove.insert(enemy);
                        overNum++;
                        
                        // 广播敌人死亡
                        QJsonObject json;
                        json["type"] = "enemy_died";
                        json["id"] = enemy->getId();
                        QJsonDocument doc(json);
                        emit broadcastData(doc.toJson(), id);
                        if (overNum == maxEnemies && gameModle == 0) {
                            
                            isEnd = true;
                            if (gameTimer) {
                                gameTimer->stop();
                                disconnect(gameTimer, nullptr, this, nullptr);
                            }
                            gameOver(true);
                            return;
                        }
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
    for (auto enemy : enemiesToRemove) {
        enemies.removeOne(enemy);
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
    
    auto enemy = std::make_shared<Enemy>(gameMap, QPoint(x, y), currentDifficulty);
    enemies.append(enemy);
    
    // 广播新敌人
    QJsonObject json;
    json["type"] = "enemy_init";
    json["id"] = enemy->getId();
    json["position"] = QJsonObject{{"x", x}, {"y", y}};
    json["difficulty"] = currentDifficulty;
    json["bodyAngle"] = enemy->getBodyAngle();
    json["turretAngle"] = enemy->getTurretAngle();
    json["health"] = enemy->getHealth();
    QJsonDocument doc(json);
    emit broadcastData(doc.toJson(), id);
}

void Game::broadcastGameState()
{
    if (isEnd) return;
    if (players.isEmpty()) return;

    QJsonObject state;
    state["type"] = "game_state";
    state["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    
    // 玩家状态
    QJsonArray playersArray;
    for (auto it = players.begin(); it != players.end(); ++it) {
        auto player = it.value();
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
    for (auto enemy : enemies) {
        if (enemiesToRemove.contains(enemy)) continue;
        
        QJsonObject enemyObj;
        enemyObj["id"] = enemy->getId();
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
        bulletObj["angle"] = bullet->getAngle();
        bulletObj["type"] = (bullet->getType() == BulletType::Player) ? "player" : "enemy";
        bulletsArray.append(bulletObj);
    }
    state["bullets"] = bulletsArray;

    QJsonArray itemsArray;
    for (const auto& item : items) {
        QJsonObject itemObj;
        itemObj["id"] = item.id;
        itemObj["x"] = item.x;
        itemObj["y"] = item.y;
        itemObj["item_type"] = static_cast<int>(item.type);
        itemsArray.append(itemObj);
    }
    state["items"] = itemsArray;
    
    // 游戏状态
    state["score"] = score;
    
    QJsonDocument doc(state);
    emit broadcastData(doc.toJson(), id);
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
    emit broadcastData(mapDoc.toJson(), id);
    
    // 发送玩家初始状态
    auto player = players[clientId];
    QJsonObject playerJson;
    playerJson["type"] = "player_init";
    playerJson["id"] = clientId;
    playerJson["position"] = QJsonObject{{"x", player->getPosition().x()}, {"y", player->getPosition().y()}};
    playerJson["bodyAngle"] = player->getBodyAngle();
    playerJson["turretAngle"] = player->getTurretAngle();
    playerJson["health"] = player->getHealth();
    
    QJsonDocument playerDoc(playerJson);
    emit broadcastData(playerDoc.toJson(), id);
    
    // 发送所有现有敌人状态
    for (auto enemy : enemies) {
        QJsonObject enemyJson;
        enemyJson["type"] = "enemy_init";
        enemyJson["id"] = enemy->getId();
        enemyJson["position"] = QJsonObject{{"x", enemy->getPosition().x()}, {"y", enemy->getPosition().y()}};
        enemyJson["difficulty"] = enemy->getDifficulty();
        enemyJson["bodyAngle"] = enemy->getBodyAngle();
        enemyJson["turretAngle"] = enemy->getTurretAngle();
        enemyJson["health"] = enemy->getHealth();

        QJsonDocument enemyDoc(enemyJson);
        emit broadcastData(enemyDoc.toJson(), id);
    }
}

void Game::terminateGame() {
    // 确保只执行一次
    if (!gameRunning) return;
    
    // 停止定时器并断开连接
    // if (gameTimer) {
    //     gameTimer->stop();
    //     disconnect(gameTimer, nullptr, this, nullptr);
    // }
    
    gameRunning = false;
    
    // 安全清理顺序
    bullets.clear();    // 1. 清理子弹（可能引用其他对象）
    items.clear();      // 2. 清理道具
    
    // 3. 清理敌人（在玩家之前）
    // for (auto& enemy : enemies) {
    //     enemy->disconnect();
    // }
    enemies.clear();
    
    // 4. 清理玩家（最后，因为可能持有敌人/子弹引用）
    for (auto it = players.begin(); it != players.end();) {
        auto player = it.value();
        // player->disconnect();
        it = players.erase(it);
    }
    
    // 重置状态
    score = 0;
    enemySpawnTimer = 0;
    generateNum = overNum = 0;
    itemSpawnTimer = 0;
}

void Game::gameOver(bool win) {
    // if (win) score += 500;
    
    // 先广播结果
    QJsonObject json;
    json["type"] = "game_over";
    json["win"] = win;
    json["score"] = score;
    qDebug() << "game";
    emit broadcastData(QJsonDocument(json).toJson(), id);
    
    

    // 延迟清理（确保广播完成）
    QTimer::singleShot(100, this, [this]() {
        terminateGame();
        emit gameTerminated(); // 新增信号通知服务器
    });
}

void Game::endlessGameOver() {
    QJsonObject json;
    json["type"] = "game_over";
    json["win"] = false;
    json["score"] = score;
    emit broadcastData(QJsonDocument(json).toJson(), id);
    
    // 延迟清理（确保广播完成）
    QTimer::singleShot(100, this, [this]() {
        terminateGame();
        emit gameTerminated(); // 新增信号通知服务器
    });    
}

void Game::safeStop() {
    // 只是终止游戏，不重复清理
    terminateGame();
}
void Game::onWallDelete(const QJsonObject &json)
{
    emit broadcastData(QJsonDocument(json).toJson(), id);
}

void Game::multiGameOver() {
    QJsonObject json;
    json["type"] = "multi_game_over";
    json["score"] = score;
    qDebug() << "game";
    emit broadcastData(QJsonDocument(json).toJson(), id);

    // 延迟清理（确保广播完成）
    QTimer::singleShot(100, this, [this]() {
        terminateGame();
        emit gameTerminated(); // 新增信号通知服务器
    });
}
