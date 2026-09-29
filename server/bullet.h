#ifndef BULLET_H
#define BULLET_H

#include <QPoint>               //表示点，存储点的位置坐标
#include <QRect>                //表示矩形的区域，用于判断碰撞
#include <QPainter>             //绘制子弹
#include <memory>               //智能指针管理子弹的声明周期

enum class BulletType {         //枚举子弹类型
    Player,
    Enemy
};

class Bullet
{
public:
    using Ptr = std::shared_ptr<Bullet>;        //使用智能指针
    using CollisionCheckFunc = std::function<bool(const QRect&)>;   //实现碰撞机制检测

    Bullet(const QPoint &position, float angle, BulletType type,
           CollisionCheckFunc collisionCallback = nullptr);     //构造函数，分别有子弹的坐标，移动的角度，子弹的类型，碰撞检测（默认为空）

    void move();                                        //子弹的移动
    bool isOutOfBounds(int width, int height) const;    //判断是否出界
    void draw(QPainter &painter) const;                 //画子弹
    QRect getRect() const;                              //获取子弹的矩形并且返回对象用于检测
    BulletType getType() const;                         //获取子弹类型
    int getInjury() const { return injury; }            //获取子弹伤害值
    QPoint getPosition() const { return position; }     //获取子弹位置
    float getAngle() const { return angle; }            //获取子弹角度

private:
    QPoint position;                                    //子弹的位置
    float angle;                                        //转动的角度
    BulletType type;                                    //子弹类型
    static const int speed = 8;                         //子弹速度
    CollisionCheckFunc collisionCheckCallback;          //检测子弹碰撞
    const int injury = 25;                     //子弹伤害值
};

#endif // BULLET_H
