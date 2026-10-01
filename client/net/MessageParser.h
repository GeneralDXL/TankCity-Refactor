#ifndef CLIENT_NET_MESSAGEPARSER_H
#define CLIENT_NET_MESSAGEPARSER_H

#include <QJsonObject>
#include <QPoint>
#include <QRect>

namespace client::net {

/**
 * 协议字段 → 客户端镜像数据的映射，**集中在这一处**。
 *
 * 为什么值得单独抽出来：搬迁前这些字段名散在 `GameWindow::onReadyRead` 的十几个分支里，
 * 同一个概念（比如坦克的位置/角度/血量）被手写了三四遍。字段名一旦与服务器对不上，
 * 编译器不会报错、测试也没有，只会表现为「画面某处永远不对」
 * —— 子弹颜色那个 bug 就是这么藏了很久的（见 `parseBullet`）。
 *
 * 另外：这些都是**纯解析**，不含 UI、不含 socket，所以可以直接单元测试。
 * 本轮**不改任何字段名**，只把它们收拢；真正该统一命名的是 M5。
 */

/// 坦克状态（玩家与敌人共用；敌人的消息里多一个难度字段）。
struct TankState
{
    int id = 0;
    QPoint position;
    double bodyAngle = 0.0;
    double turretAngle = 0.0;
    int health = 0;
    /// 仅敌人消息带此字段；`player_init` 与 `game_state` 的玩家项里没有，取默认 0。
    int difficulty = 0;
};

/// 墙（`map_init` 的每一面墙）。
struct WallState
{
    QRect rect;
    int type = 0;
    int id = 0;
};

/// 子弹。
struct BulletState
{
    QPoint position;
    double angle = 0.0;
    /**
     * 是否是玩家打出的子弹。
     *
     * ⚠️ **恒为 false** —— 这是既有的协议不一致，本轮原样保留（属 M5）：
     *  - `game_state` 与 `bullet_created` 里，客户端读的都是 `type1` 字段并当作**字符串**；
     *  - 而 `game_state`（server/game.cpp:798）发的是 **`type`**，从没发过 `type1`；
     *  - 敌人开火那条（game.cpp:506）虽然发了 `type1`，但值是**整数**，`toString()` 仍得空串；
     *  - 玩家开火那条（game.cpp:323）把 `type` 覆盖成了 `"player"`，连分支都进不来。
     * 结果就是所有子弹都按敌人色（红）绘制 —— 基线截图里那颗红色弹道即由此而来。
     */
    bool fromPlayer = false;
};

/// 道具。
struct ItemState
{
    int id = 0;
    int x = 0;
    int y = 0;
    /// 协议里的 `item_type`（整数）；转成客户端枚举由调用方决定。
    int type = 0;
};

TankState parseTank(const QJsonObject &object);
WallState parseWall(const QJsonObject &object);
BulletState parseBullet(const QJsonObject &object);
ItemState parseItem(const QJsonObject &object);

} // namespace client::net

#endif // CLIENT_NET_MESSAGEPARSER_H
