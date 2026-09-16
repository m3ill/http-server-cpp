#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

bool sendAll(int socketFd, const std::string& message) {
    std::size_t totalSent = 0;

    while (totalSent < message.size()) {
        const ssize_t sent = send(
            socketFd,
            message.data() + totalSent,
            message.size() - totalSent,
            0);

        if (sent > 0) {
            totalSent += static_cast<std::size_t>(sent);
        } else if (sent == 0) {
            std::cerr << "Socket closed while sending request\n";
            return false;
        } else if (errno == EINTR) {
            continue;
        } else {
            std::perror("send");
            return false;
        }
    }

    return true;
}

int main() {
    const int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == -1) {
        std::perror("socket");
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) != 1) {
        std::cerr << "Invalid server address\n";
        close(socketFd);
        return 1;
    }

    if (connect(socketFd, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::perror("connect");
        close(socketFd);
        return 1;
    }

    const std::string request =
        "GET /health HTTP/1.1\r\n"
        "Host: 127.0.0.1:8080\r\n"
        "Connection: close\r\n"
        "\r\n";

    if (!sendAll(socketFd, request)) {
        close(socketFd);
        return 1;
    }

    char chunk[1024];
    while (true) {
        const ssize_t receivedBytes = recv(socketFd, chunk, sizeof(chunk), 0);

        if (receivedBytes > 0) {
            std::cout.write(chunk, receivedBytes);
        } else if (receivedBytes == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            std::perror("recv");
            close(socketFd);
            return 1;
        }
    }

    close(socketFd);
    return 0;
}
