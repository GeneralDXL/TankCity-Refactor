#include "net/MessageFramer.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QtEndian>

namespace client::net {

namespace {
constexpr int kLengthHeaderBytes = sizeof(quint32);
} // namespace

void MessageFramer::append(const QByteArray &bytes)
{
    buffer_.append(bytes);
}

void MessageFramer::clear()
{
    buffer_.clear();
}

MessageFramer::Status MessageFramer::take(QJsonObject &out)
{
    if (buffer_.size() < kLengthHeaderBytes)
        return Status::NeedMoreData;

    const quint32 messageLength = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar *>(buffer_.constData()));

    if (messageLength > kMaxMessageBytes) {
        // 长度头荒唐 → 大概是流已经错位。丢掉一个字节重新找对齐点，
        // 而不是无限期地等这个（不存在的）长度。
        buffer_.remove(0, 1);
        return Status::Malformed;
    }

    const quint64 frameBytes = static_cast<quint64>(kLengthHeaderBytes) + messageLength;
    if (static_cast<quint64>(buffer_.size()) < frameBytes)
        return Status::NeedMoreData;   // 头与正文都先留着，等下一次数据

    const QByteArray payload = buffer_.mid(kLengthHeaderBytes, static_cast<int>(messageLength));
    buffer_.remove(0, static_cast<int>(frameBytes));

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return Status::Malformed;   // 与旧的「记一条警告后跳过这一条」等价
    }
    if (!doc.isObject()) {
        return Status::Malformed;
    }

    out = doc.object();
    return Status::Ok;
}

} // namespace client::net
