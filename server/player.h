#ifndef PLAYER_H
#define PLAYER_H

#include "tank.h"
#include <QPainter>
#include "bullet.h"
#include "map.h"
#include <QSet>

class Player : public Tank
{
public:
    Player(Map *gameMap, const tankcity::config::TankStats &stats); // 修改构造函数
    void init(int x, int y);
    Bullet* shoot() override;
    QRect getRect() const;
    bool canShoot() const;
    void setTurretAngle(float angle);
    void updateCooldown();
    void setBodyAngle(float angle) { bodyAngle = angle; }
    float getBodyAngle() const { return bodyAngle; }

    void setMoveForward(bool forward);
    void setMoveBackward(bool backward);
    void setTurnLeft(bool left);
    void setTurnRight(bool right);
    void setTurretTarget(const QPoint &target) { mousePos = target; }

    bool isMoveForward() const;
    bool isMoveBackward() const;
    bool isTurnLeft() const;
    bool isTurnRight() const;

    void update();

    //-------------------------------------------------加
    void setHealth(int h){health=h;}
    void setShootDelay(int delay) { shootDelay = delay; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
    void setInvincible(bool inv) { invincible = inv; }
    bool isInvincible() const { return invincible; }
    int getShootDelay(){return shootDelay;}
    int getMaxHealth() const { return maxHealth; } // 满血值（配置），治疗/血条要用
    // //-------------------------------------------------加

private:    
    QSet<int> pressedKeys;
    QPoint mousePos;  //鼠标位置
    //-------------------------------------------------加
    bool invincible = false;
    int maxHealth = 0;  // 配置里的满血值，reset 与治疗上限都用它
};
#endif
