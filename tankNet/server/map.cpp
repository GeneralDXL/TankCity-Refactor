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

    //边界
    walls.append(Wall(0, 0, 800, 20, BOUNDARY));          // Top
    walls.append(Wall(0, 580, 800, 20, BOUNDARY));        // Bottom
    walls.append(Wall(0, 20, 20, 560, BOUNDARY));         // Left
    walls.append(Wall(780, 20, 20, 560, BOUNDARY));       // Right


    switch (index) {
    case 0:
        for (int i = 0; i < 15; i++) {
            int x = QRandomGenerator::global()->bounded(100, 700);
            int y = QRandomGenerator::global()->bounded(100, 500);
            walls.append(Wall(x, y, 40, 40, BRICK));
        }
        break;

    case 1:

        for (int i = 0; i < 5; i++) {
            int x = 100 + i * 120;
            walls.append(Wall(x, 100, 80, 80, BRICK));
            walls.append(Wall(x, 400, 80, 80, BRICK));
        }


        walls.append(Wall(0, 300, 300, 20, BOUNDARY));
        walls.append(Wall(500, 300, 300, 20, BOUNDARY));
        walls.append(Wall(350, 150, 20, 200, BOUNDARY));
        walls.append(Wall(350, 400, 20, 200, BOUNDARY));
        break;

    case 2:
        for (int i = 0; i < 20; i++) {
            int x = QRandomGenerator::global()->bounded(50, 750);
            int y = QRandomGenerator::global()->bounded(50, 550);
            walls.append(Wall(x, y, 30, 30, BRICK));
        }


        walls.append(Wall(200, 200, 100, 100, BRICK));
        walls.append(Wall(500, 300, 100, 100, BRICK));
        break;

    case 3:

        for (int i = 0; i < 10; i++) {
            int x = QRandomGenerator::global()->bounded(100, 700);
            int y = QRandomGenerator::global()->bounded(100, 500);
            walls.append(Wall(x, y, 60, 60, BRICK));
        }


        walls.append(Wall(100, 100, 20, 400, BOUNDARY));
        walls.append(Wall(700, 100, 20, 400, BOUNDARY));
        walls.append(Wall(200, 250, 400, 20, BOUNDARY));
        break;

    case 4:

        for (int i = 0; i < 25; i++) {
            int x = QRandomGenerator::global()->bounded(50, 750);
            int y = QRandomGenerator::global()->bounded(50, 550);
            walls.append(Wall(x, y, 20, 20, BRICK));
        }


        walls.append(Wall(100, 100, 150, 50, BRICK));
        walls.append(Wall(550, 100, 150, 50, BRICK));
        walls.append(Wall(100, 450, 150, 50, BRICK));
        walls.append(Wall(550, 450, 150, 50, BRICK));
        walls.append(Wall(300, 250, 200, 100, BRICK));
        break;
    }
}

void Map::draw(QPainter &painter)
{

    painter.setBrush(QBrush(QColor(100, 100, 200)));
    for (const Wall &wall : walls) {
        painter.drawRect(wall.getRect());
    }


    QColor obstacleColor;
    switch (mapIndex) {
    case 0: obstacleColor = QColor(150, 150, 150); break; // Gray rocks
    case 1: obstacleColor = QColor(120, 80, 50); break;   // Brown buildings
    case 2: obstacleColor = QColor(210, 180, 140); break; // Sand
    case 3: obstacleColor = QColor(200, 230, 255); break; // Ice
    case 4: obstacleColor = QColor(34, 139, 34); break;   // Forest green
    }

    painter.setBrush(QBrush(obstacleColor));
    for (const Wall &wall : walls) {
        QRect rect = wall.getRect();
        painter.drawRect(rect);


        if (mapIndex == 4) {
            painter.setBrush(QBrush(QColor(139, 69, 19)));
            painter.drawRect(rect.x() + 7, rect.y() + 7, 6, 13);
            painter.setBrush(QBrush(QColor(0, 100, 0)));
            painter.drawEllipse(rect.x() - 5, rect.y() - 5, 30, 20);
        }
        else if (mapIndex == 1) {
            painter.setBrush(QBrush(QColor(200, 230, 255)));
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++) {
                    painter.drawRect(rect.x() + 10 + i*20, rect.y() + 10 + j*20, 15, 15);
                }
            }
        }
    }
}

bool Map::checkCollision(const QRect &rect) const
{
    // 移除所有边界调整，使用精确检测
    // 直接检查原矩形
    for (const Wall &wall : walls) {
        if (rect.intersects(wall.getRect())) {
            return true;
        }
    }

    return false;
}

bool Map::checkBulletCollision(const QRect &rect)
{
    // 移除所有边界调整，使用精确检测
    // 直接检查原矩形
    for (Wall &wall : walls) {
        if (rect.intersects(wall.getRect())) {
            if(wall.isDestructible()){
                wall.setHealth(wall.getHealth() - 25);
            }
            if(wall.isDestructible() && wall.getHealth() <= 0){
                QJsonObject json;
                json["type"] = "delete_wall";
                json["wallId"] = wall.getId();
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
    // 对于坦克等大物体使用带边距的检测
    int collisionMargin = 5;
    QRect adjustedRect = rect.adjusted(collisionMargin, collisionMargin,
                                        -collisionMargin, -collisionMargin);

    for (const Wall &wall : walls) {
        if (adjustedRect.intersects(wall.getRect())) {
            return wall.getType(); // 返回墙的类型
        }
    }

    return -1;
}
