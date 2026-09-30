#ifndef WALL_H
#define WALL_H

#define BOUNDARY 0 // 边界类型
#define BRICK 1
#define STEEL 2
#define FOREST 3
#define SEA 4
#define ICE 5

#include <QRect>
#include <QPainter>
#include "config/ConfigTypes.h"

class Wall : public QRect{
private:
    QRect rect; // 矩形区域
    int type;   // 墙的类型
    int x;      // x坐标(左上)
    int y;      // y坐标(左上)
    int width;  // 宽度
    int height; // 高度
    int health; // 生命值（来自配置；不可破坏的物块为 0）
    int id;
    // 以下四项行为一律取自 assets/config/blocks.json，不再按 type 写死判断。
    bool destructible = false;      // 能否被打掉
    bool blocksTank = true;         // 是否挡坦克
    bool blocksBullet = true;       // 是否挡子弹
    double moveSpeedFactor = 0.0;   // 地形移动倍率（0 表示不影响移动）
public:
    QRect getRect() const { return rect; } // 获取矩形区域

    /**
     * 边界墙：四边等厚、不可破坏、挡坦克也挡子弹。
     *
     * 这是引擎的固定约定，不由 blocks.json 描述 —— 关卡不允许自带边界物块，
     * 所以这里显式写出行为，而不是从配置读（BlockDef 的默认值恰好相反）。
     */
    static Wall makeBoundary(int x, int y, int width, int height);

    /// 关卡物块：可破坏性 / 生命值 / 三项碰撞开关 / 地形倍率全部来自 BlockDef。
    Wall(int x, int y, int width, int height, int type,
         const tankcity::config::BlockDef &def);

    ~Wall() = default; // 默认析构函数

    int getType() const { return type; } // 获取墙的类型
    void setType(int t) { type = t; } // 设置墙的类型
    bool isDestructible() const { return destructible; } // 是否可破坏（配置）
    bool isBlockingTank() const { return blocksTank; }   // 是否挡坦克（配置）
    bool isBlockingBullet() const { return blocksBullet; } // 是否挡子弹（配置）
    double getMoveSpeedFactor() const { return moveSpeedFactor; } // 地形倍率（配置）
    int getHealth() const { return health; } // 获取生命值
    void setHealth(int h) { health = h; } // 设置生命值
    bool isMovable() const { return !blocksTank; }
    int getId() const { return id; } // 获取墙的唯一ID  
    void takeDamage(int damage)
    {
        if (isDestructible()) {
            health -= damage;
            if (health < 0)
            {
                health = 0;
                delete this;
            }
        }
    }
    
};

#endif // WALL_H
