#ifndef TANK_H
#define TANK_H

#include "map.h"
#include "bullet.h"
#include "config/TankStats.h"
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
    virtual Bullet* shoot() = 0;
    void takeDamage(int amount);
    bool isDestroyed() const;
    int getHealth() const;
    QPoint getPosition() const;
    float getSpeed() const { return speed; } // 速度获取
    void setSpeed(float newSpeed) { speed = newSpeed; } // 速度设置
    int getBulletDamage() const { return bulletDamage; } // 子弹伤害（配置）

    /// 移动探测盒尺寸（像素）：来自 entities.json 的 collisionBox，不再是写死的 40。
    int getCollisionBoxWidth() const { return collisionBoxW; }
    int getCollisionBoxHeight() const { return collisionBoxH; }

    /**
     * 车体矩形（以当前位置为中心、按配置尺寸）。
     *
     * **受击判定与移动探针共用这一个尺寸**（M3 决议 D4）。在此之前是三套值：
     * 移动 40、`getRect()` 30、配置里写着 40 却没人读 —— 子弹会从车身边缘擦过去
     * 而不命中，同一个坦克到底多大取决于问哪一处。
     */
    QRect getRect() const;

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
    int collisionBoxW = 40; // 移动探测盒宽（配置 collisionBox[0]）
    int collisionBoxH = 40; // 移动探测盒高（配置 collisionBox[1]）
    Map *gameMap;

private:
    /// 以 center 为中心、按配置尺寸构造探测矩形。
    QRect probeRect(const QPoint &center) const;

    /**
     * 该探测矩形所在位置能否通行。
     *
     * 与旧实现的两处判定等价（`canMove()` 与 `move()` 里的 switch）：
     * 森林/冰块可穿过（只影响速度），无碰撞放行，其余（边界/砖/钢/海）拦截。
     * 旧写法把这一条谓词写了两遍，而且 `canMove()` 那份的位移**漏乘了地形倍率** ——
     * 合并成一处后这种不一致不可能再出现。
     */
    bool isPassable(const QRect &probe, Map *map) const;
};


#endif
