#include <gtest/gtest.h>
#include <QList>
#include <QJsonObject>

// 模拟Qt类
class QRect {
public:
    QRect(int x, int y, int width, int height) 
        : x(x), y(y), width(width), height(height) {}
    
    bool intersects(const QRect& other) const {
        return !(x + width < other.x ||
                 other.x + other.width < x ||
                 y + height < other.y ||
                 other.y + other.height < y);
    }

    QRect adjusted(int dl, int dt, int dr, int db) const {
        return QRect(x + dl, y + dt, width - dl + dr, height - dt + db);
    }

    int x, y, width, height;
};

// 模拟Wall类
class Wall {
public:
    enum WallType { Brick = 1, Steel = 2 };

    Wall(int id, QRect rect, int type = Brick, int health = 100)
        : _id(id), _rect(rect), _type(type), _health(health) {}
    
    QRect getRect() const { return _rect; }
    bool isDestructible() const { return _type == Brick; }
    int getHealth() const { return _health; }
    void setHealth(int health) { _health = health; }
    int getId() const { return _id; }
    int getType() const { return _type; }

private:
    int _id;
    QRect _rect;
    int _type;
    int _health;
};

class Tank {};

// 被测Map类
class Map {
public:
    QList<Wall> walls;
    int nextWallId = 1;

    bool checkCollision(const QRect &rect) const {
        for (const Wall &wall : walls) {
            if (rect.intersects(wall.getRect())) {
                return true;
            }
        }
        return false;
    }

    bool checkBulletCollision(const QRect &rect) {
        for (auto it = walls.begin(); it != walls.end();) {
            if (rect.intersects(it->getRect())) {
                if (it->isDestructible()) {
                    it->setHealth(it->getHealth() - 25);
                }
                if (it->isDestructible() && it->getHealth() <= 0) {
                    it = walls.erase(it);
                    continue;
                }
                return true;
            }
            ++it;
        }
        return false;
    }

    int checkTankCollision(const QRect &rect, const Tank *tank) const {
        const int collisionMargin = 5;
        QRect adjustedRect = rect.adjusted(collisionMargin, collisionMargin,
                                        -collisionMargin, -collisionMargin);
        
        for (const Wall &wall : walls) {
            if (adjustedRect.intersects(wall.getRect())) {
                return wall.getType();
            }
        }
        return -1;
    }

    void addWall(int id, const QRect& rect, int type = Wall::Brick) {
        walls.append(Wall(id, rect, type));
    }
};

// --- 测试用例 ---

TEST(MapCollisionTest, CheckCollisionBasic) {
    Map map;
    map.addWall(1, QRect(100, 100, 50, 50));

    // 完全重叠
    EXPECT_TRUE(map.checkCollision(QRect(100, 100, 50, 50)));
    // 部分重叠
    EXPECT_TRUE(map.checkCollision(QRect(80, 80, 40, 40)));
    // 无重叠
    EXPECT_FALSE(map.checkCollision(QRect(200, 200, 50, 50)));
    // 边界触碰
    EXPECT_TRUE(map.checkCollision(QRect(150, 100, 50, 50)));
}

TEST(MapCollisionTest, CheckBulletDestruction) {
    Map map;
    map.addWall(1, QRect(100, 100, 50, 50));  // 可破坏的砖墙
    
    // 击中可破坏墙
    EXPECT_TRUE(map.checkBulletCollision(QRect(100, 100, 50, 50)));
    EXPECT_EQ(map.walls.size(), 1);
    EXPECT_EQ(map.walls[0].getHealth(), 75);

    // 4次击中后应被破坏
    for (int i = 0; i < 3; ++i) {
        map.checkBulletCollision(QRect(100, 100, 50, 50));
    }
    EXPECT_EQ(map.walls.size(), 0);
}

TEST(MapCollisionTest, CheckBulletIndestructible) {
    Map map;
    map.addWall(1, QRect(100, 100, 50, 50), Wall::Steel);
    
    // 击中不可破坏墙
    EXPECT_TRUE(map.checkBulletCollision(QRect(100, 100, 50, 50)));
    EXPECT_EQ(map.walls.size(), 1);
    EXPECT_EQ(map.walls[0].getHealth(), 100);  // 生命值不变
}

TEST(MapCollisionTest, CheckTankCollisionMargins) {
    Map map;
    map.addWall(1, QRect(100, 100, 50, 50));

    const int margin = 5;
    QRect tankRect(95, 95, 60, 60);  // 原始尺寸会碰撞

    // 检测碰撞应使用调整后的尺寸
    EXPECT_EQ(map.checkTankCollision(tankRect, nullptr), Wall::Brick);
    
    
    EXPECT_EQ(map.checkTankCollision(QRect(105, 105, 40, 40), nullptr), Wall::Brick);

    // 边界情况：刚好在边缘内不碰撞
    EXPECT_EQ(map.checkTankCollision(QRect(151, 100, 40, 40), nullptr), -1);
    
    // 边界情况：接触碰撞边界
    EXPECT_EQ(map.checkTankCollision(QRect(145, 105, 50, 50), nullptr), Wall::Brick);
}

TEST(MapCollisionTest, MultipleWallCollision) {
    Map map;
    map.addWall(1, QRect(100, 100, 50, 50), Wall::Brick);
    map.addWall(2, QRect(200, 200, 50, 50), Wall::Steel);
    
    // 应命中第一个墙
    EXPECT_TRUE(map.checkCollision(QRect(110, 110, 10, 10)));
    
    // 子弹应破坏第一个墙
    for (int i = 0; i < 4; ++i) {
        map.checkBulletCollision(QRect(110, 110, 10, 10));
    }
    
    // 坦克碰撞应命中第二个墙
    EXPECT_EQ(map.checkTankCollision(QRect(195, 195, 60, 60), nullptr), Wall::Steel);
}