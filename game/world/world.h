#ifndef WORLD_H
#define WORLD_H

#include <QPoint>
#include <QRect>
#include <QVector>

#include "config/ConfigLoader.h"
#include "config/ConfigTypes.h"
#include "physics/CollisionWorld.h"
#include "physics/Grid.h"
#include "wall.h"

class Tank;

/**
 * 世界：**几何**的归属地 —— 网格、物块、碰撞查询。
 *
 * M3 从 `Map` 拆出来的那一半。拆分判据只有一句：**这行代码会不会向外发消息**。
 *  - `World` 只回答几何问题（这一格能不能过？这个矩形撞到什么？），
 *    它**连 `QObject` 都不是** —— 于是"不该发消息"在类型上就成立，
 *    而不是靠注释约束；
 *  - 扣血、销毁、广播这些**游戏规则**留在 `Map`（见 map.h）：`Map` 把几何问题
 *    委托给这里，规则问题留给自己。
 *
 * 本步是**门面式拆分**：`Map` 退化为转发层，实体（`Tank`/`Enemy`/`Player`）与
 * `Game` 的签名里写的仍是 `Map*`，**一行未动**。等 tile 世界与 A\* 真正落到
 * `World`（M3 后续步骤）时，再把实体签名迁过来 —— 那时它们本来就要重写。
 */
class World
{
public:
    World();

    /**
     * 按关卡数据装载墙体（边界 + 物块）。
     *
     * 关卡几何来自 assets/levels/*.json；物块的碰撞/贴图仍由 `wall.h` 的 int type
     * 表达，所以这里做一次 block id → type 的映射（见 world.cpp）。物块行为
     * （可破坏性、三项碰撞开关、地形倍率）直接取自 config 的 BlockDef ——
     * 关卡只描述「哪个物块摆在哪儿」。
     *
     * @return false 表示关卡里出现了 `Wall` 无法表达的物块 id，或引用了
     *         blocks.json 里不存在的物块（数据与引擎对不上）。此时世界为空，
     *         调用方应当放弃开局，而不是让玩家掉进空地图。
     */
    bool loadLevel(const tankcity::config::LevelData &level,
                   const tankcity::config::Config &config);

    static const int MAP_WIDTH = 1200;
    static const int MAP_HEIGHT = 900;

    /**
     * 单元格边长（像素）。
     *
     * M3 由 **40 改为 50**：`900 / 40 = 22.5`，纵向网格非整数 —— 最后 20px 落在网格之外，
     * A\* 甚至把这个非整数写进了死代码（`neighborPos.y() >= 22.5`）。
     * 而 `1200 / 50 = 24`、`900 / 50 = 18`，都是整数。
     *
     * 为什么改格尺寸而不是改世界高度（关卡数据里 `world.height` 仍是 900）：
     * 关卡里有 `y + h = 890` 的墙正压在底边界上，世界一变矮就必须动**几何**，
     * 而那会破坏 `tests/data/legacy_levels.json` 的冻结快照 —— 那是 M2 用来证明
     * 「关卡几何搬出 C++ 未失真」的物证。改格尺寸则关卡数据**一字不动**。
     *
     * 顺带解决一处不匹配：坦克的碰撞盒是 40×40，格子现在正好 50，见 kCellPadding。
     */
    static const int GRID_SIZE = 50;

    /// 横向 / 纵向格子数（1200/50 = 24、900/50 = 18，均为整数）
    int getGridWidth() const { return MAP_WIDTH / GRID_SIZE; }
    int getGridHeight() const { return MAP_HEIGHT / GRID_SIZE; }

    QPoint worldToGrid(const QPoint &worldPos) const;
    bool isCellWalkable(int gridX, int gridY) const;

