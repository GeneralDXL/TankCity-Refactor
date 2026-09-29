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

bool Map::loadLevel(const tankcity::config::LevelData &level)
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
        walls.append(Wall(r.x(), r.y(), r.width(), r.height(), BOUNDARY));

    for (const tankcity::config::LevelRect &lr : level.allRects()) {
        const int type = wallTypeOfBlock(lr.block);
        if (type < 0) {
            qCritical() << "关卡" << level.id << "使用了 Wall 无法表达的物块 id：" << lr.block;
            walls.clear();
            return false;
        }
        walls.append(Wall(lr.rect.x(), lr.rect.y(), lr.rect.width(), lr.rect.height(), type));
    }

    return true;
}

bool Map::checkCollision(const QRect &rect) const
{
    for (const Wall &wall : walls) {
        if (wall.getType() == BRICK && wall.getHealth() <= 0) {
            continue;
        }

        // 只跳过森林和冰块（坦克可以穿过）
        // 海洋不再跳过 -> 坦克不能穿过海洋
        if (wall.getType() == FOREST || wall.getType() == ICE) {
            continue;
        }

        if (rect.intersects(wall.getRect())) {
            return true;
        }
    }
    return false;
}

bool Map::checkBulletCollision(const QRect &rect)
{
    for (Wall &wall : walls) {
        // 跳过森林、海洋和冰块（子弹可以穿过）
        if (wall.getType() == FOREST ||
            wall.getType() == SEA || // 添加海洋跳过
            wall.getType() == ICE) {
            continue;
        }

        if (rect.intersects(wall.getRect())) {
            if(wall.isDestructible()){
                wall.setHealth(wall.getHealth() - 25);
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
        // 跳过森林和冰块（坦克可以穿过）
        if (wall.getType() == FOREST || wall.getType() == ICE) {
            continue;
        }

        // 海洋阻挡坦克
        if (rect.intersects(wall.getRect()) && wall.getType() == SEA) {
            return SEA; // 返回海洋类型
        }

        // 其他障碍物阻挡坦克
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
        // 跳过森林和冰块（坦克可以穿过）
        if (wall.getType() == FOREST || wall.getType() == ICE) {
            continue;
        }

        // 检查海洋、边界、砖墙和钢墙
        if (wall.getRect().intersects(cellRect)) {
            // 海洋和不可穿越的墙
            if (wall.getType() == SEA || wall.getType() == BOUNDARY ||
                wall.getType() == BRICK || wall.getType() == STEEL) {
                return false;
            }
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
