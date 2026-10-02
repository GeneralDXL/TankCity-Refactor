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
      shootDelay(stats.shootDelayTicks),
      collisionBoxW(stats.collisionBoxW), collisionBoxH(stats.collisionBoxH),
      gameMap(gameMap)
{
    shootCooldown = 0;
}

void Tank::move(float angle, float distance, Map *map)
{
    // 注：射击冷却**不在这里**推进。移动函数顺手给计时器减一是个坑 ——
    // 敌人原来绕开 move() 自己改 position，一旦改成走共享移动判定（M3 步 4b），
    // 冷却就会一帧被减两次、射速翻倍。现在冷却统一由各自的 update() 推进。
    //
    // 地形对移动的影响来自 blocks.json 的 moveSpeedFactor（森林 0.5、冰 1.5），
    // 无影响时为 1.0。与旧代码「只认 FOREST/ICE 两个类型」等价 —— 取倍率而不是
    // 判断类型，新增地形不必再改这里。
    const float scale = static_cast<float>(map->getMoveSpeedFactor(position));
    const double rad = qDegreesToRadians(static_cast<double>(angle));
    const double stepX = std::cos(rad) * distance * scale;
    const double stepY = std::sin(rad) * distance * scale;

    // 亚像素余量：位移常常不足 1px（森林 ×0.5 上的敌人斜向只有 2.5*0.707 ≈ 1.77px，
    // 更慢或倍率更小时直接为 0），逐帧向零截断会把整帧丢掉 —— 实测表现就是
    // "站在森林上不动"。把上帧余量加进来、把本帧没走完的留到下一帧，
    // 平均位移才等于速度 × 帧数（且不会像四舍五入那样凭空多走，没有加速漏洞）。
    const QPointF wanted = QPointF(stepX, stepY) + moveRemainder_;
    const QPoint intended(static_cast<int>(wanted.x()), static_cast<int>(wanted.y()));

    // 本帧终点：向零截断，与旧实现同一口径（余量为 0 时逐位一致）。
    const QPoint target = position + intended;

    // 扫掠（M3 决议 D3）：把这一帧摊到若干子步上逐点判定。
    //
    // 旧实现只判定**终点**：位移一旦长到能跨过一整面墙，终点探针就落在墙后，
    // 于是穿过去。子步长度上限取碰撞盒最短边的一半 —— 中心每步移动不到半个盒宽，
    // 探针必然与途中任何一面墙重叠，因此不可能跳过它（与墙多厚无关）。
    //
    // 当前配置（盒 40、速度 5）算出来正好是 1 个子步，也就是说与旧实现走的是同一条路：
    // 这是给「将来把盒子配小、速度配大」留的保险，而不是在修当下已发生的穿模。
    const double maxSubStep = std::max(1.0, std::min(collisionBoxW, collisionBoxH) / 2.0);
    const double length = std::hypot(static_cast<double>(intended.x()),
                                     static_cast<double>(intended.y()));
    const int subSteps = std::min(kMaxSubSteps,
                                  std::max(1, static_cast<int>(std::ceil(length / maxSubStep))));

    QPoint reached = position;
    for (int i = 1; i <= subSteps; ++i) {
        // 末点直接用 target，中间点按比例分摊。**不能逐个子步累加截断**：
        // 每步都会丢掉小数（速度 70、4 子步时每步 17.5 → 截断成 17，4 步只有 68）。
        // 末点写死成 target，就保证了「走了多远」与旧实现完全一致，变的只是判定密度。
        const QPoint probe = (i == subSteps)
                                 ? target
                                 : position + QPoint(intended.x() * i / subSteps,
                                                     intended.y() * i / subSteps);
        if (!isPassable(probeRect(probe), map))
            break;
        reached = probe;
    }

    if (reached == target) {
        // 完整走完：把不足 1px 的部分留到下一帧
        moveRemainder_ = wanted - QPointF(intended);
    } else {
        // 被挡下／只走到中途：余量作废 —— 否则顶着墙一直攒位移，
        // 一转身就会把那笔"欠账"一次性释放（瞬移）。
        moveRemainder_ = QPointF();
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

bool Tank::canStep(float angle, float distance, Map *map) const
{
    // 与 move() 用同一套算术（含地形倍率与亚像素余量），否则会出现
    // "查询说能走、实走却原地不动"，转向逻辑就会被骗进死角。
    const float scale = static_cast<float>(map->getMoveSpeedFactor(position));
    const double rad = qDegreesToRadians(static_cast<double>(angle));
    const QPointF wanted = QPointF(std::cos(rad) * distance * scale,
                                   std::sin(rad) * distance * scale) + moveRemainder_;
    const QPoint intended(static_cast<int>(wanted.x()), static_cast<int>(wanted.y()));

    // 本帧根本不足 1px：**如实回报走不了**。
    // 这里以前返回 true（当时的想法是"别把自己挡下"），结果转向逻辑会挑中一个
    // "看着能走、实际原地不动"的方向 —— 那正是森林上站桩的直接原因。
    if (intended.isNull())
        return false;

    return isPassable(probeRect(position + intended), map);
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
