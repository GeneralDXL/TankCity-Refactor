#ifndef BULLET_H
#define BULLET_H

#include "config/TankStats.h"   // BulletProfile（M4 步 2）
#include "reflect.h"            // Face：反射法线四值（与 map.h 同样的引用方式）

#include <QPoint>               //表示点，存储点的位置坐标
#include <QPointF>              //速度向量与亚像素位置
#include <QRect>                //表示矩形的区域，用于判断碰撞
#include <functional>           //碰撞检测回调 std::function
#include <memory>               //智能指针管理子弹的声明周期

enum class BulletType {         //枚举子弹类型
    Player,
    Enemy
};

/**
 * 一次命中判定的结果（M4 步 2）。
 *
 * 为什么不是 `bool`：反弹要求判定方与子弹**共同**决定这发子弹的命运 ——
 * `Map` 能回答的是"撞到了什么、它可不可弹、撞在哪一面"，
 * 而"还剩几次弹数、这次该不该消失"是**子弹自己的状态**。
 * 于是回调只给证据，决策留在子弹手里（与 M3「几何归 World、规则归 Map」同一路数）。
 */
struct BulletHit
{
    bool hit = false;                 ///< 有没有碰到物块
    bool bouncable = false;           ///< 碰到的物块在 `profile.bounceOn` 清单里
    tankcity::world::Face face = tankcity::world::Face::Left;   ///< 撞在哪一面（hit 时有效）
    QRect wallRect;                   ///< 物块矩形（hit 时有效；把子弹推出表面要用它）
};

class Bullet
{
public:
    using Ptr = std::shared_ptr<Bullet>;        //使用智能指针

    /**
     * 碰撞检测回调：给出这次命中的**证据**，由子弹决定自己的去留。
     *
     * profile 由子弹自带并随回调传回（来自 entities.json 的子弹原型）：伤害、标签、
     * 可弹物块清单都在里面。这样命中判定只需要一个参数，M4.5 的**弹药队列**
     * （同一弹夹混着不同弹种）也只需换一个 profile，判定方一行都不用改。
     */
    using CollisionCheckFunc = std::function<BulletHit(const QRect &, const tankcity::config::BulletProfile &)>;

    /// @param speed   像素 / 帧（调用方已按 tuning.baseBulletSpeed 把倍率展开）
    /// @param angle   发射角度（度）—— 只在这里出现一次，之后一切从速度向量推导
    /// @param profile 这发子弹的属性（伤害 / 标签 / 可弹清单 / 最大弹数）
    Bullet(const QPoint &position, float angle, BulletType type, double speed,
           const tankcity::config::BulletProfile &profile,
           CollisionCheckFunc collisionCallback = nullptr);

    void move();                                        //子弹的移动（含命中判定与反弹）
    bool isOutOfBounds(int width, int height) const;    //判断是否出界
    QRect getRect() const;                              //获取子弹的矩形并且返回对象用于检测
    BulletType getType() const;                         //获取子弹类型
    int getInjury() const { return profile_.damage; }   //获取子弹伤害值
    QPoint getPosition() const { return position; }     //获取子弹位置

    /// 子弹朝向（度）。**由速度向量反推** —— 所以"改成速度向量"对协议与客户端是透明的：
    /// `server/game.cpp` 广播的 `angle` 字段照旧，客户端一行都不用改。
    float getAngle() const;

    const QStringList &getTags() const { return profile_.tags; }        //子弹标签
    const tankcity::config::BulletProfile &profile() const { return profile_; }
    int bouncesLeft() const { return bouncesLeft_; }     //还剩几次弹数
    QPointF velocity() const { return velocity_; }       //当前速度向量

private:
    /// 把子弹摆到物块表面**外侧**（多留 1px 余量），免得下一帧又判定为"撞在同一面"
    /// —— 同一面连撞两次会把速度原样翻回去，表现就是子弹卡在墙里抖动。
    void placeOutside(const QRect &wallRect, tankcity::world::Face face);

    /// 标记为待清除（沿用旧约定：位置移到屏幕外，由服务端清扫）。
    void kill();

    QPoint position;                                //整数位置：判定与协议都用它
    QPointF exactPosition;                          //精确位置（亚像素）：逐帧累加，避免斜向被截断
    QPointF velocity_;                              //速度向量（像素/帧）
    BulletType type;                                //子弹类型
    tankcity::config::BulletProfile profile_;       //这发子弹的属性（来自配置）
    int bouncesLeft_ = 0;                           //剩余弹数（初始 = profile_.maxBounces）
    CollisionCheckFunc collisionCheckCallback;      //检测子弹碰撞
};

#endif // BULLET_H
