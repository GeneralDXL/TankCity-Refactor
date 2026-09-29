#ifndef TANK_H
#define TANK_H

#include "map.h"
#include "bullet.h"
#include "config/TankStats.h"
#include <QVector2D>
#include <QObject>

enum Direction {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3
};

class Tank
{
public:
    /// 数值一律来自 assets/config/entities.json（+ difficulty.json 的难度覆盖），
    /// 见 tankcity::config::TankStats。
    Tank(Map *gameMap, const tankcity::config::TankStats &stats);
    virtual ~Tank() = default;

    virtual void move(float angle, float distance, Map *map); // 修改移动函数
    bool canMove(float angle, float distance, Map *map) const; // 修改移动检测
    virtual Bullet* shoot() = 0;
    void takeDamage(int amount);
    bool isDestroyed() const;
    int getHealth() const;
    QPoint getPosition() const;
    float getSpeed() const { return speed; } // 速度获取
    void setSpeed(float newSpeed) { speed = newSpeed; } // 速度设置
    int getBulletDamage() const { return bulletDamage; } // 子弹伤害（配置）

    void setPosition(const QPoint& pos) { position = pos; }
    void setBodyAngle(float angle) { bodyAngle = angle; } // 车身角度设置
    float getBodyAngle() const { return bodyAngle; } // 车身角度获取

    // 添加获取方向函数
    Direction getDirection() const { return direction; }
    float getTurretAngle() const { return turretAngle; }

protected:
    QPoint position;
    Direction direction;
    float bodyAngle = 0; // 车身角度
    int health;
    float speed;
    double bulletSpeed = 0.0;  // 子弹速度（像素/帧）
    int bulletDamage = 1;      // 子弹伤害
    int muzzleOffset = 0;      // 炮口相对车体中心的距离（像素）
    int shootCooldown;
    int shootDelay;
    float turretAngle = 0;  // 炮管角度
    Map *gameMap;
};


#endif
