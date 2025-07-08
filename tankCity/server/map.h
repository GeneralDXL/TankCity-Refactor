#ifndef MAP_H
#define MAP_H

#include <QVector>
#include <QRect>
#include <QPainter>
#include <QObject>
#include "wall.h"

class Tank;

class Map : public QObject
{
    Q_OBJECT
public:
    Map();
    void loadMap(int index);
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
    int mapIndex;
};

#endif
