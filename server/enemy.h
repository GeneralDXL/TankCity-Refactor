#ifndef ENEMY_H
#define ENEMY_H

#include "tank.h"
#include "map.h"
#include "bullet.h"

// 添加前向声明
class GameWindow;

class Enemy : public Tank
{
public:
    /// 数值由 difficulty.json 的档位解析而来（见 tankcity::config::resolveEnemyStats）。
    Enemy(Map *gameMap, const QPoint &position, int difficulty,
          const tankcity::config::TankStats &stats);
    void update(const QPoint &playerPos, Map *map);
    Bullet* shoot() override;
    QRect getRect() const;
    bool canShoot() const;
    void setBodyAngle(float angle) { bodyAngle = angle; }
    float getBodyAngle() const { return bodyAngle; }
    int getDifficulty() const { return difficulty; }
    int getId() const { return id; }
    void setId(int id) { this->id = id; }

        // 新增寻路相关方法
    void calculatePath(const QPoint& playerGridPos);
    bool isSafePosition(const QPoint& worldPos) const;
    int stuckTimer = 0;
    float calculateObstacleDistance(const QPoint& pos, Map* map) const;

private:
    static int ID;

    int difficulty;
    int moveTimer;
    int changeDirectionTimer;
    int shootTimer;
    int id;

    // 新增寻路相关成员变量
    QVector<QPoint> path;          // 存储路径点（网格坐标）
    int currentPathIndex = -1;     // 当前路径点索引
    int recalculatePathTimer = 0;  // 路径重新计算计时器
    const int RECALCULATE_PATH_INTERVAL = 60; // 降低为80帧重新计算路径
    const int MAX_STUCK_TIME = 15; // 降低卡住判定阈值
};
#endif // ENEMY_H
