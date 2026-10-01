/**
 * @file  client_net_test.cpp
 * @brief client/net 分帧器的回归测试。
 *
 * 为什么值得测：分帧是**最容易在边界上出错**的一环 —— TCP 会把一条消息拆成几次到达，
 * 也会把几条消息挤在一次到达里。搬迁前的实现还有一个真实缺陷：等不到正文时会
 * `break`，而长度头已经被吃掉，流就此错位。用例把这些情形逐个钉住。
 */

#include <gtest/gtest.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include "net/MessageFramer.h"
#include "net/MessageParser.h"

namespace {

/// 按协议把一条 JSON 消息打成帧：4 字节大端长度 + 正文。
QByteArray frame(const QJsonObject &object)
{
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QByteArray header(sizeof(quint32), '\0');
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                          reinterpret_cast<uchar *>(header.data()));
    return header + payload;
}

} // namespace

TEST(ClientNetMessageFramer, EmptyBufferNeedsMoreData)
{
    client::net::MessageFramer framer;
    QJsonObject out;

    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::NeedMoreData);
}

TEST(ClientNetMessageFramer, PartialHeaderNeedsMoreData)
{
    client::net::MessageFramer framer;
    QJsonObject out;
    const QByteArray message = frame(QJsonObject{{"type", "game_start"}});

    framer.append(message.left(2));   // 长度头只到了一半
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::NeedMoreData);

    framer.append(message.mid(2));
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("game_start"));
}

TEST(ClientNetMessageFramer, PartialBodyNeedsMoreDataAndHeaderIsKept)
{
    client::net::MessageFramer framer;
    QJsonObject out;
    const QByteArray message = frame(QJsonObject{{"type", "map_init"}});

    // 长度头齐了、正文只有一半：必须返回 NeedMoreData，**且不消费任何字节**
    // （搬迁前的实现会在这里吃掉长度头，导致之后整体错位）。
    framer.append(message.left(message.size() - 3));
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::NeedMoreData);
    EXPECT_EQ(framer.bufferedBytes(), message.size() - 3) << "不完整的一条不应被消费";

    framer.append(message.right(3));
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("map_init"));
    EXPECT_EQ(framer.bufferedBytes(), 0);
}

TEST(ClientNetMessageFramer, TwoMessagesInOneReadAreBothReturnedInOrder)
{
    client::net::MessageFramer framer;
    QJsonObject out;

    framer.append(frame(QJsonObject{{"type", "first"}, {"n", 1}})
                  + frame(QJsonObject{{"type", "second"}, {"n", 2}}));

    ASSERT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("first"));

    ASSERT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("second"));

    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::NeedMoreData);
}

TEST(ClientNetMessageFramer, MalformedJsonIsConsumedAndDoesNotPoisonTheNextMessage)
{
    client::net::MessageFramer framer;
    QJsonObject out;

    // 一条正文不是 JSON 的消息，后面跟一条正常消息
    const QByteArray bad = frame(QJsonObject{{"type", "x"}}).left(0)
                           + [] {
                                 const QByteArray payload = "{ this is not json";
                                 QByteArray header(sizeof(quint32), '\0');
                                 qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                                                       reinterpret_cast<uchar *>(header.data()));
                                 return header + payload;
                             }();

    framer.append(bad + frame(QJsonObject{{"type", "good"}}));

    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::Malformed);
    ASSERT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok)
        << "坏消息被消费掉之后，下一条应当照常解析";
    EXPECT_EQ(out["type"].toString(), QStringLiteral("good"));
}

TEST(ClientNetMessageFramer, JsonArrayIsNotAValidMessage)
{
    client::net::MessageFramer framer;
    QJsonObject out;

    const QByteArray payload = "[1,2,3]";
    QByteArray header(sizeof(quint32), '\0');
    qToBigEndian<quint32>(static_cast<quint32>(payload.size()),
                          reinterpret_cast<uchar *>(header.data()));
    framer.append(header + payload);

    // 旧实现在这里记一句 "Invalid JSON structure" 然后跳过这一条。
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::Malformed);
}

TEST(ClientNetMessageFramer, AbsurdLengthHeaderResyncsInsteadOfWaitingForever)
{
    client::net::MessageFramer framer;
    QJsonObject out;

    const QByteArray message = frame(QJsonObject{{"type", "game_state"}});
    // 前面塞 1 个垃圾字节，长度头就被错位读了
    framer.append(QByteArray(1, 'Z') + message);

    // 会先报若干次 Malformed（每次丢弃 1 字节重新对齐），最终把真消息取出来。
    int malformed = 0;
    for (int i = 0; i < 8; ++i) {
        const auto status = framer.take(out);
        if (status == client::net::MessageFramer::Status::Ok)
            break;
        ASSERT_EQ(status, client::net::MessageFramer::Status::Malformed);
        ++malformed;
    }

    EXPECT_GT(malformed, 0);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("game_state"))
        << "错位之后应当能重新对齐，而不是永远等一个不存在的长度";
}

TEST(ClientNetMessageFramer, ClearDropsAHalfReceivedMessage)
{
    client::net::MessageFramer framer;
    QJsonObject out;
    const QByteArray message = frame(QJsonObject{{"type", "player_init"}});

    framer.append(message.left(message.size() - 1));
    framer.clear();   // 断线：半条消息不能漏进下一次连接

    EXPECT_EQ(framer.bufferedBytes(), 0);
    framer.append(message);   // 重连后从头来
    EXPECT_EQ(framer.take(out), client::net::MessageFramer::Status::Ok);
    EXPECT_EQ(out["type"].toString(), QStringLiteral("player_init"));
}

