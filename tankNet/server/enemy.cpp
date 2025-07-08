#include "enemy.h"
#include "game.h"
#include <QPainter>
#include <cmath>
#include <QRandomGenerator>
#include <QRectF>


Enemy::Enemy(Map *gameMap, const QPoint &position, int difficulty)
    : Tank(gameMap, 100, 0.3, 30),  // 调用基类构造函数
    difficulty(difficulty)         // 初始化派生类成员
{
    this->position = position;
    direction = static_cast<Direction>(QRandomGenerator::global()->bounded(4));
    moveTimer = 0;
    changeDirectionTimer = 0;
    shootTimer = 0;

    // 根据难度调整属性
    switch (difficulty) {
    case 0: // 简单
        speed = 0.2;
        shootDelay = 60;
        health=100;
        break;
    case 1: // 中等
        speed = 0.25;
        shootDelay = 45;
        health=150;
        break;
    case 2: // 困难
        speed = 0.3;
        shootDelay = 30;
        health=200;
        break;
    }
}

bool Enemy::canShoot() const
{
    return shootCooldown == 0 && shootTimer == 0;
}

void Enemy::update(const QPoint &playerPos, Map *map)
{
    // 如果敌人已被摧毁，跳过更新
    if (isDestroyed()) {
        return;
    }

    // 更新冷却时间
    if (shootCooldown > 0) shootCooldown--;
    if (shootTimer > 0) shootTimer--;

    // 更新炮塔角度指向玩家
    QPointF directionVec = playerPos - position;
    turretAngle = qRadiansToDegrees(qAtan2(directionVec.y(), directionVec.x()));

    // 降低转向频率：每30帧(约0.5秒)才改变一次方向
    changeDirectionTimer--;
    if (changeDirectionTimer <= 0) {
        changeDirectionTimer = 30;

        // 尝试朝玩家方向移动
        int dx = playerPos.x() - position.x();
        int dy = playerPos.y() - position.y();
        float targetAngle = qRadiansToDegrees(qAtan2(dy, dx));

        if (QRandomGenerator::global()->bounded(100) < 80) {
            // 80% 的概率尝试朝玩家移动
            // 平滑转向：每次最多调整15度
            float angleDiff = targetAngle - bodyAngle;
            // 将角度差标准化到[-180,180]范围内
            while (angleDiff > 180) angleDiff -= 360;
            while (angleDiff < -180) angleDiff += 360;

            // 限制转向速度
            float maxTurn = 15.0f;
            if (angleDiff > maxTurn) {
                bodyAngle += maxTurn;
            } else if (angleDiff < -maxTurn) {
                bodyAngle -= maxTurn;
            } else {
                bodyAngle = targetAngle;
            }
        } else {
            // 20% 的概率随机移动
            float randomAngle = QRandomGenerator::global()->bounded(360.0);
            float angleDiff = randomAngle - bodyAngle;
            // 将角度差标准化到[-180,180]范围内
            while (angleDiff > 180) angleDiff -= 360;
            while (angleDiff < -180) angleDiff += 360;

            // 限制转向速度
            float maxTurn = 10.0f;
            if (angleDiff > maxTurn) {
                bodyAngle += maxTurn;
            } else if (angleDiff < -maxTurn) {
                bodyAngle -= maxTurn;
            } else {
                bodyAngle = randomAngle;
            }
        }
    }

    // 尝试移动
    if (canMove(bodyAngle, speed * 10, map)) {
        move(bodyAngle, speed * 10, map);
    } else {
        // 如果无法移动，尝试其他方向（每30度尝试一次）
        for (int i = 0; i < 12; i++) {
            float newAngle = bodyAngle + i * 30;
            if (canMove(newAngle, speed * 10, map)) {
                // 平滑转向到新方向
                float angleDiff = newAngle - bodyAngle;
                // 将角度差标准化到[-180,180]范围内
                while (angleDiff > 180) angleDiff -= 360;
                while (angleDiff < -180) angleDiff += 360;

                // 限制转向速度
                float maxTurn = 15.0f;
                if (angleDiff > maxTurn) {
                    bodyAngle += maxTurn;
                } else if (angleDiff < -maxTurn) {
                    bodyAngle -= maxTurn;
                } else {
                    bodyAngle = newAngle;
                }

                move(bodyAngle, speed * 10, map);
                break;
            }
        }
    }

    // 简化的射击检测
    // 简化的射击检测
    if (QRandomGenerator::global()->bounded(100) < 30 && canShoot()) {
      shoot();
    }
}

