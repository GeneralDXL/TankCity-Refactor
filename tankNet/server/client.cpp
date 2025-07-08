#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;
#pragma comment(lib, "ws2_32.lib")

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <server_port>" << endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        cerr << "WSAStartup failed" << endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connect failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server." << endl;

    char sendBuf[1024];
    char recvBuf[1024];
    while (true) {
        cout << "Enter message (or 'exit' to quit): ";
        cin.getline(sendBuf, sizeof(sendBuf));
        if (strcmp(sendBuf, "exit") == 0) break;

        int sent = send(sock, sendBuf, strlen(sendBuf), 0);
        if (sent <= 0) break;

        int received = recv(sock, recvBuf, sizeof(recvBuf) - 1, 0);
        if (received > 0) {
            recvBuf[received] = '\0';
            cout << "Server echoed: " << recvBuf << endl;
        } else {
            break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}