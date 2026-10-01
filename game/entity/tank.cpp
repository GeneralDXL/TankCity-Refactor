#include "tank.h"
#include <QtMath>
#include <QDebug>
#include <cmath>

namespace {

/**
 * 一帧里子步探测点的上限。
 *
 * 纯属防御：子步长度按碰撞盒推导，正常配置（盒 40、速度 5）只会算出 1 个子步，
 * 即便把速度配成 300 也只有 15 个。加这个上限是防止将来出现「盒 2px + 速度 1000」
 * 之类的极端配置时在一帧里形成无界循环。
 */
constexpr int kMaxSubSteps = 64;

} // namespace

Tank::Tank(Map *gameMap, const tankcity::config::TankStats &stats)
    : health(stats.health), speed(stats.moveSpeed), bulletSpeed(stats.bulletSpeed),
      bulletDamage(stats.bulletDamage), muzzleOffset(stats.muzzleOffset),
      collisionBoxW(stats.collisionBoxW), collisionBoxH(stats.collisionBoxH),
      gameMap(gameMap)
{
    shootCooldown = 0;
}

void Tank::move(float angle, float distance, Map *map)
{
    if (shootCooldown > 0) shootCooldown--;

    // 地形对移动的影响来自 blocks.json 的 moveSpeedFactor（森林 0.5、冰 1.5），
    // 无影响时为 1.0。与旧代码「只认 FOREST/ICE 两个类型」等价 —— 取倍率而不是
    // 判断类型，新增地形不必再改这里。
    const float scale = static_cast<float>(map->getMoveSpeedFactor(position));
    const double rad = qDegreesToRadians(static_cast<double>(angle));
    const double stepX = std::cos(rad) * distance * scale;
    const double stepY = std::sin(rad) * distance * scale;

    // 本帧终点：与旧实现逐位一致（`position + QPoint(stepX, stepY)`，向零截断）。
    // 扫掠**不改动这条算术** —— 它只改变「怎么判定能不能过去」。
    const QPoint target = position + QPoint(stepX, stepY);

    // 扫掠（M3 决议 D3）：把这一帧摊到若干子步上逐点判定。
    //
    // 旧实现只判定**终点**：位移一旦长到能跨过一整面墙，终点探针就落在墙后，
    // 于是穿过去。子步长度上限取碰撞盒最短边的一半 —— 中心每步移动不到半个盒宽，
    // 探针必然与途中任何一面墙重叠，因此不可能跳过它（与墙多厚无关）。
    //
    // 当前配置（盒 40、速度 5）算出来正好是 1 个子步，也就是说与旧实现走的是同一条路：
    // 这是给「将来把盒子配小、速度配大」留的保险，而不是在修当下已发生的穿模。
    const double maxSubStep = std::max(1.0, std::min(collisionBoxW, collisionBoxH) / 2.0);
    const double length = std::hypot(stepX, stepY);
    const int subSteps = std::min(kMaxSubSteps,
                                  std::max(1, static_cast<int>(std::ceil(length / maxSubStep))));

    QPoint reached = position;
    for (int i = 1; i <= subSteps; ++i) {
        // 末点直接用 target，中间点按比例分摊。**不能逐个子步累加截断**：
        // 每步都会丢掉小数（速度 70、4 子步时每步 17.5 → 截断成 17，4 步只有 68）。
        // 末点写死成 target，就保证了「走了多远」与旧实现完全一致，变的只是判定密度。
        const QPoint probe = (i == subSteps)
                                 ? target
                                 : position + QPoint(stepX * i / subSteps, stepY * i / subSteps);
        if (!isPassable(probeRect(probe), map))
            break;
        reached = probe;
    }

    // 被拦下时停在最后一个可通行子步；在当前配置下 subSteps 恒为 1，
    // 这意味着「撞墙就整帧不动」，与旧实现相同。旧写法在这里每帧打一条 qDebug，
    // 顶着墙走会把日志刷满，故不再保留那条输出。
    position = reached;
}

QRect Tank::probeRect(const QPoint &center) const
{
    return QRect(center.x() - collisionBoxW / 2, center.y() - collisionBoxH / 2,
                 collisionBoxW, collisionBoxH);
}

QRect Tank::getRect() const
{
    return probeRect(position);
}

bool Tank::isPassable(const QRect &probe, Map *map) const
{
    const int type = map->checkTankCollision(probe, this);
    return type == FOREST || type == ICE || type == -1;
}

void Tank::takeDamage(int amount)
{
    health -= amount;
    if (health < 0) health = 0;
}

bool Tank::isDestroyed() const
{
    return health <= 0;
}

int Tank::getHealth() const
{
    return health;
}

QPoint Tank::getPosition() const
{
    return position;
}
