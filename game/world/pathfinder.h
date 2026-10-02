#ifndef PATHFINDER_H
#define PATHFINDER_H

#include <QPoint>
#include <QVector>

class World;

/**
 * 网格寻路（八方向 A\*）。
 *
 * M3 从 `Enemy::calculatePath()` 搬出来的：那段算法原来长在实体里，用的是 `Map*`，
 * 还把「寻找最近的可行点」「路径平滑」混在同一個函数里。现在它跑在 `World`
 * （网格与物块的归属地）上，与敌人这个实体无关 —— `Enemy` 只是它的调用方之一。
 *
 * ## 保留的既有取舍（**不是**顺手该改的）
 *  - **允许斜穿墙角**：邻居只判「目标格可否通行」，不判两条正交邻格
 *    （M3 决议 D3：斜穿让路径更短，代价由移动层的不穿模来兜）。
 *  - 启发函数用**曼哈顿距离**、直行 10 / 斜行 14（与旧实现逐位一致，包括同代价
 *    路径的取舍顺序 —— 八方向偏移的枚举顺序会影响选到哪条）。
 *  - `snapToWalkable()` 在半径内找可站格的做法照搬旧实现，含它「不保证最近」
 *    的取舍（见其注释）。
 */
class PathFinder
{
public:
    /**
     * 求一条从 @p startCell 到 @p targetCell 的路径。
     *
     * @return 格子序列（网格坐标），**不含起点**、**含终点**；无路可走时返回空。
     *         起点与终点重合时返回只含终点的一格（调用方据此走向目标）。
     */
    static QVector<QPoint> findPath(const World &world,
                                    const QPoint &startCell,
                                    const QPoint &targetCell);

    /**
     * 落在不可通行格上的坐标，挪到附近能站的格。
     *
     * 起点/终点都可能落在墙里（敌人被挤进墙角、玩家贴着墙），此时直接寻路必然失败，
     * 所以先各自吸附到附近的可通行格。
     *
     * @note 返回的是**切比雪夫距离最近**的可通行格（由内向外逐圈扫，取第一个命中）。
     *       旧实现在每圈内部会让后续候选覆盖前面的，于是返回"最后一个"而非"最近的"——
     *       M3 步 4a 曾照搬它以保持提取步骤行为不变，4c 改成真正的最近格：
     *       实测表明那个偏差会放大"无敌点"（玩家贴墙站时终点被吸附到远处，
     *       敌人走到吸附点就以为到了）。
     */
    static QPoint snapToWalkable(const World &world, const QPoint &cell, int maxRadius = 5);
};

#endif
