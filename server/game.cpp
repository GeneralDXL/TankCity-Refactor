#include "game.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTimer>   // 道具时效与结算延时用的是 QTimer::singleShot（走真实时间，与帧无关）
#include "asset/AssetPaths.h"
#include "server.h"

namespace {

/// 资产根目录：统一交给 engine/asset —— 按**可执行文件所在目录**定位，不看工作目录。
/// （本函数过去手写 applicationDirPath()/../../assets，现在客户端也走同一处。）
QString assetsRoot()
{
    return engine::asset::assetRoot();
}

/**
 * 医疗包回复量。旧代码是「+30，上限 100」，即补满 30% 血；血量尺度改为
 * 10 之后同比例值就是 3（items.json 里 healthPack 的 heal.amount 也是 3）。
 *
 * 整条道具效果（含拾取时限、AmmoBoost 的减 CD、SpeedBoost 的倍率）尚未接线到
 * items.json —— 那需要先定下 ItemType ↔ item id 的映射与权重生成，属 M3。
 * 这里只把被「血量尺度变化」打破的这一处按新尺度对齐，避免出现 40/10 的血条。
 */
constexpr int kHealthPackHeal = 3;

} // namespace

bool Game::ensureConfig()
{
    if (m_configReady) return true;

    const QString configDir = QDir(assetsRoot()).filePath(QStringLiteral("config"));
    try {
        m_config = tankcity::config::ConfigLoader::loadFromDirectory(configDir);
        m_configReady = true;
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "配置加载失败（" << configDir << "）：" << e.what();
        return false;
    }
    return true;
}

bool Game::resolvePlayerStats(tankcity::config::TankStats &stats)
{
    if (!ensureConfig()) return false;

    try {
        stats = tankcity::config::resolveTankStats(m_config, QStringLiteral("player"));
        return true;
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "玩家数值解析失败：" << e.what();
        return false;
    }
}

bool Game::resolveCurrentEnemyStats(tankcity::config::TankStats &stats)
{
    if (!ensureConfig()) return false;

    // 难度号来自客户端的选档（0/1/2），对应 difficulty.json 的 order 顺序。
    // 越界在旧代码里是静默落进 switch 的 default（即不覆盖基类构造参数），
    // 现在直接报错返回，避免出现数值不明的敌人。
    if (currentDifficulty < 0 || currentDifficulty >= m_config.difficulties.size()) {
        qCritical() << "难度档" << currentDifficulty << "越界，配置里只有"
                    << m_config.difficulties.size() << "档";
        return false;
    }

    try {
        stats = tankcity::config::resolveEnemyStats(m_config,
                                                   m_config.difficulties.at(currentDifficulty));
        return true;
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "敌人数值解析失败：" << e.what();
        return false;
    }
}

bool Game::loadLevelForIndex(int index)
{
    if (!ensureConfig()) return false;

    const QString levelsDir = QDir(assetsRoot()).filePath(QStringLiteral("levels"));
    try {
        const tankcity::config::LevelData level =
            tankcity::config::ConfigLoader::loadLevelByIndex(levelsDir, index, m_config);
        return gameMap->loadLevel(level, m_config);
    } catch (const tankcity::config::ConfigError &e) {
        qCritical() << "关卡加载失败：" << e.what();
        return false;
    }
}

