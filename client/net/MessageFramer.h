#ifndef CLIENT_NET_MESSAGEFRAMER_H
#define CLIENT_NET_MESSAGEFRAMER_H

#include <QByteArray>
#include <QJsonObject>

namespace client::net {

/**
 * 长度前缀分帧器：把 TCP 字节流切成一条条完整的 JSON 消息。
 *
 * 协议：`[4 字节大端长度][该长度的 UTF-8 JSON 文本]`。
 *
 * ## 为什么改成「喂字节」而不是直接读 socket
 * 搬迁前的取消息方式是在 `onReadyRead` 里先读 4 字节头，若消息体还没到齐就调用
 * `socket->waitForReadyRead(100)` **阻塞等待** —— 那是在 GUI 线程的槽函数里阻塞，
 * 网络稍慢就会卡住窗口。而且一旦等不到就 `break` 跳出循环，
 * 此时**长度头已经被吃掉**，流就此错位（下一条消息的正文会被当成长度头）。
 *
 * 分帧器把这两件事都修掉：还没凑齐就原样留着等下一次数据，长度头与正文**要么一起消费、要么一起不消费**。
 * 对合法数据流而言消息与顺序完全不变（只是不再阻塞 UI 线程）。
 *
 * 它不碰 socket、不认识任何消息类型，所以能脱离网络做单元测试。
 */
class MessageFramer
{
public:
    enum class Status
    {
        Ok,            ///< 取到一条完整消息，`out` 已填充
        NeedMoreData,  ///< 还没凑齐一条，等更多字节
        Malformed,     ///< 这一条不合协议（已丢弃），可以继续取
    };

    /// 单条消息的长度上限：用来把「把非本协议的数据当成流」这种情形尽早识别出来，
    /// 而不是傻等一个永远不会到来的长度。（游戏状态报文实测只有几 KB。）
    static constexpr quint32 kMaxMessageBytes = 4u * 1024u * 1024u;

    /// 喂入新到达的字节。
    void append(const QByteArray &bytes);

    /// 尝试取出下一条消息。
    Status take(QJsonObject &out);

    /// 丢弃缓冲（断线时必须调用：半条消息不能漏进下一次连接）。
    void clear();

    int bufferedBytes() const { return buffer_.size(); }

private:
    QByteArray buffer_;
};

} // namespace client::net

#endif // CLIENT_NET_MESSAGEFRAMER_H
