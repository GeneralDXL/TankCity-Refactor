#ifndef ENEMY_H
#define ENEMY_H

#include "tank.h"
#include <QPainter>
#include "map.h"
#include "bullet.h"

// 添加前向声明
class GameWindow;

class Enemy : public Tank
{
public:
    Enemy(Map *gameMap, const QPoint &position, int difficulty);
    void update(const QPoint &playerPos, Map *map);
    void draw(QPainter &painter) override;
    Bullet* shoot() override;
    QRect getRect() const;
    bool canShoot() const;
    void setBodyAngle(float angle) { bodyAngle = angle; }
    float getBodyAngle() const { return bodyAngle; }
    int getDifficulty() const { return difficulty; }

private:
    int difficulty;
    int moveTimer;
    int changeDirectionTimer;
    int shootTimer;
};
#endif // ENEMY_H
