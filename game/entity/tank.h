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

    /// 默认弹种的完整属性（M4 步 2）。开火方用它构造子弹。
    const tankcity::config::BulletProfile &bulletProfile() const { return bulletProfile_; }

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

    /**
     * 朝 @p angle 走 @p distance 会不会撞上 —— **只查询，不改位置**。
     *
     * 与 `move()` 用同一套判定（同一个探测盒、同一个 `isPassable`），区别是它没有副作用。
     * 敌人转向需要"先比较几个候选方向再决定走哪个"，所以必须有这样一个查询：
     * M3 步 3b 曾把语义相同的 `canMove()` 当重复代码删掉，4e 把它以正确的形态加回来
     * —— 当时删是对的（它和 `move()` 里的 switch 是同一谓词的两份），
     * 现在需要的是**查询**，不是第二份判定。
     */
    bool canStep(float angle, float distance, Map *map) const;

    void setPosition(const QPoint& pos) { position = pos; moveRemainder_ = QPointF(); }
    void setBodyAngle(float angle) { bodyAngle = angle; } // 车身角度设置
    float getBodyAngle() const { return bodyAngle; } // 车身角度获取

    // 添加获取方向函数
    Direction getDirection() const { return direction; }
    float getTurretAngle() const { return turretAngle; }

protected:
    QPoint position;
    Direction direction = Up;
    float bodyAngle = 0; // 车身角度
    int health = 0;
    float speed = 0.0f;
    double bulletSpeed = 0.0;  // 子弹速度（像素/帧）
    int bulletDamage = 1;      // 子弹伤害
    // 默认弹种的完整属性（M4 步 2）：伤害 / 标签 / 可弹清单 / 最大弹数。
    // 开火时整份交给子弹 —— 命中判定因此只需要一个参数，M4.5 的弹药队列也只需换一份。
    tankcity::config::BulletProfile bulletProfile_;
    int muzzleOffset = 0;      // 炮口相对车体中心的距离（像素）
    // 一律给默认值：漏进初始化列表就是未初始化的栈垃圾 ——
    // M3 步 3b 重写本文件时漏掉 `shootDelay(stats.shootDelayTicks)`，
    // 症状就是"打完一发再也打不出来"（`shootCooldown = shootDelay` 变成随机值）。
    int shootCooldown = 0;
    int shootDelay = 0;
    float turretAngle = 0;  // 炮管角度
    int collisionBoxW = 40; // 移动探测盒宽（配置 collisionBox[0]）
    int collisionBoxH = 40; // 移动探测盒高（配置 collisionBox[1]）
    Map *gameMap = nullptr;

private:
    /**
     * 亚像素余量：上一帧没走完的、不足 1 像素的位移。
     *
     * 位移经常不足 1px —— 森林（×0.5）上的敌人斜向走只有 `2.5 × 0.707 ≈ 1.77px`，
     * 向零截断后只剩 1；倍率再小一点就直接截成 0，**整帧位移消失**，
     * 表现就是"站在森林上不动"（实测反馈）。把余量带到下一帧累加，
     * 长距离的平均位移就不再受逐帧取整影响。
     */
    QPointF moveRemainder_;

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
