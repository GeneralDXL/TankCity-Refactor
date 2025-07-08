#include <QCoreApplication>
#include <QHostAddress>
#include "server.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    
    if (argc != 3) {
        qCritical() << "Usage: server <IP> <port>";
        return 1;
    }
    
    QString ipStr = argv[1];
    QHostAddress addr(ipStr);
    if (addr.isNull()) {
        qCritical() << "Invalid IP address:" << ipStr;
        return 1;
    }

    bool ok;
    quint16 port = QString(argv[2]).toUShort(&ok);
    if (!ok) {
        qCritical() << "Invalid port number:" << argv[2];
        return 1;
    }
    
    GameServer server;
    if (!server.listen(addr, port)) {
        qCritical() << "Could not start server:" << server.errorString();
        return 1;
    }
    qInfo() << "Server started on" << ipStr << ":" << port;

    QObject::connect(&server, &GameServer::closed, &a, &QCoreApplication::quit);
    
    return a.exec();
}