// ---------------------------------------------------------------------------
// 字段解析：JSON 形状照抄服务器真实下发的内容
// ---------------------------------------------------------------------------

TEST(ClientNetMessageParser, PlayerTankFields)
{
    const QJsonObject json{
        {"type", "player_init"},
        {"id", 3},
        {"position", QJsonObject{{"x", 120}, {"y", 340}}},
        {"bodyAngle", 90.0},
        {"turretAngle", 45.5},
        {"health", 10},
    };

    const client::net::TankState state = client::net::parseTank(json);

    EXPECT_EQ(state.id, 3);
    EXPECT_EQ(state.position, QPoint(120, 340));
    EXPECT_DOUBLE_EQ(state.bodyAngle, 90.0);
    EXPECT_DOUBLE_EQ(state.turretAngle, 45.5);
    EXPECT_EQ(state.health, 10);
    EXPECT_EQ(state.difficulty, 0) << "玩家消息里没有难度字段，应为默认值";
}

TEST(ClientNetMessageParser, EnemyTankCarriesDifficulty)
{
    const QJsonObject json{
        {"type", "enemy_init"},
        {"id", 7},
        {"difficulty", 2},
        {"position", QJsonObject{{"x", 5}, {"y", 6}}},
        {"bodyAngle", 180.0},
        {"turretAngle", 270.0},
        {"health", 4},
    };

    const client::net::TankState state = client::net::parseTank(json);

    EXPECT_EQ(state.id, 7);
    EXPECT_EQ(state.difficulty, 2);
    EXPECT_EQ(state.position, QPoint(5, 6));
    EXPECT_EQ(state.health, 4);
}

TEST(ClientNetMessageParser, WallFieldsComeFromPositionAndSizePairs)
{
    const QJsonObject json{
        {"position", QJsonObject{{"x", 10}, {"y", 20}}},
        {"size", QJsonObject{{"width", 30}, {"height", 40}}},
        {"type", 1},
        {"id", 42},
    };

    const client::net::WallState state = client::net::parseWall(json);

    EXPECT_EQ(state.rect, QRect(10, 20, 30, 40));
    EXPECT_EQ(state.type, 1);
    EXPECT_EQ(state.id, 42);
}

TEST(ClientNetMessageParser, ItemFieldsUseTheItemTypeField)
{
    const QJsonObject json{
        {"id", 11},
        {"x", 500},
        {"y", 600},
        {"item_type", 3},
    };

    const client::net::ItemState state = client::net::parseItem(json);

    EXPECT_EQ(state.id, 11);
    EXPECT_EQ(state.x, 500);
    EXPECT_EQ(state.y, 600);
    EXPECT_EQ(state.type, 3);
}

/**
 * 把「子弹颜色恒为敌人色」这个既有缺陷的行为钉住。
 *
 * 两种真实报文都试一遍，期望都是 `fromPlayer == false`：
 *  - 每帧的 game_state 发的是 `type: "player"` —— 客户端读的是 `type1`，读不到；
 *  - 敌人开火那条发的是 `type1: 1`（整数）—— 客户端按字符串读，同样读不到。
 *
 * 这条用例的价值不是"正确"，而是**显式记录现状**：等 M5 统一协议时，
 * 这里会随字段名一起改，届时测试失败即提醒「画面要变了」。
 */
TEST(ClientNetMessageParser, FromPlayerIsAlwaysFalseWithTheCurrentServerFields)
{
    const QJsonObject gameStateBullet{
        {"position", QJsonObject{{"x", 1}, {"y", 2}}},
        {"angle", 30.0},
        {"type", "player"},   // 服务器实际发的字段
    };
    EXPECT_FALSE(client::net::parseBullet(gameStateBullet).fromPlayer)
        << "客户端读 type1，而服务器发 type —— 玩家子弹也会被画成敌人色（M5 修）";

    const QJsonObject enemyFireBullet{
        {"position", QJsonObject{{"x", 3}, {"y", 4}}},
        {"angle", 60.0},
        {"type1", 1},          // 敌人开火那条发的是整数
    };
    EXPECT_FALSE(client::net::parseBullet(enemyFireBullet).fromPlayer)
        << "type1 是整数，按字符串读仍为空";
}

TEST(ClientNetMessageParser, BulletPositionAndAngleAreRead)
{
    const QJsonObject json{
        {"position", QJsonObject{{"x", 777}, {"y", 888}}},
        {"angle", 123.5},
    };

    const client::net::BulletState state = client::net::parseBullet(json);

    EXPECT_EQ(state.position, QPoint(777, 888));
    EXPECT_DOUBLE_EQ(state.angle, 123.5);
}

TEST(ClientNetMessageParser, MissingFieldsFallBackToDefaultsInsteadOfCrashing)
{
    const QJsonObject empty;

    const client::net::TankState tank = client::net::parseTank(empty);
    EXPECT_EQ(tank.id, 0);
    EXPECT_EQ(tank.position, QPoint(0, 0));
    EXPECT_EQ(tank.health, 0);

    EXPECT_EQ(client::net::parseWall(empty).rect, QRect(0, 0, 0, 0));
    EXPECT_EQ(client::net::parseBullet(empty).position, QPoint(0, 0));
    EXPECT_EQ(client::net::parseItem(empty).id, 0);
}
