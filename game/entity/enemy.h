#ifndef ENEMY_H
#define ENEMY_H

#include "tank.h"
#include "map.h"
#include "bullet.h"
#include "config/ConfigTypes.h"

#include <limits>

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

    /// 求一条到玩家所在格的路径并存入 `path`。
    /// 算法本身在 `game/world/pathfinder.h`（M3 步 4：从本类搬进 world 层）。
    void calculatePath(const QPoint& playerGridPos);

    /// 连续多少帧没能更接近当前路径点（进度判据，见 advanceTowards）
    int stuckTimer = 0;

private:
    static int ID;

    /// 朝 targetWorld 走一帧；被挡就按固定顺序偏转（贴墙滑动，带方向记忆）。
    void advanceTowards(const QPoint &targetWorld, Map *map);
    /// 换了一个路径点，进度基线重置
    void resetWaypointProgress();

    int difficulty;
    int moveTimer;
    int changeDirectionTimer;
    int shootTimer;
    int id;

    // 寻路相关
    QVector<QPoint> path;          // 存储路径点（网格坐标）
    int currentPathIndex = -1;     // 当前路径点索引
    int recalculatePathTimer = 0;  // 路径重新计算计时器
    /// 重新计算路径的间隔（帧）。来自 difficulty.json 的 `ai.repathIntervalTicks`
    /// （旧实现写死 60，配置里那个 60 从未被读过）。
    int repathIntervalTicks = 60;
    /// 卡住多久（帧）判定为需要重新寻路。来自 `ai.stuckThresholdTicks`（旧实现写死 15）。
    int stuckThresholdTicks = 15;

    /// 最近一次 update() 收到的玩家位置。`canShoot()` 用它做视线判定，
    /// 而 `Game` 每次都是先 update() 再问 canShoot()，所以它总是当帧的。
    QPoint lastKnownPlayerPos;

    /// 距当前路径点"没有更近"的连续帧数（进度判据）
    int progressTimer = 0;
    /// 上次成功偏转的方向侧：+1 右、-1 左、0 未知/直行。用来保证滑动不来回翻转。
    int slideSign = 0;
    /// 走当前路径点过程中达到过的最近距离
    double bestDistanceToWaypoint = std::numeric_limits<double>::max();
};
#endif // ENEMY_H
