/**
 * @file  engine_physics_test.cpp
 * @brief engine/physics 的回归测试：几何判定必须与旧的 QRect / Map 实现逐位一致。
 *
 * 为什么这些用例重要：碰撞是这次分层里最容易「改坏了却看不出来」的部分
 * （差一像素、顺序一变，平时都正常，只在特定贴墙位置出问题）。
 * 所以这里的断言刻意写成**与 QRect 对照**和**逐格枚举**，而不是笼统的「能撞上」。
 *
 * 另外 `Map::worldToGrid` / `gridToWorld` 的等价断言在 tests/map_grid_test.cpp：
 * `Map` 改为委托 `Grid` 之后，那组用例就成了 `Grid` 的守门人。
 */

#include <gtest/gtest.h>

#include <QRect>
#include <QVector>

#include "physics/Aabb.h"
#include "physics/CollisionWorld.h"
#include "physics/Grid.h"

// ---------------------------------------------------------------------------
// Aabb
// ---------------------------------------------------------------------------

TEST(EnginePhysicsAabb, IntersectsAgreesWithQRectOnEveryPair)
{
    // 覆盖各类边界：空矩形、恰好贴边、1 像素重叠、包含、完全相同。
    const QVector<QRect> rects = {
        QRect(0, 0, 10, 10),
        QRect(0, 0, 10, 10),   // 完全相同
        QRect(10, 0, 10, 10),  // 右侧恰好贴边：不应相交
        QRect(9, 0, 10, 10),   // 重叠 1 像素：应相交
        QRect(0, 9, 10, 10),   // 下侧重叠 1 像素
        QRect(5, 5, 1, 1),     // 完全包含在第一个里
        QRect(0, 0, 0, 10),    // 空（宽 0）
        QRect(-20, -20, 15, 15),
        QRect(100, 100, 40, 40),
    };

    for (const QRect &a : rects) {
        for (const QRect &b : rects) {
            const engine::physics::Aabb boxA = engine::physics::Aabb::fromRect(a);
            const engine::physics::Aabb boxB = engine::physics::Aabb::fromRect(b);
            EXPECT_EQ(boxA.intersects(boxB), a.intersects(b))
                << "Aabb 与 QRect 判定不一致：" << a.x() << "," << a.y() << " 与 "
                << b.x() << "," << b.y();
        }
    }
}

TEST(EnginePhysicsAabb, RectConversionRoundTrips)
{
    const QRect original(12, 34, 56, 78);
    const engine::physics::Aabb box = engine::physics::Aabb::fromRect(original);

    EXPECT_EQ(box.x, 12);
    EXPECT_EQ(box.y, 34);
    EXPECT_EQ(box.w, 56);
    EXPECT_EQ(box.h, 78);
    EXPECT_EQ(box.toRect(), original);
    EXPECT_TRUE(box.isValid());
}

