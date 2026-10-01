#ifndef ENEMY_H
#define ENEMY_H

#include "tank.h"
#include "map.h"
#include "bullet.h"
#include "config/ConfigTypes.h"

class Enemy : public Tank
{
public:
    /**
     * @param stats 由 difficulty.json 的档位解析而来（见 tankcity::config::resolveEnemyStats）
     * @param ai    同一档位的 AI 参数（重寻路间隔、卡住阈值）—— 旧实现把这两个数
     *              写死在成员里，而配置里那份从来没被读过（M3 步 4 接线）
     */
    Enemy(Map *gameMap, const QPoint &position, int difficulty,
          const tankcity::config::TankStats &stats,
          const tankcity::config::AiDef &ai);
    void update(const QPoint &playerPos, Map *map);
    Bullet* shoot() override;
    // getRect() 由 Tank 提供：受击盒与移动探针同尺寸（M3 决议 D4）
    bool canShoot() const;
    void setBodyAngle(float angle) { bodyAngle = angle; }
    float getBodyAngle() const { return bodyAngle; }
    int getDifficulty() const { return difficulty; }
    int getId() const { return id; }
    void setId(int id) { this->id = id; }

    // 新增寻路相关方法
    /// 求一条到玩家所在格的路径并存入 `path`。
    /// 算法本身在 `game/world/pathfinder.h`（M3：从本类搬进 world 层）。
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
    /// 重新计算路径的间隔（帧）。来自 difficulty.json 的 `ai.repathIntervalTicks`
    /// （旧实现写死 60，配置里那个 60 从未被读过）。
    int repathIntervalTicks = 60;
    /// 卡住多久（帧）判定为需要重新寻路。来自 `ai.stuckThresholdTicks`（旧实现写死 15）。
    int stuckThresholdTicks = 15;
};
#endif // ENEMY_H