Game::Game()
    : gameLoop_([this] { gameLoop(); }, this)   // 循环回调指向本对象的私有槽
{
    players.clear();
    items.clear();
    enemies.clear();
    bullets.clear();

    gameMap = new Map();
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
    if (!loadLevelForIndex(this->mapIndex)) {
        qCritical() << "关卡" << this->mapIndex << "装载失败，放弃开局";
        return;
    }
    
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
    gameLoop_.start();
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
    if (!loadLevelForIndex(this->mapIndex)) {
        qCritical() << "关卡" << this->mapIndex << "装载失败，放弃开局";
        return;
    }

    score = 0;
    
    gameModle = 2;
    gameRunning = true;
    gamePaused = false;
    gameLoop_.start();

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
    if (!loadLevelForIndex(this->mapIndex)) {
        qCritical() << "关卡" << this->mapIndex << "装载失败，放弃开局";
        return;
    }
    
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
    gameLoop_.start();

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

    tankcity::config::TankStats stats;
    if (!resolvePlayerStats(stats)) {
        qCritical() << "玩家数值不可用，放弃创建玩家";
        return;
    }

    // 创建玩家坦克
    auto player = std::make_shared<Player>(gameMap, stats);
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
        gameLoop_.stop();
        gameS = false;
        multiGameOver();
    }
    
    checkCollisions();
    if(!gameRunning) return;

    broadcastGameState();
    
    // 道具系统
    itemSpawnTimer++;
    if (itemSpawnTimer >= ITEM_SPAWN_INTERVAL && items.size() < MAX_ITEMS) {
        spawnItem();
        itemSpawnTimer = 0;
    }
    checkItemCollisions();
}


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
        // 上限不能再写 100：玩家满血已由配置决定（10），否则会回出超过满血的血条。
        player->setHealth(qMin(player->getHealth() + kHealthPackHeal, player->getMaxHealth()));
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
            gameLoop_.stop();
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
            gameLoop_.stop();
            gameOver(true);
            return;
        }
        if (players.isEmpty()) {
            qDebug() << "NoNoNo";
            
            isEnd = true;
            gameLoop_.stop();
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
        
        // 检查墙壁碰撞（伤害取自子弹原型，与子弹自身 move() 里的判定同源）
        if (gameMap->checkBulletCollision(bullet->getRect(), bullet->getInjury())) {
            it = bullets.erase(it);
            continue;
        }
        // 检查玩家碰撞 (敌人子弹)
        {
            bool hitPlayer = false;
            for (auto playerIt = players.begin(); playerIt != players.end(); ++playerIt) {
                auto player = playerIt.value();
                
                // 检查无敌状态
                if (player->isInvincible()) {
                    qDebug() << "Player" << playerIt.key() << "is invincible, bullet ignored";
                    continue;
                }

                if (bullet->getRect().intersects(player->getRect())) {
                    // 伤害取自子弹原型（旧代码写死 10，等价于 1 伤 × 玩家 10 血）
                    player->takeDamage(bullet->getInjury());
                    hitPlayer = true;
                    
                    if (player->isDestroyed()) {
                        // 玩家死亡处理
                        int clientId = playerIt.key();
                        QJsonObject json;
                        json["type"] = "player_died";
                        json["id"] = clientId;
                        QJsonDocument doc(json);
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
        }
        
        if (!gameRunning) return;
        if (players.isEmpty()) return;

        // 检查敌人碰撞 (玩家子弹)
        if (bullet->getType() == BulletType::Player) {
            bool hitEnemy = false;
            
            for (auto enemy : enemies) {
                if (enemiesToRemove.contains(enemy)) continue;
                
                if (bullet->getRect().intersects(enemy->getRect())) {
                    // 伤害取自子弹原型（旧代码写死 25，等价于 1 伤 × 敌人 4/6/8 血）
                    enemy->takeDamage(bullet->getInjury());
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
                            gameLoop_.stop();
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
    
    tankcity::config::TankStats stats;
    if (!resolveCurrentEnemyStats(stats)) {
        qCritical() << "敌人数值不可用，放弃生成敌人";
        return;
    }

    auto enemy = std::make_shared<Enemy>(gameMap, QPoint(x, y), currentDifficulty, stats);
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
    
    gameRunning = false;
    
    // 安全清理顺序
    bullets.clear();    // 1. 清理子弹（可能引用其他对象）
    items.clear();      // 2. 清理道具
    
    // 3. 清理敌人（在玩家之前）
    enemies.clear();

    // 4. 清理玩家（最后，因为可能持有敌人/子弹引用）
    for (auto it = players.begin(); it != players.end();) {
        auto player = it.value();
        it = players.erase(it);
    }
    
    // 重置状态
    score = 0;
    enemySpawnTimer = 0;
    generateNum = overNum = 0;
    itemSpawnTimer = 0;
}

void Game::gameOver(bool win) {
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
