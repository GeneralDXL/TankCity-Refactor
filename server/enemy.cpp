#include "enemy.h"
#include "game.h"
#include <QPainter>
#include <queue>
#include <cmath>
#include <QRandomGenerator>
#include <QRectF>
#include <queue>
#include <set>

int Enemy::ID = 0;


Enemy::Enemy(Map *gameMap, const QPoint &position, int difficulty,
             const tankcity::config::TankStats &stats)
    : Tank(gameMap, stats),       // 调用基类构造函数
    difficulty(difficulty)         // 初始化派生类成员
{
    this->position = position;
    direction = static_cast<Direction>(QRandomGenerator::global()->bounded(4));
    moveTimer = 0;
    changeDirectionTimer = 0;
    shootTimer = 0;

    // 速度 / 射击间隔 / 血量一律来自 difficulty.json 的对应档位（含 overrides），
    // 见 tankcity::config::resolveEnemyStats()；难度号只作为标识保留（协议与贴图要用）。

    id = ++ID;
}

bool Enemy::canShoot() const
{
    return shootCooldown == 0 && shootTimer == 0;
}

// 检查位置是否安全（不会卡墙）
bool Enemy::isSafePosition(const QPoint& worldPos) const {
    if (!gameMap) return true;

    // 扩大检测区域，避免边缘卡顿
    QRect testRect(worldPos.x() - 12, worldPos.y() - 12, 24, 24);

    // 检查与墙的碰撞
    for (const Wall &wall : gameMap->getWalls()) {
        // 跳过森林和冰块
        if (wall.getType() == FOREST || wall.getType() == ICE) {
            continue;
        }

        // 检查海洋、边界、砖墙和钢墙
        if (wall.getRect().intersects(testRect)) {
            // 海洋和不可穿越的墙
            if (wall.getType() == SEA || wall.getType() == BOUNDARY ||
                wall.getType() == BRICK || wall.getType() == STEEL) {
                return false;
            }
        }
    }

    // 额外检查：确保位置在有效地图范围内
    if (worldPos.x() < 20 || worldPos.y() < 20 ||
        worldPos.x() > 1200 - 20 ||
        worldPos.y() > 900 - 20) {
        return false;
    }

    return true;
}


// A*寻路节点结构
struct Node {
    QPoint gridPos;
    int g; // 从起点到当前节点的代价
    int h; // 从当前节点到目标的预估代价
    Node* parent;

    Node(QPoint pos, int gCost, int hCost, Node* p = nullptr)
        : gridPos(pos), g(gCost), h(hCost), parent(p) {}

    int f() const { return g + h; }

    // 用于优先队列的比较
    bool operator>(const Node& other) const {
        return f() > other.f();
    }
};

struct QPointCompare {
    bool operator()(const QPoint& a, const QPoint& b) const {
        if (a.x() != b.x()) return a.x() < b.x();
        return a.y() < b.y();
    }
};

