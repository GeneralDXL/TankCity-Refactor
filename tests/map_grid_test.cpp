/**
 * @file  map_grid_test.cpp
 * @brief 地图网格坐标转换的单元测试。
 *
 * M0 的首个真实单元测试：验证 Map 的世界坐标 <-> 网格坐标转换。
 *
 * **M3 更新**：格尺寸从 40 改为 50（见 map.h 的说明）—— 世界与格的期望值随之改变。
 * 这些断言与格尺寸绑定，格变了它们就该变；而「几何等价」由 `level_test.cpp` 的
 * 冻结快照守着，那一条**不该**变。
 */

#include <gtest/gtest.h>

#include "map.h"

/** 世界坐标按 GRID_SIZE 向下取整为格子坐标。 */
TEST(MapGrid, WorldToGridFloorsCell) {
    Map map;  // 构造函数不加载地图数据
    EXPECT_EQ(map.worldToGrid(QPoint(0, 0)), QPoint(0, 0));
    EXPECT_EQ(map.worldToGrid(QPoint(49, 49)), QPoint(0, 0));
    EXPECT_EQ(map.worldToGrid(QPoint(50, 50)), QPoint(1, 1));   // 40,40 现在是同一格
    EXPECT_EQ(map.worldToGrid(QPoint(40, 40)), QPoint(0, 0));
    EXPECT_EQ(map.worldToGrid(QPoint(100, 150)), QPoint(2, 3));
}

/** 网格坐标映射到格子中心点。 */
TEST(MapGrid, GridToWorldReturnsCellCenter) {
    Map map;
    EXPECT_EQ(map.gridToWorld(QPoint(0, 0)), QPoint(25, 25));
    EXPECT_EQ(map.gridToWorld(QPoint(1, 1)), QPoint(75, 75));
}

/** 先转再转回，应回到原始格子。 */
TEST(MapGrid, RoundTripIsStable) {
    Map map;
    for (int gx = 0; gx < 5; ++gx) {
        for (int gy = 0; gy < 5; ++gy) {
            const QPoint cell(gx, gy);
            EXPECT_EQ(map.worldToGrid(map.gridToWorld(cell)), cell);
        }
    }
}

/**
 * M3 的核心判据：**格必须整除世界**。
 *
 * 旧值 40 之所以要换掉，就是因为 `900 / 40 = 22.5` —— 纵向最后一格不完整，
 * 那 20px 既在网格之外、又被 A\* 用非整数边界迁就着。这条断言把「整数格」
 * 这件事钉住：谁再把格尺寸改成不能整除世界的值，测试立刻失败。
 */
TEST(MapGrid, GridTilesTheWorldExactly) {
    Map map;

    EXPECT_EQ(Map::MAP_WIDTH % Map::GRID_SIZE, 0);
    EXPECT_EQ(Map::MAP_HEIGHT % Map::GRID_SIZE, 0);

    EXPECT_EQ(map.getGridWidth(), Map::MAP_WIDTH / Map::GRID_SIZE);
    EXPECT_EQ(map.getGridHeight(), Map::MAP_HEIGHT / Map::GRID_SIZE);

    // 旧值的失败现场（留作对照）：40 能整除 1200，却除不尽 900。
    EXPECT_NE(Map::MAP_HEIGHT % 40, 0);
}
