#include "net/MessageParser.h"

namespace client::net {

namespace {

QPoint parsePoint(const QJsonObject &object)
{
    return QPoint(object["x"].toInt(), object["y"].toInt());
}

} // namespace

TankState parseTank(const QJsonObject &object)
{
    TankState state;
    state.id = object["id"].toInt();
    state.position = parsePoint(object["position"].toObject());
    state.bodyAngle = object["bodyAngle"].toDouble();
    state.turretAngle = object["turretAngle"].toDouble();
    state.health = object["health"].toInt();
    state.difficulty = object["difficulty"].toInt();
    return state;
}

WallState parseWall(const QJsonObject &object)
{
    WallState state;
    const QPoint position = parsePoint(object["position"].toObject());
    const QJsonObject size = object["size"].toObject();
    state.rect = QRect(position.x(), position.y(), size["width"].toInt(), size["height"].toInt());
    state.block = object["block"].toString();   // M3 步 5：物块 id（过渡期与 type 并存）
    state.type = object["type"].toInt();
    state.id = object["id"].toInt();
    return state;
}

BulletState parseBullet(const QJsonObject &object)
{
    BulletState state;
    state.position = parsePoint(object["position"].toObject());
    state.angle = object["angle"].toDouble();

    // 字段名与判断方式都与搬迁前逐字一致：读 `type1`、按字符串比较。
    // 这在当前服务器下**永远得到 false**（原因见头文件），但改它就是改画面，属 M5。
    state.fromPlayer = object["type1"].toString() == QLatin1String("player");
    return state;
}

ItemState parseItem(const QJsonObject &object)
{
    ItemState state;
    state.id = object["id"].toInt();
    state.x = object["x"].toInt();
    state.y = object["y"].toInt();
    state.type = object["item_type"].toInt();
    return state;
}

} // namespace client::net
