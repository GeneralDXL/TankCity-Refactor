#ifndef MAP_H
#define MAP_H

#include <QJsonObject>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QVector>

#include "bullet.h"   // BulletHit（命中判定的结果，M4 步 2）
#include "wall.h"
#include "world.h"

class Tank;

/**
 * 地图：**游戏规则**的那一半 —— 物块被打中之后扣血、销毁、广播。
 *
 * M3 把「几何」拆给了 `World`（见 world.h）。于是 `Map` 只剩两件事：
 *  1. 把几何问题**整层转发**给 `World`（既有调用点因此一行不改）；
 *  2. 把规则问题留在自己身上：子弹命中 → 扣血 → 血量归零则广播 `delete_wall` 并移除物块。
 *
 * 本类**是** `QObject` 且持有信号 —— 发消息正是它存在的理由，与 `World`
 * 「连 QObject 都不是」形成对照。（拆分判据见 world.h 的类注释。）
 *
 * ## 为什么保留这层转发
 * `Tank`/`Player`/`Enemy`/`Game` 的签名里写的都是 `Map *`（`Tank::move(..., Map *)`、
 * `Enemy::update(..., Map *)`……）。一步到位改签名，会把「拆出 World」与「改实体接口」
 * 混进同一个提交里，一旦出问题无法二分。等 tile 世界与双摇杆真正落到 `World` 上
 * （M3 后续步骤）时再迁移 —— 那时这些签名本来就要重写。
 */
class Map : public QObject
{
    Q_OBJECT
public:
    Map();

    // --- 几何：整层转发给 World ---

    /**
     * 按关卡数据装载墙体。语义与返回值的说明见 `World::loadLevel`。
     * @return false 表示关卡数据与引擎对不上（物块 id 无法表达 / blocks.json 里没有）
     */
    bool loadLevel(const tankcity::config::LevelData &level,
                   const tankcity::config::Config &config);

    static const int MAP_WIDTH = World::MAP_WIDTH;
    static const int MAP_HEIGHT = World::MAP_HEIGHT;
    static const int GRID_SIZE = World::GRID_SIZE;

    int getGridWidth() const { return world_.getGridWidth(); }
    int getGridHeight() const { return world_.getGridHeight(); }

    QPoint worldToGrid(const QPoint &worldPos) const { return world_.worldToGrid(worldPos); }
    QPoint gridToWorld(const QPoint &gridPos) const { return world_.gridToWorld(gridPos); }
    bool isCellWalkable(int gridX, int gridY) const { return world_.isCellWalkable(gridX, gridY); }
    bool isLineWalkable(const QPoint &start, const QPoint &end) const
    {
        return world_.isLineWalkable(start, end);
    }

    /// 视线（M4 步 3）：森林**挡**、海**不挡**。
    /// 敌怪开火与索敌走它 —— 与 `isLineWalkable` 是两件事（后者还被 A\* 路径平滑用着）。
    bool isLineOfSight(const QPoint &start, const QPoint &end) const
    {
        return world_.isLineOfSight(start, end);
    }
    double getMoveSpeedFactor(const QPoint &position) const
    {
        return world_.getMoveSpeedFactor(position);
    }

    /// 该位置的移动模型（普通 / 惯性）—— 见 `World::getMovementAt`（M4 步 4）。
    tankcity::config::MovementDef getMovementAt(const QPoint &position) const
    {
        return world_.getMovementAt(position);
    }

    bool checkCollision(const QRect &rect) const { return world_.checkCollision(rect); }

    /// 出生点用：离 preferred 最近的可通行格中心（见 `World::nearestWalkableCenter`）。
    QPoint nearestWalkableCenter(const QPoint &preferred) const
    {
        return world_.nearestWalkableCenter(preferred);
    }

    /// 这个矩形所在位置有没有挡坦克的物块（见 `World::blocksTankAt`）。
    bool blocksTankAt(const QRect &rect) const { return world_.blocksTankAt(rect); }

    const QVector<Wall> &getWalls() const { return world_.getWalls(); }

    /**
     * 几何层的只读引用。
     *
     * 给「按 World 工作」的地方用 —— 目前是 `PathFinder`（A\* 跑在网格上）。
     * 实体签名迁到 `World*` 之前，这是 `Map*` 持有者取到几何层的通路。
     */
    const World &world() const { return world_; }

    // --- 规则：只有这一条不属于几何 ---

    /// 子弹碰撞：**只回答发生了什么**，是否消失由子弹自己决定（M4 步 2）。
    ///
    /// 返回 `BulletHit`：有没有撞到、是不是可弹物块、撞在哪一面、物块矩形。
    /// 之所以不再是 `bool`：反弹需要"判定方给证据、子弹做决策" ——
    /// `Map` 知道撞到了什么，子弹知道自己还剩几次弹数。
    ///
    /// 规则要点：
    /// - **打不动也算命中** ✓（普通弹打钢材不扣血，但子弹照样消失 —— 手感上就是"打在钢板上"）；
    /// - 物块要求 `requiredBulletTags` 时，子弹必须带其中一个标签才扣得动（钢材要 `armorPiercing`）；
    /// - 物块在 `profile.bounceOn` 里时**不吃伤害**，只报告"可弹"（手稿：弹射仅限钢材与边界，
    ///   击中砖块等走"破坏"这条路）。
    BulletHit checkBulletCollision(const QRect &rect,
                                   const tankcity::config::BulletProfile &profile);

signals:
    void broadcastMessage(const QJsonObject &data); // 广播消息

private:
    World world_;
};

#endif
