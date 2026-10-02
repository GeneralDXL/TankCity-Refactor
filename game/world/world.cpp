#include "world.h"

#include <QDebug>

#include "physics/Aabb.h"

// M1.1：几何查询（相交、格点换算、走线）已移交 engine/physics：
//   - 遍历与筛选交给 physics/CollisionWorld（它只认位标记，不认「坦克/子弹」）；
//   - 扣血、销毁、广播这些**判定**留在 Map（规则层），本文件只回答几何问题。
// 每个查询的筛选条件都刻意与旧实现逐条对应（含顺序），容易看错的几处都写了注释。

// M3 步 5：这里原先有一张「物块 id -> Wall::type」的桥接表（brick -> 1、steel -> 2 …），
// 是 M2 时代为「Wall 仍用 int type 表达碰撞与贴图、协议里也发类型号」而存在的。
// 现在物块身份从 blocks.json 一路上传（Wall::getBlockId → map_init 的 block 字段 →
// 客户端贴图表），类型号已无必要，桥接表随之删除 —— 新增物块只需写 blocks.json。

World::World() = default;

bool World::loadLevel(const tankcity::config::LevelData &level,
                      const tankcity::config::Config &config)
{
    walls.clear();

    // 格点寻路（worldToGrid / isCellWalkable）与画面尺寸仍写死 1200x900，
    // 关卡一旦声明别的世界尺寸，坐标换算就全错了 —— 宁可拒绝开局也不要跑出鬼地图。
    if (level.worldWidth != MAP_WIDTH || level.worldHeight != MAP_HEIGHT) {
        qCritical() << "关卡" << level.id << "的世界尺寸" << level.worldWidth << "x"
                    << level.worldHeight << "与引擎写死的" << MAP_WIDTH << "x" << MAP_HEIGHT
                    << "不一致，无法装载";
        rebuildCollisionWorld();
        return false;
    }

    // 边界墙：旧代码里手写的四条，现在由关卡属性生成
    for (const QRect &r : level.boundaryRects())
        walls.append(Wall::makeBoundary(r.x(), r.y(), r.width(), r.height()));

    for (const tankcity::config::LevelRect &lr : level.allRects()) {
        // 物块是否合法只由 blocks.json 说了算（M3 步 5 之前还要先问一句"Wall 能不能表达
        // 这个类型号"，那张桥接表已经删了）。拒绝时清空 World，不留半张地图。
        const tankcity::config::BlockDef *def = config.block(lr.block);
        if (def == nullptr) {
            qCritical() << "关卡" << level.id << "引用了 blocks.json 中不存在的物块：" << lr.block;
            walls.clear();
            rebuildCollisionWorld();
            return false;
        }
        walls.append(Wall(lr.rect.x(), lr.rect.y(), lr.rect.width(), lr.rect.height(), *def));
    }

    rebuildCollisionWorld();
    return true;
}

engine::physics::CollisionWorld::Flags World::flagsOf(const Wall &wall)
{
    engine::physics::CollisionWorld::Flags flags = 0;
    if (wall.isBlockingTank())
        flags |= kBlocksMove;
    if (wall.isBlockingBullet())
        flags |= kBlocksShot;
    return flags;
}

void World::rebuildCollisionWorld()
{
    world_.clear();
    for (const Wall &wall : walls) {
        const engine::physics::CollisionWorld::Flags flags = flagsOf(wall);
        if (flags == 0)
            continue;   // 既挡不了移动也挡不了投射物（如森林），无需进碰撞世界
        world_.add(wall.getId(), engine::physics::Aabb::fromRect(wall.getRect()), flags);
    }
}

const Wall *World::wallById(int wallId) const
{
    for (const Wall &wall : walls) {
        if (wall.getId() == wallId)
            return &wall;
    }
    return nullptr;
}

Wall *World::mutableWallById(int wallId)
{
    for (Wall &wall : walls) {
        if (wall.getId() == wallId)
            return &wall;
    }
    return nullptr;
}

