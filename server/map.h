#ifndef MAP_H
#define MAP_H

#include <QVector>
#include <QRect>
#include <QObject>
#include "wall.h"
#include "config/ConfigLoader.h"
#include "config/ConfigTypes.h"

class Tank;

class Map : public QObject
{
    Q_OBJECT
public:
    Map();

    /**
     * 按关卡数据装载墙体（边界 + 物块），替换掉旧的硬编码 switch-case。
     *
     * 关卡几何来自 assets/levels/*.json；物块的碰撞/贴图仍然由 `wall.h` 的 int type
     * 表达，所以这里做一次 block id → type 的映射（见 map.cpp）。物块的行为
     * （可破坏性、三项碰撞开关、地形倍率）则直接取自 config 里的 BlockDef ——
     * 关卡只描述「哪个物块摆在哪儿」。
     *
     * @return false 表示关卡里出现了 Wall 无法表达的物块 id，或引用了 blocks.json
     *         里不存在的物块（数据与引擎对不上）。此时地图为空，调用方应当放弃开局
     *         而不是让玩家掉进空地图。
     */
    bool loadLevel(const tankcity::config::LevelData &level,
                   const tankcity::config::Config &config);

    bool checkCollision(const QRect &rect) const; // 碰撞检测
    /// 子弹碰撞：返回 true 表示子弹命中并应当消失。
    /// @param damage 子弹自带的伤害，打在可破坏物块上时由这里扣除
    ///               （伤害值只由配置定义，因此随参数传入而不是写死在这里）。
    bool checkBulletCollision(const QRect &rect, int damage);   //子弹碰撞检测
    int checkTankCollision(const QRect &rect, const Tank *tank) const; //坦克碰撞检测
    const QVector<Wall>& getWalls() const { return walls; }

    static const int MAP_WIDTH=1200;
    static const int MAP_HEIGHT=900;

    static const int GRID_SIZE=40;
    int getGridWidth(){return MAP_WIDTH/GRID_SIZE;}
    int getGridHeght(){return MAP_HEIGHT/GRID_SIZE;}

    QPoint worldToGrid(const QPoint& worldPos) const;
    bool isCellWalkable(int gridX, int gridY) const;
    bool isLineWalkable(const QPoint& start, const QPoint& end) const;
    QPoint gridToWorld(const QPoint& gridPos) const;
    int getTerrainType(const QPoint &position) const;

    /**
     * 该位置的地形移动倍率，1.0 表示不受影响。
     *
     * 取自 blocks.json 的 `moveSpeedFactor`（森林 0.5、冰 1.5；0 表示该物块不影响
     * 移动）。取「首个包含该点的墙体」与旧的 getTerrainType 同序，故结果一致。
     */
    double getMoveSpeedFactor(const QPoint &position) const;

signals:
    void broadcastMessage(const QJsonObject &data); // 广播消息

private:
    QVector<Wall> walls;     //边界墙
};

#endif
