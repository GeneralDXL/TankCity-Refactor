#include "player.h"
#include <QtMath>
#include "bullet.h"
#include <QDebug>
#include <cmath>
#include<QDateTime>

Player::Player(Map *gameMap, const tankcity::config::TankStats &stats)
    : Tank(gameMap, stats), maxHealth(stats.health)
{
    // 初始化成员变量
    position = QPoint(0, 0);
    bodyAngle = 0; // 初始角度向上
    health = maxHealth;
    shootCooldown = 0;
    turretAngle = 0;
    this->gameMap = gameMap;
}

void Player::init(int x, int y)
{
    position = QPoint(x, y);
    bodyAngle = 0; // 初始角度向上
    health = maxHealth;
    shootCooldown = 0;
    turretAngle = 0;
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
    int offsetX = static_cast<int>(muzzleOffset * cos(rad));
    int offsetY = static_cast<int>(muzzleOffset * sin(rad));
    QPoint bulletPos = position + QPoint(offsetX, offsetY);

    // 创建新子弹（速度与伤害来自 entities.json 的子弹原型）
    // 子弹带走自己的完整属性（M4 步 2：伤害 / 标签 / 可弹清单 / 弹数）。
    // 命中判定因此只需一个参数；M4.5 的弹药队列换一份 profile 即可，判定方一行不改。
    return new Bullet(bulletPos, turretAngle, BulletType::Player, bulletSpeed,
                      bulletProfile_,
                      [this](const QRect& rect, const tankcity::config::BulletProfile &profile) {
                          return this->gameMap->checkBulletCollision(rect, profile);
                      });
}


void Player::updateCooldown()
{
    if (shootCooldown > 0) {
        shootCooldown--;
    }
}

void Player::update()
{
    updateTurretAim();
    updateMovement();
}

void Player::updateTurretAim()
{
    // 炮塔始终指向瞄准点。与旧实现逐字对应 —— 包括 `!isNull()` 这道门槛：
    // 瞄准点为 (0,0) 时不改角度。本步不顺手改这个口径（世界左上角恰好是 (0,0)，
    // 要改成"是否有过瞄准点"得另开一个标记，属另一件事）。
    if (!aimPoint.isNull()) {
        const float dx = static_cast<float>(aimPoint.x() - position.x());
        const float dy = static_cast<float>(aimPoint.y() - position.y());
        setTurretAngle(qRadiansToDegrees(atan2(dy, dx)));
    }
}

void Player::updateMovement()
{
    // 冷却每帧走一格。这一步原来藏在 `Tank::move()` 里（移动时推进、静止时由这里补），
    // M3 步 4b 把它从 move() 移出来改成无条件推进 —— 每帧仍恰好减一次。
    updateCooldown();

    if (moveVector.isNull())
        return;

    // 八向归一化：数字键的斜向送来 (±1, ±1)，模长 √2 —— 不归一化的话斜着走会快 41%。
    // 归一化放在**服务端**（权威端）而不是客户端：「走多快」是规则，不是输入；
    // 而且摇杆将来送来的是模长 ≤1 的连续值，这里只削峰、不放大，一并兼容。
    QPointF v = moveVector;
    double length = std::hypot(v.x(), v.y());
    if (length > 1.0) {
        v /= length;
        length = 1.0;
    }

    // 车体朝向 = 移动方向。这是相对旧「坦克式」的**手感变化**，也是本里程碑的核心：
    //   - A/D 不再单独转身 —— 车身随移动方向立刻指向该方向；
    //   - S 不再是半速倒车 —— 八向下「往后走」就是车头转向后、正常速度走。
    // 依据 M3 决议：双摇杆下车体唯一的职责就是表达"往哪走"，转向不再是独立自由度。
    bodyAngle = static_cast<float>(qRadiansToDegrees(atan2(v.y(), v.x())));
    bodyAngle = fmod(bodyAngle + 360.0f, 360.0f);

    move(bodyAngle, static_cast<float>(speed * length), gameMap);
}
