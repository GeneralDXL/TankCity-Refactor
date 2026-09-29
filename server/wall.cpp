#include "wall.h"

Wall Wall::makeBoundary(int x, int y, int width, int height)
{
    tankcity::config::BlockDef def;
    def.destructible = false;
    def.maxHealth = 0;
    def.blocksTank = true;
    def.blocksBullet = true;
    return Wall(x, y, width, height, BOUNDARY, def);
}

Wall::Wall(int x, int y, int width, int height, int type,
           const tankcity::config::BlockDef &def)
    : QRect(x, y, width, height), rect(x, y, width, height), type(type), x(x), y(y),
      width(width), height(height), health(def.maxHealth), destructible(def.destructible),
      blocksTank(def.blocksTank), blocksBullet(def.blocksBullet),
      moveSpeedFactor(def.moveSpeedFactor)
{
    static int nextId = 1;
    id = nextId++; // 使用自增ID作为唯一ID，避免地址相关问题
}
