#ifndef MAP_H
#define MAP_H

#include <QJsonObject>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QVector>

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
    int getTerrainType(const QPoint &position) const { return world_.getTerrainType(position); }
    double getMoveSpeedFactor(const QPoint &position) const
    {
        return world_.getMoveSpeedFactor(position);
    }

    bool checkCollision(const QRect &rect) const { return world_.checkCollision(rect); }

    /// @param tank 透传给 `World::checkTankCollision`（目前未使用，见其说明）。
    int checkTankCollision(const QRect &rect, const Tank *tank) const
    {
        return world_.checkTankCollision(rect, tank);
    }

    const QVector<Wall> &getWalls() const { return world_.getWalls(); }

    /**
     * 几何层的只读引用。
     *
     * 给「按 World 工作」的地方用 —— 目前是 `PathFinder`（A\* 跑在网格上）。
     * 实体签名迁到 `World*` 之前，这是 `Map*` 持有者取到几何层的通路。
     */
    const World &world() const { return world_; }

    // --- 规则：只有这一条不属于几何 ---

    /// 子弹碰撞：返回 true 表示子弹命中并应当消失。
    /// @param damage 子弹自带的伤害，打在可破坏物块上时由这里扣除
    ///               （伤害值只由配置定义，因此随参数传入而不是写死在这里）。
    bool checkBulletCollision(const QRect &rect, int damage);   //子弹碰撞检测

signals:
    void broadcastMessage(const QJsonObject &data); // 广播消息

private:
    World world_;
};

#endif
