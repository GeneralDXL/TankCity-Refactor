#include "enemy.h"
#include "pathfinder.h"   // A* 已搬进 game/world（M3 步 4）
#include <QtMath>
#include <cmath>
#include <QRandomGenerator>
#include <QRectF>

int Enemy::ID = 0;


Enemy::Enemy(Map *gameMap, const QPoint &position, int difficulty,
             const tankcity::config::TankStats &stats,
             const tankcity::config::AiDef &ai)
    : Tank(gameMap, stats),       // 调用基类构造函数
    difficulty(difficulty)         // 初始化派生类成员
{
    this->position = position;
    direction = static_cast<Direction>(QRandomGenerator::global()->bounded(4));
    moveTimer = 0;
    changeDirectionTimer = 0;
    shootTimer = 0;

    // AI 节奏来自 difficulty.json 的 ai 段（M3 步 4 接线；此前写死在头文件里）
    repathIntervalTicks = ai.repathIntervalTicks;
    stuckThresholdTicks = ai.stuckThresholdTicks;

    // 速度 / 射击间隔 / 血量一律来自 difficulty.json 的对应档位（含 overrides），
    // 见 tankcity::config::resolveEnemyStats()；难度号只作为标识保留（协议与贴图要用）。

    id = ++ID;
}

bool Enemy::canShoot() const
{
    return shootCooldown == 0 && shootTimer == 0;
}



void Enemy::calculatePath(const QPoint& playerGridPos)
{
    path.clear();
    currentPathIndex = -1;

    if (!gameMap)
        return;

    // 路径由 world 层的 PathFinder 求（M3 步 4：A* 从本类搬进 game/world）。
    // 本函数只负责"把结果存下来" —— 起点/终点的吸附、斜穿取舍、路径平滑
    // 都在 PathFinder 里（见 game/world/pathfinder.h）。
    path = PathFinder::findPath(gameMap->world(),
                                gameMap->worldToGrid(position),
                                playerGridPos);

    if (!path.isEmpty())
        currentPathIndex = 0;
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
    if (recalculatePathTimer <= 0 || path.isEmpty() || stuckTimer > stuckThresholdTicks) {
        recalculatePathTimer = repathIntervalTicks;
        stuckTimer = 0;
        slideSign = 0;              // 新路径：滑动方向重新学
        resetWaypointProgress();
        calculatePath(map->worldToGrid(playerPos));
    }

    // ---- 沿路径推进 ----
    // 目标点：路径上的当前格；路径为空时退回"直接朝玩家"（旧行为保留，但碰撞判定
    // 改由 advanceTowards 统一处理）。
    QPoint aimPos = playerPos;
    if (currentPathIndex >= 0 && currentPathIndex < path.size()) {
        aimPos = map->gridToWorld(path[currentPathIndex]);

        QPointF toTarget = QPointF(aimPos) - QPointF(position);
        const double distanceToTarget = std::hypot(toTarget.x(), toTarget.y());

        // 到点就推进到下一个路径点（阈值沿用旧的 speed * 2）
        if (distanceToTarget < speed * 2) {
            currentPathIndex++;
            if (currentPathIndex >= path.size()) {
                calculatePath(map->worldToGrid(playerPos));
                return;
            }
            aimPos = map->gridToWorld(path[currentPathIndex]);
            resetWaypointProgress();
        }
    }

    advanceTowards(aimPos, map);
    // 射击逻辑（保持原样）
    if (QRandomGenerator::global()->bounded(100) < 40 && canShoot()) {
        shoot();
    }
}

void Enemy::resetWaypointProgress()
{
    bestDistanceToWaypoint = std::numeric_limits<double>::max();
    progressTimer = 0;
}

void Enemy::advanceTowards(const QPoint &targetWorld, Map *map)
{
    const QPointF delta = QPointF(targetWorld) - QPointF(position);
    const double distance = std::hypot(delta.x(), delta.y());
    if (distance <= 0.0)
        return;

    // ---- 进度判据：本步的关键修复之一 ----
    // 判"卡住"不能看"这一帧动没动"，要看"离目标点有没有更近"。
    // 旧实现是前者：只要 16 向避障里挑到一个能站的位置就把 stuckTimer 清零，
    // 于是敌人在墙角来回挪**永远不算卡住**，也就永远不会重新寻路 ——
    // 这正是贴角抽搐可以无限持续下去的原因。改成看进度之后，
    // "原地来回挪"会在 stuckThresholdTicks 帧内被判为卡住并触发重寻路。
    if (distance < bestDistanceToWaypoint - 0.5) {
        bestDistanceToWaypoint = distance;
        progressTimer = 0;
    } else if (++progressTimer > stuckThresholdTicks) {
        recalculatePathTimer = 0;   // 下一帧重新寻路
        resetWaypointProgress();
        slideSign = 0;
        return;
    }

    const QPointF dir = delta / distance;
    const double desiredAngle = qRadiansToDegrees(qAtan2(dir.y(), dir.x()));

    // ---- 贴墙滑动：确定顺序 + 带记忆（本步的关键修复之二）----
    // 0（直行）永远排第一；其余偏转角按"上次成功的那一侧优先"排。
    // 记忆是重点：旧实现每帧在 16 个方向里重挑"离障碍最远"的那个，
    // 位置稍变就可能换一个方向，于是贴角时左右横跳。这里只会一直往同一侧滑。
    const double preferred = (slideSign < 0) ? -1.0 : 1.0;
    const double offsets[7] = {0.0,
                               preferred * 30.0, preferred * 60.0, preferred * 90.0,
                               -preferred * 30.0, -preferred * 60.0, -preferred * 90.0};

    const QPoint before = position;
    for (double offset : offsets) {
        // 走共享的移动判定（Tank::move）：带扫掠、带地形倍率、用配置的碰撞盒。
        // 旧实现绕开了它直接改 position，所以 3b 的扫掠对敌人根本没生效。
        move(static_cast<float>(desiredAngle + offset), speed, map);
        if (position == before)
            continue;   // 这个方向被挡下了，试下一个

        // 车体朝向 = **实际位移**方向。
        // 旧实现在避障分支里算的是 `bestPos - position`，而 position 在那之前
        // 已被赋值成 bestPos，于是恒为 atan2(0, 0) = 0 —— 一避障车头就朝右跳。
        const QPoint moved = position - before;
        bodyAngle = static_cast<float>(
            qRadiansToDegrees(qAtan2(static_cast<double>(moved.y()), static_cast<double>(moved.x()))));

        // 记住这次是往哪一侧偏的（直行成功时保留原有记忆）
        if (offset > 0.0)
            slideSign = 1;
        else if (offset < 0.0)
            slideSign = -1;
        return;
    }

    // 七个方向全被挡：真的被困住。缩短重寻路间隔（沿用旧的 stuckTimer 语义）
    stuckTimer++;
    if (stuckTimer > stuckThresholdTicks / 2)
        recalculatePathTimer = 0;
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




