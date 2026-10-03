#ifndef BULLET_H
#define BULLET_H

#include <QPoint>               //表示点，存储点的位置坐标
#include <QRect>                //表示矩形的区域，用于判断碰撞
#include <QStringList>          //子弹标签（M4 步 1）
#include <functional>           //碰撞检测回调 std::function
#include <memory>               //智能指针管理子弹的声明周期

enum class BulletType {         //枚举子弹类型
    Player,
    Enemy
};

class Bullet
{
public:
    using Ptr = std::shared_ptr<Bullet>;        //使用智能指针
    /**
     * 碰撞检测回调：返回 true 表示子弹命中并应当消失。
     *
     * damage 与 tags 都由子弹自带（来自 entities.json 的子弹原型）并随回调传回，是为了让
     * 「命中物块之后扣不扣血、扣多少」与判定发生在同一处 —— 否则这些又得在 map.cpp 里再写一遍。
     *
     * tags 是 M4 步 1 加的：物块可以要求特定标签（钢材要 armorPiercing），
     * 判定方需要知道这发子弹提供了哪些标签。参数从子弹取而不是从坦克闭包捕获，
     * 是为了让 M4.5 的「弹药队列」（同一弹夹混着不同弹种）不必再改这个接口。
     */
    using CollisionCheckFunc = std::function<bool(const QRect&, int damage, const QStringList &tags)>;

    /// @param speed  像素 / 帧（调用方已按 tuning.baseBulletSpeed 把倍率展开）
    /// @param damage 命中伤害（来自子弹原型）
    /// @param tags   子弹标签（来自子弹原型，如 armorPiercing）
    Bullet(const QPoint &position, float angle, BulletType type, double speed, int damage,
           const QStringList &tags,
           CollisionCheckFunc collisionCallback = nullptr);     //构造函数，分别有子弹的坐标，移动的角度，子弹的类型，子弹速度、伤害、标签，碰撞检测（默认为空）

    void move();                                        //子弹的移动
    bool isOutOfBounds(int width, int height) const;    //判断是否出界
    QRect getRect() const;                              //获取子弹的矩形并且返回对象用于检测
    BulletType getType() const;                         //获取子弹类型
    int getInjury() const { return injury; }            //获取子弹伤害值
    QPoint getPosition() const { return position; }     //获取子弹位置
    float getAngle() const { return angle; }            //获取子弹角度

    /// 这发子弹的标签（M4 步 1）。命中判定用；将来的弹药队列、弹射计数也走它。
    const QStringList &getTags() const { return tags; }

private:
    QPoint position;                                    //子弹的位置
    float angle;                                        //转动的角度
    BulletType type;                                    //子弹类型
    double speed;                                       //子弹速度（像素/帧，来自配置）
    QStringList tags;                                   //子弹标签（来自配置）
    CollisionCheckFunc collisionCheckCallback;          //检测子弹碰撞
    int injury;                                         //子弹伤害值（来自配置）
};

#endif // BULLET_H
