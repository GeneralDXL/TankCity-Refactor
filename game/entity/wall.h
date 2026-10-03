#ifndef WALL_H
#define WALL_H

#include <QRect>
#include "config/ConfigTypes.h"

class Wall : public QRect{
private:
    QRect rect; // 矩形区域
    int x;      // x坐标(左上)
    int y;      // y坐标(左上)
    int width;  // 宽度
    int height; // 高度
    int health; // 生命值（来自配置；不可破坏的物块为 0）
    int id;
    // 以下四项行为一律取自 assets/config/blocks.json，不再按 type 写死判断。
    bool destructible = false;      // 能否被打掉
    bool blocksTank = true;         // 是否挡坦克
    bool blocksBullet = true;       // 是否挡子弹
    // 是否挡视线（M4 步 3，来自 blocks.json 的 blocksSight）。
    // 森林为 true ⇒ 敌怪的视线穿不过森林 ⇒「待在林子里就不被看见」自然成立，无需特例；
    // 海为 false ⇒ 隔海看得见、但绕不过去（手稿里的设计意图）。
    bool blocksSight = false;
    // 打掉它所需的子弹标签（M4 步 1）。空 = 任何子弹都能打（砖块）；非空 = 必须带其中一个标签
    // （钢材要 armorPiercing，普通弹打上去不掉血）。取自 blocks.json 的 requiredBulletTags。
    QStringList requiredBulletTags;
    double moveSpeedFactor = 0.0;   // 地形移动倍率（0 表示不影响移动）
    // 物块 id（blocks.json 的键；边界墙为 "boundary"）。M3 步 5 引入：
    // 渲染与协议都在往「按 id 走」迁移，int type 与 world.cpp 里的桥接表将随之删除。
    // 声明在这里是为了与构造函数初始化列表的顺序一致（否则 -Wreorder）。
    QString block;
public:
    QRect getRect() const { return rect; } // 获取矩形区域

    /**
     * 边界墙：四边等厚、不可破坏、挡坦克也挡子弹。
     *
     * 这是引擎的固定约定，不由 blocks.json 描述 —— 关卡不允许自带边界物块，
     * 所以这里显式写出行为，而不是从配置读（BlockDef 的默认值恰好相反）。
     */
    static Wall makeBoundary(int x, int y, int width, int height);

    /// 关卡物块：物块 id 与行为（可破坏性 / 生命值 / 三项碰撞开关 / 地形倍率）
    /// 全部来自 BlockDef。
    Wall(int x, int y, int width, int height,
         const tankcity::config::BlockDef &def);

    ~Wall() = default; // 默认析构函数

    /// 物块 id（blocks.json 的键；边界墙是 "boundary"）。
    /// M3 步 5 起渲染与协议都按它走 —— 那个跨进程约定的类型号已经没有存在的必要。
    QString getBlockId() const { return block; }
    bool isDestructible() const { return destructible; } // 是否可破坏（配置）
    bool isBlockingTank() const { return blocksTank; }   // 是否挡坦克（配置）
    bool isBlockingBullet() const { return blocksBullet; } // 是否挡子弹（配置）
    bool isBlockingSight() const { return blocksSight; }   // 是否挡视线（配置）

    /**
     * 这发子弹打不打得到它（M4 步 1）。
     *
     * 判据是两个集合**有无交集**：物块要求 `requiredBulletTags` 中至少一个标签，
     * 子弹提供 `bulletTags`。物块不要求任何标签时，任何子弹都打得动。
     *
     * 于是「钢材要穿甲弹、砖块不挑弹种」纯粹是配置差别 —— 没有 `if (steel)` 这种特判。
     */
    bool acceptsBullet(const QStringList &bulletTags) const
    {
        if (requiredBulletTags.isEmpty())
            return true;
        for (const QString &tag : requiredBulletTags) {
            if (bulletTags.contains(tag))
                return true;
        }
        return false;
    }
    double getMoveSpeedFactor() const { return moveSpeedFactor; } // 地形倍率（配置）
    int getHealth() const { return health; } // 获取生命值
    void setHealth(int h) { health = h; } // 设置生命值
    bool isMovable() const { return !blocksTank; }
    int getId() const { return id; } // 获取墙的唯一ID  

    // 注：这里原本还有一个 takeDamage()，里面写着 `delete this`。
    // Wall 是 QVector<Wall> 里的**值元素**，一旦有人调用就是未定义行为；
    // 而它从未被调用过（唯一的伤害路径是 Map::checkBulletCollision 里的 setHealth），
    // 故于 M1.1 删除。
};

#endif // WALL_H
