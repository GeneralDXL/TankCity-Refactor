#ifndef MAP_H
#define MAP_H

#include <QVector>
#include <QRect>
#include <QPainter>
#include <QObject>
#include "wall.h"
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
     * 表达，所以这里做一次 block id → type 的映射（见 map.cpp）。
     *
     * @return false 表示关卡里出现了 Wall 无法表达的物块 id（数据与引擎对不上），
     *         此时地图为空，调用方应当放弃开局而不是让玩家掉进空地图。
     */
    bool loadLevel(const tankcity::config::LevelData &level);

    bool checkCollision(const QRect &rect) const; // 碰撞检测
    bool checkBulletCollision(const QRect &rect);   //子弹碰撞检测
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

signals:
    void broadcastMessage(const QJsonObject &data); // 广播消息

private:
    QVector<Wall> walls;     //边界墙
};

#endif