    /**
     * 从 @p preferred 出发，取一个**坦克放得下**的格子中心（世界坐标）。
     *
     * 判据就是 `isCellWalkable`：D1 的口径是「格内内缩后仍容得下坦克才算可通行」，
     * 所以「可通行格的中心」恰好等价于「整辆车放得下的位置」。
     * 距离按切比雪夫（棋盘距离）取最近，平手按行优先 —— 保证确定性。
     * 整张图没有可通行格时原样返回 @p preferred（不猜、不抛异常）。
     *
     * 用途是**出生点**：在此之前出生点是写死的坐标、与关卡内容无关 ——
     * 只要那张地图在那个位置放了挡车的物块（`level_13` 的海正好压在 80,400 那一格），
     * 玩家一开局就卡在里面动弹不得。
     */
    QPoint nearestWalkableCenter(const QPoint &preferred) const;
    bool isLineWalkable(const QPoint &start, const QPoint &end) const;
    QPoint gridToWorld(const QPoint &gridPos) const;

    /**
     * 该位置的地形移动倍率，1.0 表示不受影响。
     *
     * 取自 blocks.json 的 `moveSpeedFactor`（森林 0.5、冰 1.5；0 表示该物块不影响
     * 移动）。取「首个包含该点的墙体」，与旧实现同序。
     */
    double getMoveSpeedFactor(const QPoint &position) const;

    /// 是否有物块挡住这个矩形（只判「挡移动」标记，见 kBlocksMove）。
    bool checkCollision(const QRect &rect) const;

    /// 这个矩形所在位置有没有挡坦克的物块（只判 `blocksTank` 标记，见 kBlocksMove）。
    ///
    /// M3 步 5：旧签名返回"撞到的那个墙的**类型号**"，调用方却只用它判"有没有碰撞"
    /// （类型名白名单那两条其实是死逻辑）。类型号消失后这个返回值没有意义，故改成 bool。
    /// 同时去掉了那个 `const Tank *` 形参 —— D4 提过的"按体型区分"始终没做，
    /// 没必要让每个调用方都传一个用不上的指针。
    bool blocksTankAt(const QRect &rect) const;

    /**
     * 首个挡子弹的物块 id（没有则 -1）。
     *
     * 只做**查询**：找到是谁。要不要扣血、是否销毁、要不要广播，是 `Map` 的规则。
     */
    int firstShotBlockerId(const QRect &rect) const;

    const QVector<Wall> &getWalls() const { return walls; }

    // --- 供规则层（Map）改动世界的小接口 ---
    const Wall *wallById(int wallId) const;
    Wall *mutableWallById(int wallId);
    /// 按 id 移除物块并同步碰撞世界；不存在返回 false。
    bool removeWallById(int wallId);

private:
    /**
     * 碰撞标记：**含义由游戏层定义**，引擎只负责按位匹配
     * （`engine/physics/CollisionWorld` 不知道「坦克」「子弹」是什么）。
     */
    enum CollisionFlags {
        kBlocksMove = 1u << 0,   ///< 是否挡移动（blocks.json 的 blocksTank）
        kBlocksShot = 1u << 1,   ///< 是否挡投射物（blocks.json 的 blocksBullet）
    };

    /**
     * 「某一格能否通过」判定时的内缩像素。
     *
     * 取 5 使探测盒成为 `50 - 2*5 = 40` 见方 —— **正好等于坦克的碰撞盒**：
     * 格子内缩之后仍容得下坦克，才算这一格可通行。格尺寸是 40 时这个关系并不成立
     * （40 内缩 5 得 30×30，比坦克还小），只能靠"贴着墙走"凑合；改成 50 之后，
     * 「这一格能否通过」与「坦克能不能站进去」才真正是同一件事。
     */
    static const int kCellPadding = 5;

    /// 由物块配置转成碰撞标记。
    static engine::physics::CollisionWorld::Flags flagsOf(const Wall &wall);

    /**
     * 重建碰撞世界。
     *
     * 碰撞世界只保存**几何与标记的副本**（这样查询不必每次遍历 Wall 对象、也不必
     * 关心血量等状态），所以装载与移除物块后都要同步一次。物块数量是关卡级别
     * （几十个），直接整体重建比维护索引更不容易出错。
     */
    void rebuildCollisionWorld();

    QVector<Wall> walls;     //边界墙

    engine::physics::Grid grid_{GRID_SIZE};          ///< 格点换算（A* 与「这一格能否通过」）
    engine::physics::CollisionWorld world_;          ///< 静态物块的碰撞查询
};

#endif