void Enemy::calculatePath(const QPoint& playerGridPos)
{
    path.clear();
    currentPathIndex = -1;

    if (!gameMap) return;

    QPoint startGrid = gameMap->worldToGrid(position);
    QPoint targetGrid = playerGridPos;

    // 如果起点或终点不可通行，尝试寻找最近的可行点
    if (!gameMap->isCellWalkable(startGrid.x(), startGrid.y())) {
        // 寻找最近的可行点
        for (int r = 1; r <= 5; r++) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dy = -r; dy <= r; dy++) {
                    QPoint testPoint(startGrid.x() + dx, startGrid.y() + dy);
                    if (gameMap->isCellWalkable(testPoint.x(), testPoint.y())) {
                        startGrid = testPoint;
                        break;
                    }
                }
            }
        }
    }

    if (!gameMap->isCellWalkable(targetGrid.x(), targetGrid.y())) {
        // 寻找最近的可行点
        for (int r = 1; r <= 5; r++) {
            for (int dx = -r; dx <= r; dx++) {
                for (int dy = -r; dy <= r; dy++) {
                    QPoint testPoint(targetGrid.x() + dx, targetGrid.y() + dy);
                    if (gameMap->isCellWalkable(testPoint.x(), testPoint.y())) {
                        targetGrid = testPoint;
                        break;
                    }
                }
            }
        }
    }

    // 如果已经在目标位置
    if (startGrid == targetGrid) {
        path.append(targetGrid);
        currentPathIndex = 0;
        return;
    }

    // 定义优先队列（最小堆）
    auto cmp = [](const Node* a, const Node* b) {
        if (a->f() == b->f()) return a->h > b->h;
        return a->f() > b->f();
    };
    std::priority_queue<Node*, std::vector<Node*>, decltype(cmp)> openSet(cmp);
    std::set<QPoint, QPointCompare> closedSet;
    std::unordered_map<int, Node*> allNodes;

    // 起点节点
    int startKey = startGrid.x() * 10000 + startGrid.y();
    Node* startNode = new Node(startGrid, 0,
                               abs(startGrid.x() - targetGrid.x()) + abs(startGrid.y() - targetGrid.y()));
    openSet.push(startNode);
    allNodes[startKey] = startNode;

    // 八方向移动的偏移量
    const QPoint directions[] = {
        QPoint(0, -1),  // 上
        QPoint(1, 0),   // 右
        QPoint(0, 1),   // 下
        QPoint(-1, 0),  // 左
        QPoint(-1, -1), // 左上
        QPoint(1, -1),  // 右上
        QPoint(1, 1),   // 右下
        QPoint(-1, 1)   // 左下
    };

    Node* targetNode = nullptr;

    while (!openSet.empty()) {
        Node* currentNode = openSet.top();
        openSet.pop();

        // 如果找到目标
        if (currentNode->gridPos == targetGrid) {
            targetNode = currentNode;
            break;
        }

        closedSet.insert(currentNode->gridPos);

        // 检查所有方向
        for (const auto& dir : directions) {
            QPoint neighborPos = currentNode->gridPos + dir;

            // 检查邻居是否在网格范围内
            if (neighborPos.x() < 0 || neighborPos.y() < 0 ||
                neighborPos.x() >= 30 ||
                neighborPos.y() >= 22.5) {
                continue;
            }

            // 检查邻居是否已在关闭列表
            if (closedSet.find(neighborPos) != closedSet.end()) {
                continue;
            }

            // 检查邻居是否可通行
            if (!gameMap->isCellWalkable(neighborPos.x(), neighborPos.y())) {
                continue;
            }

            // 计算移动代价（对角线移动代价稍高）
            int moveCost = (abs(dir.x()) + abs(dir.y()) == 2 ? 14 : 10);
            int newG = currentNode->g + moveCost;
            int newH = abs(neighborPos.x() - targetGrid.x()) +
                       abs(neighborPos.y() - targetGrid.y());

            // 检查邻居是否已在开放列表中
            int neighborKey = neighborPos.x() * 10000 + neighborPos.y();
            Node* neighborNode = nullptr;
            if (allNodes.find(neighborKey) != allNodes.end()) {
                neighborNode = allNodes[neighborKey];
            }

            // 如果不在开放列表或找到更短路径
            if (!neighborNode || newG < neighborNode->g) {
                if (!neighborNode) {
                    neighborNode = new Node(neighborPos, newG, newH, currentNode);
                    openSet.push(neighborNode);
                    allNodes[neighborKey] = neighborNode;
                } else {
                    neighborNode->g = newG;
                    neighborNode->parent = currentNode;
                    // 需要重新加入优先队列以更新位置
                    openSet.push(neighborNode);
                }
            }
        }
    }

    // 如果找到路径，回溯并存储
    if (targetNode) {
        Node* node = targetNode;
        while (node) {
            path.prepend(node->gridPos);
            node = node->parent;
        }

        // 删除起点（已经是当前位置）
        if (path.size() > 1) {
            path.removeFirst();
        }

        currentPathIndex = 0;
    }

    // 清理内存
    for (auto& pair : allNodes) {
        delete pair.second;
    }

    // 路径平滑处理 - 简化路径
    if (path.size() > 2) {
        QVector<QPoint> simplifiedPath;
        simplifiedPath.append(path.first());

        int lastValidIndex = 0;
        for (int i = 1; i < path.size() - 1; i++) {
            // 检查直线是否畅通
            if (!gameMap->isLineWalkable(simplifiedPath.last(), path[i+1])) {
                // 添加一个中间点避免直线障碍
                if (i - lastValidIndex > 1) {
                    QPoint midPoint = (simplifiedPath.last() + path[i]) / 2;
                    if (gameMap->isCellWalkable(midPoint.x(), midPoint.y())) {
                        simplifiedPath.append(midPoint);
                    }
                }
                simplifiedPath.append(path[i]);
                lastValidIndex = i;
            }
        }
        simplifiedPath.append(path.last());

        path = simplifiedPath;
    }
}

