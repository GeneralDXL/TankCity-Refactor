#ifndef PLAYER_H
#define PLAYER_H

#include "tank.h"
#include "bullet.h"
#include "map.h"
#include <QPoint>
#include <QPointF>

class Player : public Tank
{
public:
    Player(Map *gameMap, const tankcity::config::TankStats &stats); // 修改构造函数
    void init(int x, int y);
    Bullet* shoot() override;
    // getRect() 由 Tank 提供：受击盒与移动探针同尺寸（M3 决议 D4）
    bool canShoot() const;
    void setTurretAngle(float angle);
    void updateCooldown();
    void setBodyAngle(float angle) { bodyAngle = angle; }
    float getBodyAngle() const { return bodyAngle; }

    // --- 双摇杆输入（M3）：左摇杆走、右摇杆瞄，两路互不相干 ---

    /**
     * 车体推进方向（本帧想往哪走）。
     *
     * 数字键送来的是 (±1, ±1) 的八向值，**这里不做归一化** —— 「走多快」是规则，
     * 由 `updateMovement()` 在服务端一侧处理。因此将来换成摇杆（送来 -0.37 这类
     * 连续值）不需要改协议，也不需要改这个接口。
     */
    void setMoveVector(const QPointF &v) { moveVector = v; }
    QPointF getMoveVector() const { return moveVector; }

    /**
     * 瞄准点（世界坐标）。
     *
     * 只**存下来**，由 `update()` 换算成 `turretAngle`。这样输入路径完全不需要知道
     * 「炮塔」这个概念：旧接口 `setTurretTarget()` 是每收到一条输入就直接改炮塔状态，
     * 改造后只有 `update()` 会写 `turretAngle`。
     *
     * 注意仍是**绝对瞄准点**而不是方向向量：当前视图与世界是 1:1（1200x900、无镜头），
     * 两者等价；等 M7 有镜头/缩放之后，客户端要把屏幕坐标换算成世界坐标再发。
     */
    void setAimPoint(const QPoint &target) { aimPoint = target; }
    QPoint getAimPoint() const { return aimPoint; }

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
    /// 炮塔跟随瞄准点（旧行为逐字保留）。
    void updateTurretAim();
    /// 车体按 `moveVector` 推进（八向；斜向归一化后速度与直行一致）。
    void updateMovement();

    QPointF moveVector;   ///< 本帧的推进方向（未归一化）
    QPoint aimPoint;      ///< 瞄准点（世界坐标）

    //-------------------------------------------------加
    bool invincible = false;
    int maxHealth = 0;  // 配置里的满血值，reset 与治疗上限都用它
};
#endif