TEST(EnginePhysicsAabb, ContainsPointMatchesQRect)
{
    const engine::physics::Aabb box(10, 10, 20, 20);

    EXPECT_TRUE(box.contains(10, 10));   // 左上角算在内
    EXPECT_TRUE(box.contains(29, 29));   // 右下角是 x+w-1
    EXPECT_FALSE(box.contains(30, 30));  // 边界外一格不算
    EXPECT_FALSE(box.contains(9, 10));
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------

TEST(EnginePhysicsGrid, ToCellFloorsLikeTheLegacyImplementation)
{
    const engine::physics::Grid grid(40);

    EXPECT_EQ(grid.toCell(QPoint(0, 0)), QPoint(0, 0));
    EXPECT_EQ(grid.toCell(QPoint(39, 39)), QPoint(0, 0));
    EXPECT_EQ(grid.toCell(QPoint(40, 40)), QPoint(1, 1));
    EXPECT_EQ(grid.toCell(QPoint(80, 120)), QPoint(2, 3));
    // 与旧实现同样向零截断（而不是向下取整）。
    EXPECT_EQ(grid.toCell(QPoint(-1, -1)), QPoint(0, 0));
}

TEST(EnginePhysicsGrid, ToWorldReturnsCellCenterAndRoundTrips)
{
    const engine::physics::Grid grid(40);

    EXPECT_EQ(grid.toWorld(QPoint(0, 0)), QPoint(20, 20));
    EXPECT_EQ(grid.toWorld(QPoint(1, 1)), QPoint(60, 60));

    for (int x = 0; x < 30; ++x) {
        for (int y = 0; y < 22; ++y) {
            const QPoint cell(x, y);
            EXPECT_EQ(grid.toCell(grid.toWorld(cell)), cell)
                << "格中心换算无法回到原格：" << x << "," << y;
        }
    }
}

TEST(EnginePhysicsGrid, CellRectAndPaddingMatchTheLegacyNumbers)
{
    const engine::physics::Grid grid(40);

    EXPECT_EQ(grid.cellRect(QPoint(0, 0)), QRect(0, 0, 40, 40));
    EXPECT_EQ(grid.cellRect(QPoint(1, 2)), QRect(40, 80, 40, 40));
    // 旧的 isCellWalkable 用的就是这个内缩 5：40x40 -> 30x30，左上角内移 5。
    EXPECT_EQ(grid.cellRect(QPoint(0, 0), 5), QRect(5, 5, 30, 30));
    EXPECT_EQ(grid.cellRect(QPoint(3, 4), 5), QRect(125, 165, 30, 30));
}

TEST(EnginePhysicsGrid, WalkLineOnTheSameCellNeverCallsBack)
{
    const engine::physics::Grid grid(40);

    int calls = 0;
    const bool walkable = grid.walkLine(QPoint(3, 3), QPoint(3, 3), [&calls](const QPoint &) {
        ++calls;
        return true;
    });

    EXPECT_TRUE(walkable);
    EXPECT_EQ(calls, 0) << "起终点同格应当直接返回 true";
}

TEST(EnginePhysicsGrid, WalkLineVisitsBothEndsExactlyOncePerCell)
{
    const engine::physics::Grid grid(40);

    QVector<QPoint> visited;
    grid.walkLine(QPoint(0, 0), QPoint(4, 0), [&visited](const QPoint &cell) {
        visited.append(cell);
        return true;
    });

    // 终点格会被检查（像素级先踏入终点格范围），且每格只回调一次。
    const QVector<QPoint> expected = {QPoint(0, 0), QPoint(1, 0), QPoint(2, 0),
                                      QPoint(3, 0), QPoint(4, 0)};
    EXPECT_EQ(visited, expected);
}

TEST(EnginePhysicsGrid, WalkLineDoesNotRunPastTheEndCell)
{
    const engine::physics::Grid grid(40);

    QVector<QPoint> visited;
    grid.walkLine(QPoint(0, 0), QPoint(1, 0), [&visited](const QPoint &cell) {
        visited.append(cell);
        return true;
    });

    EXPECT_EQ(visited, (QVector<QPoint>{QPoint(0, 0), QPoint(1, 0)}));
}

TEST(EnginePhysicsGrid, WalkLineStopsEarlyWhenRejected)
{
    const engine::physics::Grid grid(40);

    QVector<QPoint> visited;
    const bool walkable = grid.walkLine(QPoint(0, 0), QPoint(4, 0), [&visited](const QPoint &cell) {
        visited.append(cell);
        return cell != QPoint(2, 0);   // 第 3 格拒绝
    });

    EXPECT_FALSE(walkable);
    EXPECT_EQ(visited, (QVector<QPoint>{QPoint(0, 0), QPoint(1, 0), QPoint(2, 0)}))
        << "被拒绝之后不应继续走";
}

// ---------------------------------------------------------------------------
// CollisionWorld
// ---------------------------------------------------------------------------

namespace {

constexpr engine::physics::CollisionWorld::Flags kBlocksMove = 1u << 0;
constexpr engine::physics::CollisionWorld::Flags kBlocksShot = 1u << 1;

} // namespace

TEST(EnginePhysicsCollisionWorld, VisitsOverlapsInInsertionOrder)
{
    engine::physics::CollisionWorld world;
    // 故意让 id 与插入顺序相反：查询结果必须按插入顺序，而不是按 id。
    world.add(30, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(20, engine::physics::Aabb(5, 5, 10, 10), kBlocksMove);
    world.add(10, engine::physics::Aabb(8, 8, 10, 10), kBlocksMove);

    QVector<int> hits;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 40, 40), kBlocksMove, [&hits](const auto &body) {
        hits.append(body.id);
        return true;
    });

    EXPECT_EQ(hits, (QVector<int>{30, 20, 10})) << "遍历顺序必须等于插入顺序";
}

