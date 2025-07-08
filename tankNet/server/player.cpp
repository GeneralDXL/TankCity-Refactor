#include "player.h"
#include <QPainter>
#include "bullet.h"
#include <QDebug>
#include <cmath>

Player::Player(Map *gameMap) : Tank(gameMap, 100, 5.0f, 10)
{
    // 初始化成员变量
    position = QPoint(0, 0);
    bodyAngle = 0; // 初始角度向上
    health = 100;
    shootCooldown = 0;
    turretAngle = 0;
    this->gameMap = gameMap;
}

void Player::init(int x, int y)
{
    position = QPoint(x, y);
    bodyAngle = 0; // 初始角度向上
    health = 100;
    shootCooldown = 0;
    turretAngle = 0;
}

void Player::draw(QPainter &painter)
{
    painter.save();
    painter.translate(position);
    painter.rotate(bodyAngle); // 使用车身角度旋转
    painter.rotate(270);        // 新增：整体顺时针旋转90°

    // 坦克车身 - 蓝色主题
    QRect bodyRect(-15, -10, 30, 20); // 尺寸缩小
    QLinearGradient bodyGrad(bodyRect.topLeft(), bodyRect.bottomRight());
    bodyGrad.setColorAt(0, QColor(80, 140, 255)); // 更亮的蓝色
    bodyGrad.setColorAt(1, QColor(40, 90, 200));  // 更深的蓝色

    painter.setPen(QPen(Qt::darkBlue, 1.5)); // 更细的边框
    painter.setBrush(bodyGrad);
    painter.drawRoundedRect(bodyRect, 4, 4); // 圆角更小

    // 坦克履带 - 深灰色，方向反转
    painter.setBrush(QBrush(QColor(50, 50, 50))); // 更深的灰色
    painter.drawRect(-18, -12, 6, 24);  // 左履带 (更窄)
    painter.drawRect(12, -12, 6, 24);   // 右履带 (更窄)

    // 履带细节 - 垂直方向
    painter.setPen(QPen(QColor(30, 30, 30), 1)); // 更深的细节色
    for (int y = -10; y <= 10; y += 5) {
        painter.drawLine(-18, y, -12, y); // 左履带细节 (垂直)
        painter.drawLine(12, y, 18, y);   // 右履带细节 (垂直)
    }

    // 炮塔基座 - 圆形
    painter.setPen(QPen(Qt::darkBlue, 1.5));
    painter.setBrush(QBrush(QColor(70, 120, 230))); // 匹配车身的蓝色
    painter.drawEllipse(-8, -8, 16, 16); // 尺寸缩小

    painter.restore();

    // 炮塔（独立于车身方向）
    painter.save();
    painter.translate(position);
    painter.rotate(turretAngle);
    // painter.rotate(90); // 新增：炮塔也要整体旋转90°

    // 炮管 - 更细的锥形
    painter.setPen(QPen(QColor(40, 60, 110), 4)); // 更细的炮管
    painter.drawLine(0, 0, 24, 0); // 缩短长度

    QPolygonF cannon;
    cannon << QPointF(24, -2) << QPointF(28, 0) << QPointF(24, 2); // 更小的锥形
    painter.setBrush(QBrush(QColor(60, 80, 130))); // 更深的蓝色
    painter.drawPolygon(cannon);

    // 炮口效果
    if (shootCooldown > 0 && shootCooldown >= shootDelay - 5) {
        painter.setPen(QPen(QColor(255, 220, 120), 3)); // 更亮的闪光
        painter.drawPoint(28, 0);
    }

    painter.restore();
}

void Player::setTurretAngle(float angle)
{
    turretAngle = angle;
}

bool Player::canShoot() const
{
    // 射击冷却时间结束即可射击
    return shootCooldown <= 0;
}

Bullet* Player::shoot()
{
    if (!canShoot()) {
        qDebug() << "射击冷却中，无法发射";
        return nullptr;
    }

    shootCooldown = shootDelay;

    // 计算子弹发射位置
    float rad = qDegreesToRadians(turretAngle);
    int offsetX = static_cast<int>(25 * cos(rad));
    int offsetY = static_cast<int>(25 * sin(rad));
    QPoint bulletPos = position + QPoint(offsetX, offsetY);

    // 创建新子弹
    return new Bullet(bulletPos, turretAngle, BulletType::Player,
                      [this](const QRect& rect) {
                          return this->gameMap->checkBulletCollision(rect);
                      });
}


void Player::updateCooldown()
{
    if (shootCooldown > 0) {
        shootCooldown--;
    }
}

QRect Player::getRect() const
{
    // 调整碰撞框大小以匹配新尺寸
    return QRect(position.x() - 15, position.y() - 15, 30, 30);
}

void Player::setMoveForward(bool forward)
{
    if (forward) {
        pressedKeys.insert(Qt::Key_W);
    } else {
        pressedKeys.remove(Qt::Key_W);
    }
}

void Player::setMoveBackward(bool backward)
{
    if (backward) {
        pressedKeys.insert(Qt::Key_S);
    } else {
        pressedKeys.remove(Qt::Key_S);
    }
}

void Player::setTurnLeft(bool left)
{
    if (left) {
        pressedKeys.insert(Qt::Key_A);
    } else {
        pressedKeys.remove(Qt::Key_A);
    }
}

void Player::setTurnRight(bool right)
{
    if (right) {
        pressedKeys.insert(Qt::Key_D);
    } else {
        pressedKeys.remove(Qt::Key_D);
    }
}

bool Player::isMoveForward() const
{
    return pressedKeys.contains(Qt::Key_W);
}

bool Player::isMoveBackward() const
{
    return pressedKeys.contains(Qt::Key_S);
}

bool Player::isTurnLeft() const
{
    return pressedKeys.contains(Qt::Key_A);
}

bool Player::isTurnRight() const
{
    return pressedKeys.contains(Qt::Key_D);
}

void Player::update()
{
    // 更新炮塔角度（始终指向鼠标位置）
    if (!mousePos.isNull()) {
        float dx = mousePos.x() - position.x();
        float dy = mousePos.y() - position.y();
        float angle = qRadiansToDegrees(atan2(dy, dx));
        setTurretAngle(angle);
    }

    // 处理车身旋转
    if (isTurnLeft() && !isTurnRight()) {
        bodyAngle += 5.0f;  // 逆时针旋转
    } else if (isTurnRight() && !isTurnLeft()) {
        bodyAngle -= 5.0f;  // 顺时针旋转
    }

    // 规范化角度到0-360范围
    bodyAngle = fmod(bodyAngle + 360.0f, 360.0f);

    // 处理移动
    if (isMoveForward() && !isMoveBackward()) {
        // 向前移动
        move(bodyAngle, speed, gameMap);
    } else if (isMoveBackward() && !isMoveForward()) {
        // 向后移动（速度减半）
        move(bodyAngle + 180, speed * 0.5f, gameMap);
    } else {
        // 没有移动时更新冷却
        updateCooldown();
    }
}