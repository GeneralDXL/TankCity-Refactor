#include "map.h"

#include <QDebug>
#include <QJsonObject>

// 注：旧 map.cpp 里的 <QRandomGenerator> / <QJsonDocument> 从头到尾只出现在 include 行，
// 关卡几何完全由写死的表达式算出（已由 level_test.cpp 对照冻结快照验证），故一并清掉。

namespace {

/// 物块 id -> Wall::type 的桥接表。
///
/// M2 只把「关卡几何」外置成数据，Wall 仍然用 wall.h 的 int type 表达碰撞、贴图，
/// 以及 map_init 协议里的 wallType，所以这里显式做一次映射。新增物块种类时，
/// 除了 assets/config/blocks.json，还要在这里登记一次；等 M3 让渲染与碰撞直接按
/// block id 走，这层桥接即可删除。
struct BlockTypeEntry
{
    const char *id;
    int type;
};

const BlockTypeEntry kBlockTypes[] = {
    {"brick", BRICK},
    {"steel", STEEL},
    {"forest", FOREST},
    {"sea", SEA},
    {"ice", ICE},
};

int wallTypeOfBlock(const QString &blockId)
{
    for (const BlockTypeEntry &entry : kBlockTypes) {
        if (blockId == QLatin1String(entry.id))
            return entry.type;
    }
    return -1;
}

} // namespace

Map::Map() = default;

bool Map::loadLevel(const tankcity::config::LevelData &level,
                    const tankcity::config::Config &config)
{
    walls.clear();

    // 格点寻路（worldToGrid / isCellWalkable）与画面尺寸仍写死 1200x900，
    // 关卡一旦声明别的世界尺寸，坐标换算就全错了 —— 宁可拒绝开局也不要跑出鬼地图。
    if (level.worldWidth != MAP_WIDTH || level.worldHeight != MAP_HEIGHT) {
        qCritical() << "关卡" << level.id << "的世界尺寸" << level.worldWidth << "x"
                    << level.worldHeight << "与引擎写死的" << MAP_WIDTH << "x" << MAP_HEIGHT
                    << "不一致，无法装载";
        return false;
    }

    // 边界墙：旧代码里手写的四条，现在由关卡属性生成
    for (const QRect &r : level.boundaryRects())
        walls.append(Wall::makeBoundary(r.x(), r.y(), r.width(), r.height()));

    for (const tankcity::config::LevelRect &lr : level.allRects()) {
        // 先问引擎能不能表达（贴图与 map_init 协议用的都是 int type），再取配置里的行为。
        // 顺序不可颠倒：level_test 的合成关卡正是靠这一条拒绝「引擎无法表达」的物块 id。
        const int type = wallTypeOfBlock(lr.block);
        if (type < 0) {
            qCritical() << "关卡" << level.id << "使用了 Wall 无法表达的物块 id：" << lr.block;
            walls.clear();
            return false;
        }
        const tankcity::config::BlockDef *def = config.block(lr.block);
        if (def == nullptr) {
            qCritical() << "关卡" << level.id << "引用了 blocks.json 中不存在的物块：" << lr.block;
            walls.clear();
            return false;
        }
        walls.append(Wall(lr.rect.x(), lr.rect.y(), lr.rect.width(), lr.rect.height(), type, *def));
    }

    return true;
}

bool Map::checkCollision(const QRect &rect) const
{
    for (const Wall &wall : walls) {
        // 是否挡坦克由 blocks.json 的 blocksTank 决定（森林、冰块为 false）
        if (!wall.isBlockingTank()) {
            continue;
        }

        // 已经被打掉的砖墙不再阻挡
        if (wall.isDestructible() && wall.getHealth() <= 0) {
            continue;
        }

        if (rect.intersects(wall.getRect())) {
            return true;
        }
    }
    return false;
}

bool Map::checkBulletCollision(const QRect &rect, int damage)
{
    for (Wall &wall : walls) {
        // 是否挡子弹由 blocks.json 的 blocksBullet 决定
        // （旧代码跳过森林/海洋/冰块，正是这三者为 false；边界与砖钢为 true）
        if (!wall.isBlockingBullet()) {
            continue;
        }

        if (rect.intersects(wall.getRect())) {
            if(wall.isDestructible()){
                wall.setHealth(wall.getHealth() - damage);
            }
            if(wall.isDestructible() && wall.getHealth() <= 0)
            {
                QJsonObject json;
                json["type"]="delete_wall";
                json["wallId"]=wall.getId();
                emit broadcastMessage(json);
                walls.removeOne(wall);
            }
            return true;
        }
    }
    return false;
}

int Map::checkTankCollision(const QRect &rect, const Tank *tank) const
{
    for (const Wall &wall : walls) {
        // 是否挡坦克由 blocks.json 的 blocksTank 决定（森林、冰块为 false）。
        // 旧代码把海洋单列一支，返回的也是 SEA，与这里返回 getType() 等价。
        if (!wall.isBlockingTank()) {
            continue;
        }

        if (rect.intersects(wall.getRect())) {
            return wall.getType(); // 返回墙的类型
        }
    }
    return -1; // 无碰撞
}


QPoint Map::worldToGrid(const QPoint& worldPos) const {
    return QPoint(worldPos.x() / 40, worldPos.y() / 40);
}

bool Map::isCellWalkable(int gridX, int gridY) const {
    int padding = 5;
    QRect cellRect(
        gridX * 40 + padding,
        gridY * 40 + padding,
        40 - 2 * padding,
        40 - 2 * padding
        );


    for (const Wall &wall : walls) {
        // 只由 blocksTank 决定：森林、冰块可穿过，海洋/边界/砖/钢不可（与旧写法一致）
        if (!wall.isBlockingTank()) {
            continue;
        }

        if (wall.getRect().intersects(cellRect)) {
            return false;
        }
    }
    return true;
}

bool Map::isLineWalkable(const QPoint& start, const QPoint& end) const {
    QPoint worldStart = gridToWorld(start);
    QPoint worldEnd = gridToWorld(end);

    // Bresenham算法遍历直线上的点
    int dx = abs(worldEnd.x() - worldStart.x());
    int dy = abs(worldEnd.y() - worldStart.y());
    int sx = (worldStart.x() < worldEnd.x()) ? 1 : -1;
    int sy = (worldStart.y() < worldEnd.y()) ? 1 : -1;
    int err = dx - dy;

    QPoint current = worldStart;
    while (current != worldEnd) {
        // 检查当前点是否可行
        if (!isCellWalkable(worldToGrid(current).x(), worldToGrid(current).y())) {
            return false;
        }

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            current.rx() += sx;
        }
        if (e2 < dx) {
            err += dx;
            current.ry() += sy;
        }
    }
    return true;
}

QPoint Map::gridToWorld(const QPoint& gridPos) const {
    return QPoint(gridPos.x() * 40 + 20, gridPos.y() * 40 + 20);
}

int Map::getTerrainType(const QPoint &position) const
{
    for (const Wall &wall : walls) {
        if (wall.contains(position)) {
            return wall.getType();
        }
    }
    return -1; // 默认地形
}

double Map::getMoveSpeedFactor(const QPoint &position) const
{
    for (const Wall &wall : walls) {
        if (wall.contains(position)) {
            const double factor = wall.getMoveSpeedFactor();
            return factor > 0.0 ? factor : 1.0; // 0 表示该物块不影响移动
        }
    }
    return 1.0;
}