TEST(EnginePhysicsCollisionWorld, MaskFiltersBodies)
{
    engine::physics::CollisionWorld world;
    world.add(1, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(2, engine::physics::Aabb(0, 0, 10, 10), kBlocksShot);
    world.add(3, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove | kBlocksShot);

    QVector<int> moveHits;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 10, 10), kBlocksMove, [&moveHits](const auto &b) {
        moveHits.append(b.id);
        return true;
    });
    EXPECT_EQ(moveHits, (QVector<int>{1, 3}));

    QVector<int> shotHits;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 10, 10), kBlocksShot, [&shotHits](const auto &b) {
        shotHits.append(b.id);
        return true;
    });
    EXPECT_EQ(shotHits, (QVector<int>{2, 3}));

    // 没有任何标记的物体对任何 mask 都不可见
    QVector<int> none;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 10, 10), 0u, [&none](const auto &b) {
        none.append(b.id);
        return true;
    });
    EXPECT_TRUE(none.isEmpty());
}

TEST(EnginePhysicsCollisionWorld, VisitorReturningFalseStopsImmediately)
{
    engine::physics::CollisionWorld world;
    world.add(1, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(2, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(3, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);

    QVector<int> hits;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 10, 10), kBlocksMove, [&hits](const auto &b) {
        hits.append(b.id);
        return false;   // 「首个命中即返回」的写法
    });

    EXPECT_EQ(hits, (QVector<int>{1})) << "false 之后不应继续遍历";
}

TEST(EnginePhysicsCollisionWorld, NonOverlappingBodiesAreSkipped)
{
    engine::physics::CollisionWorld world;
    world.add(1, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(2, engine::physics::Aabb(100, 100, 10, 10), kBlocksMove);

    QVector<int> hits;
    world.forEachOverlap(engine::physics::Aabb(5, 5, 1, 1), kBlocksMove, [&hits](const auto &b) {
        hits.append(b.id);
        return true;
    });

    EXPECT_EQ(hits, (QVector<int>{1}));
}

TEST(EnginePhysicsCollisionWorld, RemoveAndFindByBodyId)
{
    engine::physics::CollisionWorld world;
    world.add(7, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    world.add(8, engine::physics::Aabb(0, 0, 10, 10), kBlocksMove);
    EXPECT_EQ(world.size(), 2);

    ASSERT_NE(world.find(7), nullptr);
    EXPECT_EQ(world.find(7)->id, 7);
    EXPECT_EQ(world.find(999), nullptr);

    EXPECT_TRUE(world.remove(7));
    EXPECT_FALSE(world.remove(7)) << "重复移除应返回 false";
    EXPECT_EQ(world.find(7), nullptr);
    EXPECT_EQ(world.size(), 1);

    // 剩下的物体仍可被查到 —— 顺序与内容都不受移除影响。
    QVector<int> hits;
    world.forEachOverlap(engine::physics::Aabb(0, 0, 10, 10), kBlocksMove, [&hits](const auto &b) {
        hits.append(b.id);
        return true;
    });
    EXPECT_EQ(hits, (QVector<int>{8}));

    world.clear();
    EXPECT_TRUE(world.empty());
}
