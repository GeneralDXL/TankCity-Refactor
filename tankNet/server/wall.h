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

class Wall : public QRect{
private:
    QRect rect; // 矩形区域
    int type;   // 墙的类型
    int x;      // x坐标(左上)
    int y;      // y坐标(左上)
    int width;  // 宽度
    int height; // 高度
    int health; // 生命值
    int id;
public:
    QRect getRect() const { return rect; } // 获取矩形区域
    Wall(int x, int y, int width, int height, int type);

    ~Wall() = default; // 默认析构函数

    int getType() const { return type; } // 获取墙的类型
    void setType(int t) { type = t; } // 设置墙的类型
    bool isDestructible() const; // 是否可破坏
    int getHealth() const { return health; } // 获取生命值
    void setHealth(int h) { health = h; } // 设置生命值
    bool isMovable() const {
        return (type == FOREST || type == ICE);
    }

    int getId() const { return id; } // 获取墙的唯一ID
    
};

#endif // WALL_H
