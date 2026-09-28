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
    void draw(QPainter &painter);
    bool checkCollision(const QRect &rect) const; // 碰撞检测
    bool checkBulletCollision(const QRect &rect);   //子弹碰撞检测
    int checkTankCollision(const QRect &rect, const Tank *tank) const; //坦克碰撞检测
    const QVector<Wall>& getWalls() const { return walls; }

signals:
    void broadcastMessage(const QJsonObject &data); // 广播消息

private:
    QVector<Wall> walls;     //边界墙
    int mapIndex;
};

#endif