float Enemy::calculateObstacleDistance(const QPoint& pos, Map* map) const
{
    float minDistance = std::numeric_limits<float>::max();
    QRect tankRect(pos.x() - 15, pos.y() - 15, 30, 30);

    for (const Wall &wall : map->getWalls()) {
        // 跳过可穿越的地形
        if (wall.getType() == FOREST || wall.getType() == ICE) {
            continue;
        }

        if (wall.getRect().intersects(tankRect)) {
            // 计算到障碍物的距离
            QRect wallRect = wall.getRect();
            QPoint center(pos.x(), pos.y());
            QPoint wallCenter(wallRect.center());

            float distance = std::sqrt(std::pow(center.x() - wallCenter.x(), 2) +
                                       std::pow(center.y() - wallCenter.y(), 2));

            if (distance < minDistance) {
                minDistance = distance;
            }
        }
    }

    return (minDistance == std::numeric_limits<float>::max()) ? 1000.0f : minDistance;
}


void Enemy::update(const QPoint &playerPos, Map *map)
{
    if (isDestroyed()) return;

    // 更新冷却时间
    if (shootCooldown > 0) shootCooldown--;
    if (shootTimer > 0) shootTimer--;

    // 更新炮塔角度指向玩家
    QPointF directionVec = playerPos - position;
    turretAngle = qRadiansToDegrees(qAtan2(directionVec.y(), directionVec.x()));

    // 更新路径重新计算计时器
    recalculatePathTimer--;
    if (recalculatePathTimer <= 0 || path.isEmpty() || stuckTimer > MAX_STUCK_TIME) {
        recalculatePathTimer = RECALCULATE_PATH_INTERVAL;
        stuckTimer = 0;
        calculatePath(map->worldToGrid(playerPos));
    }

    // 如果有路径，沿路径移动
    if (currentPathIndex >= 0 && currentPathIndex < path.size()) {
        QPoint targetWorldPos = map->gridToWorld(path[currentPathIndex]);
        QPointF directionToTarget = targetWorldPos - position;
        float distanceToTarget = sqrt(directionToTarget.x() * directionToTarget.x() +
                                      directionToTarget.y() * directionToTarget.y());

        // 如果接近当前路径点，移动到下一个点
        if (distanceToTarget < speed * 2) {
            currentPathIndex++;
            if (currentPathIndex >= path.size()) {
                calculatePath(map->worldToGrid(playerPos));
                return;
            }
            targetWorldPos = map->gridToWorld(path[currentPathIndex]);
            directionToTarget = targetWorldPos - position;
            distanceToTarget = sqrt(directionToTarget.x() * directionToTarget.x() +
                                    directionToTarget.y() * directionToTarget.y());
        }

        // 计算移动方向
        if (distanceToTarget > 0) {
            float moveX = (directionToTarget.x() / distanceToTarget) * speed;
            float moveY = (directionToTarget.y() / distanceToTarget) * speed;
            // 更新车身角度
            bodyAngle = qRadiansToDegrees(qAtan2(moveY, moveX));

            // 应用地形效果：倍率来自 blocks.json 的 moveSpeedFactor
            // （旧代码这里森林写 0.75、玩家侧写 0.5，同一地形两套数值，M2 起统一；
            //   无地形影响时为 1.0，乘 1.0f 是精确的，与旧写法一致）
            const float scale = static_cast<float>(map->getMoveSpeedFactor(position));
            moveX *= scale;
            moveY *= scale;

            QPoint newPos = position + QPoint(moveX, moveY);

            // 检查新位置是否安全
            if (isSafePosition(newPos)) {
                position = newPos;
                stuckTimer = 0;
            } else {
                // ==== 改进的避障逻辑 - 尝试16个方向 ====
                QPoint bestPos = position;
                float maxObstacleDistance = -1;
                bool foundPath = false;

                // 尝试16个方向 (22.5度间隔)
                for (int i = 0; i < 16; i++) {
                    float angle = i * 22.5f;
                    float rad = qDegreesToRadians(angle);
                    QPoint testPos = position + QPoint(cos(rad) * speed * 1.5,
                                                       sin(rad) * speed * 1.5);

                    if (isSafePosition(testPos)) {
                        // 计算到障碍物的距离
                        float obstacleDist = calculateObstacleDistance(testPos, map);

                        if (obstacleDist > maxObstacleDistance) {
                            maxObstacleDistance = obstacleDist;
                            bestPos = testPos;
                            foundPath = true;
                        }
                    }
                }

                // 尝试后退方向
                if (!foundPath) {
                    float backAngle = bodyAngle + 180.0f;
                    float rad = qDegreesToRadians(backAngle);
                    QPoint testPos = position + QPoint(cos(rad) * speed * 1.5,
                                                       sin(rad) * speed * 1.5);
                    if (isSafePosition(testPos)) {
                        bestPos = testPos;
                        foundPath = true;
                    }
                }

                if (foundPath) {
                    position = bestPos;
                    stuckTimer = 0;
                    // 更新车身角度
                    QPoint moveVec = bestPos - position;
                    bodyAngle = qRadiansToDegrees(qAtan2(moveVec.y(), moveVec.x()));
                } else {
                    stuckTimer++;
                    // 如果卡住时间过长，重新计算路径
                    if (stuckTimer > MAX_STUCK_TIME/2) {
                        recalculatePathTimer = 0;
                    }
                }
            }
        }
    } else {
        // ====== 增强的无路径移动策略 ======
        // 地形倍率同样来自配置（森林由旧代码的 0.75 统一为 0.5）
        const float actualSpeed =
            speed * static_cast<float>(map->getMoveSpeedFactor(position));

        // 计算指向玩家的方向
        QPointF directionToPlayer = playerPos - position;
        float distanceToPlayer = sqrt(directionToPlayer.x() * directionToPlayer.x() +
                                      directionToPlayer.y() * directionToPlayer.y());
        if (distanceToPlayer > 0) {
            float moveX = (directionToPlayer.x() / distanceToPlayer) * actualSpeed;
            float moveY = (directionToPlayer.y() / distanceToPlayer) * actualSpeed;

            // 更新车身角度
            bodyAngle = qRadiansToDegrees(qAtan2(moveY, moveX));

            QPoint newPos = position + QPoint(moveX, moveY);
            // 新增：如果直接路径受阻，尝试16个方向
            if (!isSafePosition(newPos)) {
                // 尝试16个方向 (22.5度间隔)
                for (int i = 0; i < 16; i++) {
                    float angle = i * 22.5f;
                    float rad = qDegreesToRadians(angle);
                    QPoint testPos = position + QPoint(cos(rad) * actualSpeed * 1.5,
                                                       sin(rad) * actualSpeed * 1.5);

                    if (isSafePosition(testPos)) {
                        newPos = testPos;
                        // 更新车身角度
                        QPoint moveVec = testPos - position;
                        bodyAngle = qRadiansToDegrees(qAtan2(moveVec.y(), moveVec.x()));
                        break;
                    }
                }
            }

            // 检查新位置是否安全
            if (isSafePosition(newPos)) {
                position = newPos;
            }
        }
    }

    // 射击逻辑（保持原样）
    if (QRandomGenerator::global()->bounded(100) < 40 && canShoot()) {
        shoot();
    }
}

Bullet* Enemy::shoot()
    {
        if (shootCooldown > 0 || shootTimer > 0) return nullptr;

        shootCooldown = 100;
        shootTimer = shootDelay;

        // 根据炮管角度计算子弹位置
        float rad = qDegreesToRadians(turretAngle);
        QPoint bulletPos = position + QPoint(muzzleOffset * cos(rad), muzzleOffset * sin(rad));

        // 获取地图指针（确保回调函数不依赖敌人实例）
        Map* mapPtr = this->gameMap;

        // 创建新子弹（使用地图指针而非this；速度与伤害来自 entities.json）
        return new Bullet(bulletPos, static_cast<int>(turretAngle), BulletType::Enemy,
                          bulletSpeed, bulletDamage,
                          [mapPtr](const QRect& rect, int damage) {  // 修改：捕获地图指针
                              return mapPtr->checkBulletCollision(rect, damage);
                          });
    }

QRect Enemy::getRect() const
{
    return QRect(position.x() - 15, position.y() - 15, 30, 30);
}




