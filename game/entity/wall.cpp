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
      moveSpeedFactor(def.moveSpeedFactor),
      // 边界墙用的是一个空的默认 BlockDef（它的行为是引擎约定，不由 blocks.json 描述），
      // 所以这里给它一个固定 id，客户端按 id 查贴图表时才有得查。
      block(def.id.isEmpty() ? QStringLiteral("boundary") : def.id)
{
    static int nextId = 1;
    id = nextId++; // 使用自增ID作为唯一ID，避免地址相关问题
}
