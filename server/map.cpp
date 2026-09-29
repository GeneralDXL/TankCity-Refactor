#include "map.h"
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonDocument>

Map::Map() : mapIndex(0)
{
}

void Map::loadMap(int index)
{
    mapIndex = index;
    walls.clear();

    // 添加边界墙（所有地图通用）
    walls.append(Wall(0, 0, MAP_WIDTH, 10, BOUNDARY)); // 上边界
    walls.append(Wall(0, 10, 10, MAP_HEIGHT-10, BOUNDARY)); // 左边界
    walls.append(Wall(10, MAP_HEIGHT-10, MAP_WIDTH-10, 10, BOUNDARY)); // 下边界
    walls.append(Wall(MAP_WIDTH-10, 10, 10, MAP_HEIGHT-20, BOUNDARY)); // 右边界


    switch (index) {
    case 0: //地图 1
    {
        walls.append(Wall(10,450,390,10,2));
        walls.append(Wall(400,450,10,225,2));
        walls.append(Wall(600,10,10,440,2));
        walls.append(Wall(200,225,400,10,2));
        walls.append(Wall(800,225,10,225,2));
        walls.append(Wall(1000,225,190,10,2));
        walls.append(Wall(600,675,10,215,2));
        walls.append(Wall(600,675,200,10,2));
        walls.append(Wall(1000,675,10,215,2));
        walls.append(Wall(1000,225,190,10,2));
        break;
    }

    case 1: // 地图 2
    {
        walls.append(Wall(10,450,150,10,2));
        walls.append(Wall(10,600,600,10,2));
        walls.append(Wall(10,750,150,10,2));
        walls.append(Wall(600,150,150,10,2));
        walls.append(Wall(450,750,150,10,2));
        walls.append(Wall(900,300,150,10,2));
        walls.append(Wall(1050,450,140,10,2));
        walls.append(Wall(1050,750,140,10,2));
        walls.append(Wall(300,10,10,290,2));
        walls.append(Wall(150,300,310,10,2));
        walls.append(Wall(150,150,10,150,2));
        walls.append(Wall(450,150,10,150,2));
        walls.append(Wall(1050,10,10,150,2));
        walls.append(Wall(600,300,10,150,2));
        walls.append(Wall(900,450,10,150,2));
        walls.append(Wall(600,755,10,135,2));
        walls.append(Wall(900,150,150,10,2));
        walls.append(Wall(300,450,600,10,2));
        walls.append(Wall(900,600,150,10,2));
        walls.append(Wall(600,750,300,10,2));
        break;
    }

    case 2: // 地图 3
    {
        //首先是竖着的
        walls.append(Wall(300,10,10,145,2));
        walls.append(Wall(750,10,10,150,2));
        walls.append(Wall(900,10,10,300,2));
        walls.append(Wall(1050,300,10,150,2));
        walls.append(Wall(150,300,10,150,2));
        walls.append(Wall(300,300,10,150,2));
        walls.append(Wall(450,300,10,590,2));
        walls.append(Wall(600,300,10,150,2));
        walls.append(Wall(900,450,10,440,2));
        walls.append(Wall(1050,600,10,150,2));
        //然后是横着的
        walls.append(Wall(300,150,300,10,2));
        walls.append(Wall(1050,150,140,10,2));
        walls.append(Wall(1050,450,140,10,2));
        walls.append(Wall(150,450,300,10,2));
        walls.append(Wall(460,600,295,10,2));
        walls.append(Wall(10,600,300,10,2));
        walls.append(Wall(600,300,150,10,2));
        walls.append(Wall(600,450,150,10,2));
        walls.append(Wall(750,750,150,10,2));
        walls.append(Wall(900,600,150,10,2));
        break;
    }

    case 3: // （有砖墙）

        {
            //首先是银墙
            walls.append(Wall(10,150,150,10,2));
            walls.append(Wall(150,300,10,300,2));
            walls.append(Wall(150,600,150,10,2));
            walls.append(Wall(300,600,10,290,2));
            walls.append(Wall(600,300,160,10,2));
            walls.append(Wall(750,150,10,150,2));
            walls.append(Wall(750,150,300,10,2));
            walls.append(Wall(900,150,10,300,2));
            walls.append(Wall(1050,10,10,300,2));
            walls.append(Wall(1050,450,10,150,2));
            //然后是砖墙
            walls.append(Wall(300,10,10,140,1));
            walls.append(Wall(300,150,10,150,1));
            walls.append(Wall(300,300,10,150,1));
            walls.append(Wall(300,150,150,10,1));
            walls.append(Wall(300,300,150,10,1));
            walls.append(Wall(450,150,150,10,1));
            walls.append(Wall(450,300,10,150,1));
            walls.append(Wall(450,450,10,150,1));
            walls.append(Wall(450,450,150,10,1));
            walls.append(Wall(600,10,10,150,1));
            walls.append(Wall(600,450,10,150,1));
            walls.append(Wall(600,600,10,150,1));
            walls.append(Wall(600,450,150,10,1));
            walls.append(Wall(600,600,150,10,1));
            walls.append(Wall(750,750,10,140,1));
            walls.append(Wall(750,600,150,10,1));
            walls.append(Wall(750,750,150,10,1));
            walls.append(Wall(900,750,150,10,1));
            walls.append(Wall(1050,450,140,10,1));
            walls.append(Wall(1050,600,140,10,1));
            walls.append(Wall(1050,750,140,10,1));
            break;
        }

    case 4: // （森林）
    {
        //然后是树林
        walls.append(Wall(160,10,1040,880,3));
        walls.append(Wall(10,150,150,740,3));
        //首先是银墙
        walls.append(Wall(150,10,10,150,2));
        walls.append(Wall(10,450,300,10,2));
        walls.append(Wall(150,150,150,10,2));
        walls.append(Wall(300,150,10,150,2));
        walls.append(Wall(150,600,10,290,2));
        walls.append(Wall(150,600,150,10,2));
        walls.append(Wall(300,600,10,160,2));
        walls.append(Wall(300,750,160,10,2));
        walls.append(Wall(450,150,10,160,2));
        walls.append(Wall(450,600,10,150,2));
        walls.append(Wall(450,150,150,10,2));
        walls.append(Wall(600,150,10,150,2));
        walls.append(Wall(600,600,10,150,2));
        walls.append(Wall(600,300,450,10,2));
        walls.append(Wall(600,600,590,10,2));
        walls.append(Wall(750,150,150,10,2));
        walls.append(Wall(900,10,10,290,2));
        walls.append(Wall(900,600,10,150,2));
        walls.append(Wall(900,450,290,10,2));
        break;
    }

    case 5: // （冰块）
    {
        //首先绘制冰块
        walls.append(Wall(150,10,1040,880,5));
        walls.append(Wall(10,150,140,740,5));
        //然后绘制所有的墙
        walls.append(Wall(450,10,10,290,2));
        walls.append(Wall(455,150,145,10,2));
        walls.append(Wall(750,10,10,140,2));
        walls.append(Wall(750,150,150,10,2));
        walls.append(Wall(1050,150,140,10,2));
        walls.append(Wall(10,450,440,10,2));
        walls.append(Wall(600,450,450,10,2));
        walls.append(Wall(300,600,10,150,2));
        walls.append(Wall(900,300,150,10,2));
        walls.append(Wall(750,300,10,310,2));
        walls.append(Wall(900,300,10,300,2));
        walls.append(Wall(150,450,10,150,2));
        walls.append(Wall(450,450,10,150,2));
        walls.append(Wall(600,600,150,10,2));
        walls.append(Wall(900,600,150,10,2));
        walls.append(Wall(600,300,150,10,2));
        walls.append(Wall(150,750,10,140,2));
        walls.append(Wall(450,750,10,140,2));
        walls.append(Wall(600,750,450,10,2));
        walls.append(Wall(750,750,10,140,2));
        break;
    }

    case 6: // 海洋地图
    {
       
        int oceanWidth = 200;
        int oceanX = (MAP_WIDTH - oceanWidth) / 2;
       
        int gapWidth = 40;
     
        int upperRightGapX = oceanX + oceanWidth - gapWidth;
     
        int lowerRightGapX = oceanX + oceanWidth - gapWidth;
       
        walls.append(Wall(500,120,200,640,4));
        //然后是刚墙
        walls.append(Wall(oceanX - 30, 140, 30, 40, 2));
        walls.append(Wall(oceanX + gapWidth, 140, 30, 40, 2));
        walls.append(Wall(upperRightGapX - 30, 140, 30, 40, 2));
        walls.append(Wall(upperRightGapX + gapWidth, 140, 30, 40, 2));
        walls.append(Wall(oceanX - 30, MAP_HEIGHT - 80, 30, 40, 2));
        walls.append(Wall(oceanX + gapWidth, MAP_HEIGHT - 80, 30, 40, 2));
        walls.append(Wall(lowerRightGapX - 30, MAP_HEIGHT - 80, 30, 40, 2));
        walls.append(Wall(lowerRightGapX + gapWidth, MAP_HEIGHT - 80, 30, 40, 2));
        walls.append(Wall(200, 200, 40, 40, 2));
        walls.append(Wall(100, 400, 60, 20,2));
        walls.append(Wall(150, 600, 40, 40,2));
        walls.append(Wall(MAP_WIDTH - 250, 200, 40, 40, 2));
        walls.append(Wall(MAP_WIDTH - 180, 400, 60, 20, 2));
        walls.append(Wall(MAP_WIDTH - 200, 600, 40, 40, 2));
        walls.append(Wall(oceanX - 100, 300, 40, 300, 2));
        walls.append(Wall(oceanX + oceanWidth + 60, 300, 40, 300, 2));
        break;
    }

    case 7:
    {
        int oceanHeight = 150;
        walls.append(Wall(210, 10, MAP_WIDTH - 420, oceanHeight, SEA)); // 上海洋
        walls.append(Wall(10, MAP_HEIGHT - oceanHeight - 10, MAP_WIDTH - 20, oceanHeight, SEA)); // 下海洋
        int middleHeight = MAP_HEIGHT - 2 * oceanHeight - 20;
        int middleY = oceanHeight + 10;
        int forestWidth = 200;
        walls.append(Wall(10, 10, forestWidth, middleHeight+150, FOREST));
        int iceWidth = 200;
        walls.append(Wall(MAP_WIDTH - iceWidth - 10, 10, iceWidth, middleHeight+150, ICE));
        int channelWidth = MAP_WIDTH - 20 - forestWidth - iceWidth;
        int channelX = forestWidth + 10;
        walls.append(Wall(channelX + 50, middleY + 50, 30, 100, STEEL));
        walls.append(Wall(channelX + channelWidth - 80, middleY + 50, 30, 100, STEEL));
        walls.append(Wall(channelX + (channelWidth - 30) / 2, middleY + 150, 30, 200, STEEL));
        walls.append(Wall(channelX + (channelWidth - 120) / 2, middleY + 250, 120, 30, STEEL));
        int baseY = MAP_HEIGHT - oceanHeight - 80;
        walls.append(Wall(channelX + (channelWidth - 80) / 2, baseY, 30, 30, STEEL));
        walls.append(Wall(channelX + (channelWidth - 80) / 2 + 50, baseY, 30, 30, STEEL));
        walls.append(Wall(channelX + (channelWidth - 80) / 2, baseY + 30, 80, 30, STEEL));
        break;
    }

    case 8:
    {
        int oceanHeight = 150;
        walls.append(Wall(10, 10, MAP_WIDTH-20, oceanHeight, SEA)); // 上部海洋
        walls.append(Wall(10, MAP_HEIGHT-oceanHeight-10, MAP_WIDTH-20, oceanHeight, SEA)); // 下部海洋
        int forestWidth = MAP_WIDTH * 0.4;
        int forestX = (MAP_WIDTH - forestWidth) / 2;
        int forestHeight = MAP_HEIGHT - 2*oceanHeight - 20;
        walls.append(Wall(forestX, oceanHeight+10, forestWidth, forestHeight, FOREST));
        int iceWidth = 100;
        walls.append(Wall(forestX - iceWidth, oceanHeight+10, iceWidth, forestHeight, ICE)); // 左侧冰块
        walls.append(Wall(forestX + forestWidth, oceanHeight+10, iceWidth, forestHeight, ICE)); // 右侧冰块
        int centerX = MAP_WIDTH/2;
        int centerY = MAP_HEIGHT/2;
        walls.append(Wall(centerX-30, centerY-60, 60, 30, STEEL)); // 横向钢墙
        walls.append(Wall(centerX-15, centerY-30, 30, 120, STEEL)); // 纵向钢墙
        walls.append(Wall(forestX+30, oceanHeight+80, 30, 100, BRICK));
        walls.append(Wall(forestX+80, oceanHeight+180, 30, 100, STEEL));
        walls.append(Wall(forestX+forestWidth-60, oceanHeight+80, 30, 100, BRICK));
        walls.append(Wall(forestX+forestWidth-110, oceanHeight+180, 30, 100, STEEL));
        walls.append(Wall(centerX-40, MAP_HEIGHT-oceanHeight-80, 30, 30, STEEL));
        walls.append(Wall(centerX+10, MAP_HEIGHT-oceanHeight-80, 30, 30, STEEL));
        walls.append(Wall(centerX-40, MAP_HEIGHT-oceanHeight-50, 80, 30, STEEL));

        break;
    }

    case 9: //
    {
        // 移除默认边界墙（保留原有边界）
        // 中央圆形海洋（由多个矩形近似）
        int centerX = MAP_WIDTH / 2;
        int centerY = MAP_HEIGHT / 2;
        int oceanRadius = 200;  // 海洋半径


        // 添加更多海洋方块使中心区域更完整
        for (int r = 160; r < oceanRadius; r += 40) {
            for (int angle = 0; angle < 360; angle += 30) {
                double rad = qDegreesToRadians(angle);
                int x = centerX + r * cos(rad) - 10;
                int y = centerY + r * sin(rad) - 10;
                walls.append(Wall(x, y, 20, 20, SEA));
            }
        }

        // 添加中央大块海洋
        walls.append(Wall(centerX - 100, centerY - 100, 200, 200, SEA));

        // 环绕海洋的森林区域（外环）
        int forestThickness = 150;
        walls.append(Wall(10, 10, MAP_WIDTH - 20, forestThickness, FOREST)); // 上森林
        walls.append(Wall(10, MAP_HEIGHT - forestThickness - 10, MAP_WIDTH - 20, forestThickness, FOREST)); // 下森林
        walls.append(Wall(10, forestThickness + 10, forestThickness, MAP_HEIGHT - 2 * forestThickness - 20, FOREST)); // 左森林
        walls.append(Wall(MAP_WIDTH - forestThickness - 10, forestThickness + 10, forestThickness, MAP_HEIGHT - 2 * forestThickness - 20, FOREST)); // 右森林

        // 在森林和海洋之间添加冰块过渡带
        int iceWidth = 80;
        walls.append(Wall(forestThickness + 10, forestThickness + 10, MAP_WIDTH - 2 * forestThickness - 20, iceWidth, ICE)); // 上冰块
        walls.append(Wall(forestThickness + 10, MAP_HEIGHT - forestThickness - iceWidth - 10, MAP_WIDTH - 2 * forestThickness - 20, iceWidth, ICE)); // 下冰块
        walls.append(Wall(forestThickness + 10, forestThickness + iceWidth + 10, iceWidth, MAP_HEIGHT - 2 * (forestThickness + iceWidth) - 20, ICE)); // 左冰块
        walls.append(Wall(MAP_WIDTH - forestThickness - iceWidth - 10, forestThickness + iceWidth + 10, iceWidth, MAP_HEIGHT - 2 * (forestThickness + iceWidth) - 20, ICE)); // 右冰块

        // 添加角落冰块
        walls.append(Wall(forestThickness + 10, forestThickness + 10, iceWidth, iceWidth, ICE)); // 左上角
        walls.append(Wall(MAP_WIDTH - forestThickness - iceWidth - 10, forestThickness + 10, iceWidth, iceWidth, ICE)); // 右上角
        walls.append(Wall(forestThickness + 10, MAP_HEIGHT - forestThickness - iceWidth - 10, iceWidth, iceWidth, ICE)); // 左下角
        walls.append(Wall(MAP_WIDTH - forestThickness - iceWidth - 10, MAP_HEIGHT - forestThickness - iceWidth - 10, iceWidth, iceWidth, ICE)); // 右下角

        // 零散分布的小钢墙（共12个）
        walls.append(Wall(150, 150, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 180, 150, 30, 30, STEEL));
        walls.append(Wall(150, MAP_HEIGHT - 180, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 180, MAP_HEIGHT - 180, 30, 30, STEEL));

        walls.append(Wall(400, 100, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 430, 100, 30, 30, STEEL));
        walls.append(Wall(400, MAP_HEIGHT - 130, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 430, MAP_HEIGHT - 130, 30, 30, STEEL));

        walls.append(Wall(100, 400, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 130, 400, 30, 30, STEEL));
        walls.append(Wall(100, MAP_HEIGHT - 430, 30, 30, STEEL));
        walls.append(Wall(MAP_WIDTH - 130, MAP_HEIGHT - 430, 30, 30, STEEL));

        break;
    }
    }
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