void Enemy::draw(QPainter &painter)
{
    painter.save();
    painter.translate(position);
    painter.rotate(bodyAngle); // 使用车身角度旋转
    painter.rotate(270); 

    // 根据难度选择颜色
    QColor color;
    switch (difficulty) {
    case 0: color = QColor(220, 20, 60); break;   // 红色 - 简单
    case 1: color = QColor(255, 140, 0); break;   // 橙色 - 中等
    case 2: color = QColor(178, 34, 34); break;   // 深红 - 困难
    }

    // 坦克车身 - 菱形设计
    QPolygonF body;
    body << QPointF(0, -20) << QPointF(20, 0)
         << QPointF(0, 20) << QPointF(-20, 0);

    QLinearGradient bodyGrad(-20, 0, 20, 0);
    bodyGrad.setColorAt(0, color.lighter(150));
    bodyGrad.setColorAt(1, color.darker(150));

    painter.setPen(QPen(color.darker(200), 2));
    painter.setBrush(bodyGrad);
    painter.drawPolygon(body);

    // 坦克履带 - 深灰色
    painter.setBrush(QBrush(QColor(60, 60, 60)));
    painter.drawRect(-25, -15, 10, 30);  // 左履带
    painter.drawRect(15, -15, 10, 30);   // 右履带

    // 履带细节
    painter.setPen(QPen(QColor(40, 40, 40), 1));
    for (int i = -10; i <= 10; i += 5) {
        painter.drawLine(-25, i, -15, i); // 左履带细节
        painter.drawLine(15, i, 25, i);   // 右履带细节
    }

    // 炮塔基座 - 方形
    painter.setPen(QPen(color.darker(200), 2));
    painter.setBrush(QBrush(color.darker(150)));
    painter.drawRect(-8, -8, 16, 16);

    painter.restore();

    // 炮塔（独立于车身方向）
    painter.save();
    painter.translate(position);
    painter.rotate(turretAngle);

    // 炮管 - 锥形
    painter.setPen(QPen(QColor(80, 30, 30), 6));
    painter.drawLine(0, 0, 30, 0);

    QPolygonF cannon;
    cannon << QPointF(30, -3) << QPointF(35, 0) << QPointF(30, 3);
    painter.setBrush(QBrush(QColor(100, 40, 40)));
    painter.drawPolygon(cannon);

    // 炮口效果
    if (shootCooldown > 0 && shootCooldown >= shootDelay - 5) {
        painter.setPen(QPen(QColor(255, 100, 0), 4));
        painter.drawPoint(35, 0);
    }

    painter.restore();

    // 绘制难度指示器
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    QString diffChar;
    switch (difficulty) {
    case 0: diffChar = "E"; break;
    case 1: diffChar = "M"; break;
    case 2: diffChar = "H"; break;
    }
    painter.drawText(position.x() - 5, position.y() - 25, diffChar);
}
Bullet* Enemy::shoot()
    {
        if (shootCooldown > 0 || shootTimer > 0) return nullptr;

        shootCooldown = 100;
        shootTimer = shootDelay;

        // 根据炮管角度计算子弹位置
        float rad = qDegreesToRadians(turretAngle);
        QPoint bulletPos = position + QPoint(25 * cos(rad), 25 * sin(rad));

        // 获取地图指针（确保回调函数不依赖敌人实例）
        Map* mapPtr = this->gameMap;

        // 创建新子弹（使用地图指针而非this）
        return new Bullet(bulletPos, static_cast<int>(turretAngle), BulletType::Enemy,
                          [mapPtr](const QRect& rect) {  // 修改：捕获地图指针
                              return mapPtr->checkBulletCollision(rect);
                          });
    }

QRect Enemy::getRect() const
{
    return QRect(position.x() - 15, position.y() - 15, 30, 30);
}
