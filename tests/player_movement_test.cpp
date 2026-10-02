/**
 * @file  player_movement_test.cpp
 * @brief M3 双摇杆：速度向量推进的单元测试。
 *
 * 把「八向平滑移动」与「炮塔独立瞄准」里**能自动验证**的部分钉死；剩下的
 * 「手感」只能由 GeneralDXL 试玩确认（M3 草案 §1 第 3 条，唯一的人工判据）。
 *
 * 为什么这些用例不需要配置或关卡文件：空 `Map` 上
 *  - `getMoveSpeedFactor()` 找不到物块，返回 1.0（无地形影响）；
 *  - `checkTankCollision()` 找不到物块，返回 -1（无碰撞）；
 * 于是 `Tank::move()` 退化为纯运动学 —— 正是本文件要验证的那部分。
 * `TankStats` 是纯数据结构，直接构造即可，不必走 ConfigLoader。
 */

#include <gtest/gtest.h>

#include "config/TankStats.h"
#include "map.h"
#include "player.h"

#include <cmath>
#include <memory>

namespace {

/// 速度取整数 5，使「每帧 5px」的期望值精确可断言（见下面的取整说明）。
tankcity::config::TankStats makeStats(double moveSpeed = 5.0, int box = 40)
{
    tankcity::config::TankStats stats;
    stats.health = 10;
    stats.moveSpeed = moveSpeed;
    stats.shootDelayTicks = 10;
    stats.bulletSpeed = 10.0;
    stats.bulletDamage = 1;
    stats.muzzleOffset = 20;
    stats.collisionBoxW = box;
    stats.collisionBoxH = box;
    return stats;
}

/// 在一个没有墙的空地图上，把玩家放在世界中央。
std::unique_ptr<Player> makePlayer(Map *map, double moveSpeed = 5.0, int box = 40)
{
    auto player = std::make_unique<Player>(map, makeStats(moveSpeed, box));
    player->init(Map::MAP_WIDTH / 2, Map::MAP_HEIGHT / 2);
    return player;
}

/// 朝 v 方向推进 frames 帧之后与原点的距离。
double travelDistance(Map *map, const QPointF &v, int frames, QPoint *outDelta = nullptr)
{
    auto player = makePlayer(map);
    const QPoint start = player->getPosition();
    for (int i = 0; i < frames; ++i) {
        player->setMoveVector(v);
        player->update();
    }
    const QPoint delta = player->getPosition() - start;
    if (outDelta != nullptr)
        *outDelta = delta;
    return std::hypot(static_cast<double>(delta.x()), static_cast<double>(delta.y()));
}

} // namespace

/**
 * 直行 100 帧走的距离 = 速度 × 帧数。
 *
 * 这条是基准：档位速度 5 → 500px。整数坐标对 cardinal 方向是精确的
 * （cos(0°)=1、sin(0°)=0），所以可以写等号。
 */
TEST(PlayerMovement, CardinalMoveTravelsAtMoveSpeed) {
    Map map;
    QPoint delta;
    const double distance = travelDistance(&map, QPointF(1, 0), 100, &delta);

    EXPECT_EQ(delta, QPoint(500, 0));
    EXPECT_DOUBLE_EQ(distance, 500.0);
}

/**
 * **斜向不比直行快** —— 归一化的核心断言。
 *
 * 原始输入是 (1,1)，模长 √2；若不做归一化，斜着走会比直行快 41%（老游戏里的经典利用点）。
 * 这里同时给出上界（不许快）与下界（不许慢得离谱）。
 *
 * 下界之所以只能给到 0.8：`Tank::move()` 把位移取整到整数像素
 * （`QPoint(moveX, moveY)` 是**截断**而非四舍五入），斜向每帧 3.5355 被截成 3，
 * 两个轴各亏 0.5355px/帧 → 100 帧后 424px 而不是理想的 353.6×√2 = 500。
 * 即「斜向略慢约 15%」而不是「略快」。步 3b 重写移动判定时会顺带带上小数余量，
 * 届时这个下界仍成立（那时斜向会精确到 ~500）。
 */
TEST(PlayerMovement, DiagonalIsNotFasterThanCardinal) {
    Map map;
    const double cardinal = travelDistance(&map, QPointF(1, 0), 100);
    const double diagonal = travelDistance(&map, QPointF(1, 1), 100);

    EXPECT_LE(diagonal, cardinal * 1.02) << "斜向移动不得快于直行（归一化失效）";
    EXPECT_GE(diagonal, cardinal * 0.80) << "斜向慢得超出取整误差，检查归一化是否把模长削过头";
}

