#include <QCoreApplication>
#include <QHostAddress>
#include "server.h"

namespace {

/// 不带参数时的默认监听地址（M1.1）：与客户端写死的 127.0.0.1:12345 对齐，
/// 本机自测不必再查参数。
constexpr quint16 kDefaultPort = 12345;
const char *const kDefaultAddress = "0.0.0.0";

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // `[<IP> <port>]` 可整体缺省：给了就两个都给，没给就走默认。
    // 只给一个参数无法判断那是 IP 还是端口，故直接拒绝，免得把端口当 IP 用。
    QString ipStr = kDefaultAddress;
    quint16 port = kDefaultPort;

    if (argc == 3) {
        ipStr = argv[1];
        bool ok = false;
        port = QString(argv[2]).toUShort(&ok);
        if (!ok) {
            qCritical() << "Invalid port number:" << argv[2];
            return 1;
        }
    } else if (argc != 1) {
        qCritical() << "Usage: tankcity_server [<IP> <port>]";
        qCritical() << "Without arguments it listens on" << kDefaultAddress << ":" << kDefaultPort;
        return 1;
    }

    QHostAddress addr(ipStr);
    if (addr.isNull()) {
        qCritical() << "Invalid IP address:" << ipStr;
        return 1;
    }

    GameServer server;
    if (!server.listen(addr, port)) {
        qCritical() << "Could not start server:" << server.errorString();
        return 1;
    }
    qInfo() << "Server started on" << ipStr << ":" << port;

    // 停机清理由 ~GameServer() 兜底；服务端目前靠外部终止（关控制台窗口 / Ctrl+C）。
    //
    // 这里原本还有一行 `connect(&server, &GameServer::stopServer, &a, &QCoreApplication::quit)`，
    // 但 stopServer() 是 slot 而不是 signal —— Qt 只打一条 "signal not found" 警告就静默失效，
    // 而此前服务端是隐藏窗口的 GUI 程序，这条警告一直被埋在调试输出里。
    // 改成控制台程序（M1.1）后它才现形，故删除；优雅停机留到 M5 网络统一时一并做。
    return a.exec();
}
