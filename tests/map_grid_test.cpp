/**
 * @file  map_grid_test.cpp
 * @brief 地图网格坐标转换的单元测试。
 *
 * M0 的首个真实单元测试：验证 Map 的世界坐标 <-> 网格坐标转换。
 * 网格尺寸 GRID_SIZE = 40，见 map.h。
 */

#include <gtest/gtest.h>

#include "map.h"

/** 世界坐标按 GRID_SIZE 向下取整为格子坐标。 */
TEST(MapGrid, WorldToGridFloorsCell) {
    Map map;  // 构造函数不加载地图数据
    EXPECT_EQ(map.worldToGrid(QPoint(0, 0)), QPoint(0, 0));
    EXPECT_EQ(map.worldToGrid(QPoint(39, 39)), QPoint(0, 0));
    EXPECT_EQ(map.worldToGrid(QPoint(40, 40)), QPoint(1, 1));
    EXPECT_EQ(map.worldToGrid(QPoint(80, 120)), QPoint(2, 3));
}

/** 网格坐标映射到格子中心点。 */
TEST(MapGrid, GridToWorldReturnsCellCenter) {
    Map map;
    EXPECT_EQ(map.gridToWorld(QPoint(0, 0)), QPoint(20, 20));
    EXPECT_EQ(map.gridToWorld(QPoint(1, 1)), QPoint(60, 60));
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
