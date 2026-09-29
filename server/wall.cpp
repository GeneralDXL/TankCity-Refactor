#include "wall.h"

Wall::Wall(int x, int y, int width, int height, int type)
    : QRect(x, y, width, height), rect(x, y, width, height), type(type), x(x), y(y), width(width), height(height), health(100)
{
    // 初始化墙的矩形区域
    rect.setRect(x, y, width, height);
    switch (type) {
        case BOUNDARY:
            health = 0; // 边界墙不可破坏
            break;
        case BRICK:
            health = 100; // 砖墙生命值
            break;
        case STEEL:
            health = 200000; // 钢墙生命值
            break;
        case FOREST:
            health = 0; // 森林生命值
            break;
        case SEA:
            health = 0; // 海洋不可破坏
            break;
        case ICE:
            health = 0; // 冰块生命值
            break;
        default:
            health = 100; // 默认生命值
    }

    static int nextId = 1;
    id = nextId++; // 使用自增ID作为唯一ID，避免地址相关问题
}

bool Wall::isDestructible() const {
    // 判断墙是否可破坏
    return type == BRICK || type == STEEL;
}