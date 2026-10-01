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