bool World::removeWallById(int wallId)
{
    for (int i = 0; i < walls.size(); ++i) {
        if (walls.at(i).getId() == wallId) {
            walls.removeAt(i);
            world_.remove(wallId);   // 几何副本同步移除
            return true;
        }
    }
    return false;
}

bool World::checkCollision(const QRect &rect) const
{
    bool blocked = false;
    world_.forEachOverlap(engine::physics::Aabb::fromRect(rect), kBlocksMove,
                          [this, &blocked](const engine::physics::CollisionWorld::Body &body) {
        // 是否挡坦克由 blocks.json 的 blocksTank 决定（森林、冰块为 false）——
        // 那一条已在标记里筛掉，这里只有「被打掉之后不再阻挡」这一条状态判据。
        const Wall *wall = wallById(body.id);
        if (wall != nullptr && wall->isDestructible() && wall->getHealth() <= 0) {
            return true;   // 跳过，继续看下一个
        }
        blocked = true;
        return false;      // 首个命中即返回
    });
    return blocked;
}

bool World::blocksTankAt(const QRect &rect) const
{
    bool blocked = false;
    world_.forEachOverlap(engine::physics::Aabb::fromRect(rect), kBlocksMove,
                          [&blocked](const engine::physics::CollisionWorld::Body &) {
        // 是否挡坦克由 blocks.json 的 blocksTank 决定（森林、冰块为 false）——
        // 那一条已在标记里筛掉（见 flagsOf），这里只需回答"有没有"。
        blocked = true;
        return false;   // 首个命中即返回
    });
    return blocked;
}

int World::firstShotBlockerId(const QRect &rect) const
{
    int hitWallId = -1;

    world_.forEachOverlap(engine::physics::Aabb::fromRect(rect), kBlocksShot,
                          [&hitWallId](const engine::physics::CollisionWorld::Body &body) {
        // 是否挡子弹由 blocks.json 的 blocksBullet 决定
        // （旧代码跳过森林/海洋/冰块，正是这三者为 false；边界与砖钢为 true）
        hitWallId = body.id;
        return false;      // 首个命中即返回
    });

    return hitWallId; // -1 表示没有挡子弹的物块
}

QPoint World::worldToGrid(const QPoint &worldPos) const
{
    return grid_.toCell(worldPos);
}

bool World::isCellWalkable(int gridX, int gridY) const
{
    // 只由 blocksTank 决定：森林、冰块可穿过，海洋/边界/砖/钢不可（与旧写法一致）。
    // 这里刻意**不做**血量判据 —— 旧实现也没有，而这条在 A* 内层循环里会被调用成千上万次。
    bool walkable = true;
    const QRect cellRect = grid_.cellRect(QPoint(gridX, gridY), kCellPadding);

    world_.forEachOverlap(engine::physics::Aabb::fromRect(cellRect), kBlocksMove,
                          [&walkable](const engine::physics::CollisionWorld::Body &) {
        walkable = false;
        return false;
    });
    return walkable;
}

bool World::isLineWalkable(const QPoint &start, const QPoint &end) const
{
    // Bresenham 走线本身已是引擎能力（engine/physics/Grid::walkLine）；
    // 「一格能不能过」仍是地图的判断，所以以回调注入。
    return grid_.walkLine(start, end, [this](const QPoint &cell) {
        return isCellWalkable(cell.x(), cell.y());
    });
}

QPoint World::gridToWorld(const QPoint &gridPos) const
{
    return grid_.toWorld(gridPos);
}

double World::getMoveSpeedFactor(const QPoint &position) const
{
    for (const Wall &wall : walls) {
        if (wall.contains(position)) {
            const double factor = wall.getMoveSpeedFactor();
            return factor > 0.0 ? factor : 1.0; // 0 表示该物块不影响移动
        }
    }
    return 1.0;
}
