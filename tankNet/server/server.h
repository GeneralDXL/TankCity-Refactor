#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QSet>
#include <QMap>
#include <QThread>
#include "game.h"

class GameServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit GameServer(QObject *parent = nullptr);
    ~GameServer();
    
    bool startServer(quint16 port);
    void stopServer();

signals:
    void closed();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    // void onNewConnection();
    void onClientDisconnected();
    void onReadyRead();
    void handleGameBroadcast(const QByteArray &data);

private:
    void sendToClient(QTcpSocket *client, const QByteArray &data);
    void processClientData(QTcpSocket *client, const QByteArray &data);

    QMap<QTcpSocket*, int> clients; // socket -> clientId
    QMap<int, QTcpSocket*> clientIds; // clientId -> socket
    int nextClientId = 1;

    Game *game;
    QThread *gameThread;
};

#endif // SERVER_H