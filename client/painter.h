#ifndef PAINTER_H
#define PAINTER_H

#include <QPainter>
#include <QColor>
#include <QVector>

class WallPainter {
public:
    WallPainter(int x, int y, int width, int height, int type, int id);
    QRect getRect() const { return QRect(x, y, width, height); } // 获取矩形区域
    int getId() const { return id; } // 获取墙的唯一ID
    int getType()const {return type;};

private:
    int type;   // 墙的类型
    int x;      // x坐标(左上)
    int y;      // y坐标(左上)
    int width;  // 宽度
    int height; // 高度
    int id;
};

class MapPainter {
public:
    MapPainter();
    void draw(QPainter &painter);
    void clearWalls() { walls.clear(); }
    void addWall(const WallPainter &wall) { walls.append(wall); }
    void removeWall(int id) {
        for (int i = 0; i < walls.size(); ++i) {
            if (walls[i].getId() == id) { // Assuming id is used as x coordinate for simplicity
                walls.removeAt(i);
                break;
            }
        }
    }

private:
    QVector<WallPainter> walls;
    int mapIndex;
};

class TankPainter {
public:
    TankPainter(int health, int shootDelay);
    virtual void draw(QPainter &painter) = 0;
    int getHealth() const { return health; }
    QPoint getPosition() const { return position; }
    void setPosition(const QPoint &pos) { position = pos; }
    void setBodyAngle(float angle) { bodyAngle = angle; }
    void setTurretAngle(float angle) { turretAngle = angle; }
    void setHealth(int hp) { health = hp; }
    void setId(int tankId) { id = tankId; }
    int getId() const { return id; }

protected:
    QPoint position;
    float bodyAngle; // 车身角度
    float turretAngle = 0;  // 炮管角度
    int shootCooldown; // 射击冷却时间
    int shootDelay; // 射击延迟，单位毫秒
    int health; // 生命值
    int id;
};

class PlayerPainter : public TankPainter {
public:
    PlayerPainter();
    void draw(QPainter &painter);
    void setInvincible(bool inv) { invincible = inv; } // -----------加
private:
      bool invincible = false;//--------------加
};

class EnemyPainter : public TankPainter {
public:
    EnemyPainter(int difficulty);
    void draw(QPainter &painter);

private:
    int difficulty;
};

enum class BulletPainterType {         //枚举子弹类型
    Player,
    Enemy
};

class BulletPainter {
public:
    BulletPainter(const QPoint &position, float angle, BulletPainterType type);
    void draw(QPainter &painter) const;
    float getAngle() const { return angle; }

private:
    QPoint position; // 子弹位置
    float angle; // 子弹角度
    BulletPainterType type; // 子弹类型
};

#endif // PAINTER_H