/**
 * 八向：车体朝向 = 移动方向。
 *
 * 旧「坦克式」是 A/D 单独转身、车身朝向与移动方向无关；双摇杆下车身唯一的职责
 * 就是表达往哪走（M3 草案：`Player::update` 改为速度向量推进）。
 * 屏幕 y 向下，故 (0,-1) 是正上方 → 270°（与旧代码 bodyAngle 的取值域 0–360 一致）。
 */
TEST(PlayerMovement, BodyFollowsMovementDirection) {
    Map map;

    struct Case { QPointF v; float expected; const char *name; };
    const Case cases[] = {
        {QPointF( 1,  0),   0.0f, "右"},
        {QPointF( 1,  1),  45.0f, "右下"},
        {QPointF( 0,  1),  90.0f, "下"},
        {QPointF(-1,  1), 135.0f, "左下"},
        {QPointF(-1,  0), 180.0f, "左"},
        {QPointF(-1, -1), 225.0f, "左上"},
        {QPointF( 0, -1), 270.0f, "上"},
        {QPointF( 1, -1), 315.0f, "右上"},
    };

    for (const Case &c : cases) {
        auto player = makePlayer(&map);
        player->setMoveVector(c.v);
        player->update();
        EXPECT_NEAR(player->getBodyAngle(), c.expected, 0.01f) << c.name << "：车身未跟随移动方向";
    }
}

/**
 * 后退不再半速。
 *
 * 旧实现：`S` 是「车头不变、沿 bodyAngle+180 以 speed*0.5 倒车」。
 * 双摇杆下 `S` 就是「往下走」——车头转到 90°、正常速度。
 */
TEST(PlayerMovement, BackwardIsNotHalfSpeed) {
    Map map;
    QPoint delta;
    travelDistance(&map, QPointF(0, 1), 100, &delta);

    EXPECT_EQ(delta, QPoint(0, 500)) << "S 仍是半速倒车（旧手感未被替换）";
}

/** 松开方向键：位置不动，车头保持最后朝向（不归零、不自己转）。 */
TEST(PlayerMovement, StoppedPlayerKeepsHeading) {
    Map map;
    auto player = makePlayer(&map);

    player->setMoveVector(QPointF(0, -1));
    player->update();
    const QPoint moved = player->getPosition();
    const float heading = player->getBodyAngle();

    player->setMoveVector(QPointF(0, 0));
    for (int i = 0; i < 10; ++i)
        player->update();

    EXPECT_EQ(player->getPosition(), moved) << "静止时不应产生位移";
    EXPECT_FLOAT_EQ(player->getBodyAngle(), heading) << "静止时车头不应改变";
}

/**
 * 炮塔与车身解耦 —— 「双摇杆」的另一半。
 *
 * 车体朝下走（bodyAngle 90°）的同时，瞄准点放在正右方：turretAngle 应为 0°。
 * 这两条互不影响，正是「独立炮塔角」的含义。
 */
TEST(PlayerMovement, TurretAimIsIndependentOfBody) {
    Map map;
    auto player = makePlayer(&map);

    player->setAimPoint(player->getPosition() + QPoint(200, 0));   // 正右方
    player->setMoveVector(QPointF(0, 1));                          // 往下走
    player->update();

    EXPECT_NEAR(player->getTurretAngle(), 0.0f, 0.01f) << "炮塔未指向瞄准点";
    EXPECT_NEAR(player->getBodyAngle(), 90.0f, 0.01f) << "车体未跟随移动方向";
}

/** 瞄准点是绝对坐标：坦克移动后，同一瞄准点的角度会随之改变（与旧行为一致）。 */
TEST(PlayerMovement, AimIsAnAbsolutePointNotADirection) {
    Map map;
    auto player = makePlayer(&map);
    const QPoint aim = player->getPosition() + QPoint(100, 100);

    // 先站着不动，把基准角度取准（瞄准点在正右下方 100,100 → 恰好 45°）
    player->setAimPoint(aim);
    player->setMoveVector(QPointF(0, 0));
    player->update();
    EXPECT_NEAR(player->getTurretAngle(), 45.0f, 0.01f);

    // 往右走 40 帧后再看**同一个**瞄准点：它已落在左下方，角度随之变化。
    // 容差 2°：update() 先算炮塔再移动，所以最后一帧的角度对应移动前的位置。
    player->setMoveVector(QPointF(1, 0));
    for (int i = 0; i < 40; ++i)
        player->update();
    EXPECT_NEAR(player->getTurretAngle(), 135.0f, 2.0f) << "瞄准点被当成了方向向量（应当随位置变化）";
}

/**
 * 子步扫掠不得丢掉位移 —— 钉住「末点直接用终点」这个设计决定。
 *
 * 速度 70、盒 40 → 子步上限 40/2 = 20 → 4 个子步，每步 17.5px。
 * 若实现改成「逐个子步累加截断」，每步只走 17px、四步共 68px（少 2px），
 * 位移就会随帧率/速度悄悄缩水。
 */
TEST(PlayerMovement, SubSteppedMoveDoesNotLoseDistance) {
    Map map;
    auto player = makePlayer(&map, 70.0);   // 70 > 盒 40 的一半 → 必然多子步
    const QPoint start = player->getPosition();

    player->setMoveVector(QPointF(1, 0));
    player->update();

    EXPECT_EQ(player->getPosition() - start, QPoint(70, 0)) << "子步累加截断丢掉了位移";
}

/**
 * 探测盒尺寸来自配置，不再是写死的 40（M3 决议 D4）。
 *
 * 在此之前：`Tank::move()` 的两处探测都写死 40×40，而 entities.json 的 `collisionBox`
 * 虽然被加载器解析进了 `TankDef`，却没有继续走到 `TankStats` —— 即**改 JSON 不生效**。
 */
TEST(PlayerMovement, CollisionBoxComesFromStats) {
    Map map;
    tankcity::config::TankStats stats = makeStats();
    stats.collisionBoxW = 24;   // 故意取不等值，确保两个分量各自接线
    stats.collisionBoxH = 36;
    Player player(&map, stats);

    EXPECT_EQ(player.getCollisionBoxWidth(), 24);
    EXPECT_EQ(player.getCollisionBoxHeight(), 36);
}

/**
 * 打完一发之后，冷却走完必须能再打 —— 钉住"只能打一次"这类回归。
 *
 * 背景：M3 步 4b 把 `shootCooldown--` 从 `Tank::move()` 里移出来，改由
 * `Player::updateMovement()` 每帧推进一次。移动函数不再顺手减计时器是对的，
 * 但一旦哪里漏了这一步，症状就是"第一次能开火，之后永远开不了火"。
 * 这条用例逐帧检查，任何一类"冷却不推进"都会在这里露出来。
 */
TEST(PlayerShooting, CooldownTicksSoThePlayerCanFireAgain) {
    Map map;
    auto player = makePlayer(&map, 5.0);   // 配置里玩家 shootDelayTicks = 10

    ASSERT_TRUE(player->canShoot()) << "开局就不能开火";

    Bullet *first = player->shoot();
    ASSERT_NE(first, nullptr) << "第一发打不出来";
    delete first;   // Player::shoot() 把所有权交给调用方（服务端放进 bullets 里）

    EXPECT_FALSE(player->canShoot()) << "刚打完就能再打，冷却没生效";

    // 冷却 10 帧：第 10 帧之后必须恢复
    for (int frame = 0; frame < 10; ++frame)
        player->update();

    EXPECT_TRUE(player->canShoot())
        << "冷却没有随帧推进 —— 表现为\"攻击一次后无法再次攻击\"";

    Bullet *second = player->shoot();
    EXPECT_NE(second, nullptr) << "第二发打不出来";
    delete second;
}

/**
 * 位移被倍率压到 1px 以下时，必须**随时间累加**，不能逐帧丢掉。
 *
 * 实测病灶：森林（×0.5）上的敌人斜向位移是 `2.5 × 0.707 ≈ 1.77px`，向零截断后更慢时
 * 直接成 0 → 整帧位移消失，表现为"站在森林上不动"。这里用"低速 + 斜向 + 空场"
 * 复现同一算术，不依赖关卡数据。
 */
TEST(PlayerMovement, SubPixelStepsStillAccumulate) {
    Map map;
    auto player = makePlayer(&map, 1.25);   // 斜向每轴 0.88px → 单帧截断为零
    const QPoint start = player->getPosition();

    for (int i = 0; i < 100; ++i) {
        player->setMoveVector(QPointF(1, 1));
        player->update();
    }

    const QPoint delta = player->getPosition() - start;
    const double distance =
        std::hypot(static_cast<double>(delta.x()), static_cast<double>(delta.y()));

    EXPECT_GT(distance, 1.0) << "位移被截成 0，整帧丢失 —— 这就是森林上站桩的成因";
    EXPECT_NEAR(distance, 125.0, 3.0) << "100 帧 × 1.25px 应走满 125px";
}

/**
 * 受击盒与移动探针同尺寸（D4 的「统一」）。
 *
 * 在此之前 `getRect()` 返回 30×30、移动探针却是 40×40 —— 子弹擦着车身飞过去不命中。
 * 顺便钉住「以中心对齐」：盒左上角 = 位置 - 半宽/半高。
 */
TEST(PlayerMovement, HitBoxMatchesTheConfiguredBox) {
    Map map;
    tankcity::config::TankStats stats = makeStats();
    stats.collisionBoxW = 24;
    stats.collisionBoxH = 36;
    Player player(&map, stats);
    player.init(100, 200);

    EXPECT_EQ(player.getRect(), QRect(100 - 12, 200 - 18, 24, 36));
}
