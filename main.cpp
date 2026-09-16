#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct Request {
    std::string method;
    std::string path;
    std::string version;
};

struct Response {
    int status;
    std::string reason;
    std::string body;
};

std::optional<std::string> readHeaders(int clientFd) {
    constexpr std::size_t kMaxHeaderBytes = 8 * 1024;
    const std::string headerEnd = "\r\n\r\n";
    char chunk[1024];
    std::string buffer;

    while (true) {
        const std::size_t endPos = buffer.find(headerEnd);
        if (endPos != std::string::npos) {
            if (endPos + headerEnd.size() > kMaxHeaderBytes) {
                std::cerr << "HTTP header is too long\n";
                return std::nullopt;
            }
            return buffer.substr(0, endPos + headerEnd.size());
        }

        if (buffer.size() >= kMaxHeaderBytes) {
            std::cerr << "HTTP header is too long\n";
            return std::nullopt;
        }

        const ssize_t receivedBytes = recv(clientFd, chunk, sizeof(chunk), 0);
        if (receivedBytes > 0) {
            buffer.append(chunk, static_cast<std::size_t>(receivedBytes));
        } else if (receivedBytes == 0) {
            std::cerr << "Client closed the connection before finishing headers\n";
            return std::nullopt;
        } else if (errno == EINTR) {
            continue;
        } else {
            std::perror("recv");
            return std::nullopt;
        }
    }
}

std::optional<Request> parseRequest(const std::string& rawRequest) {
    const std::size_t lineEnd = rawRequest.find("\r\n");
    if (lineEnd == std::string::npos || lineEnd == 0) {
        return std::nullopt;
    }

    const std::string requestLine = rawRequest.substr(0, lineEnd);
    std::istringstream lineStream(requestLine);
    Request request;

    if (!(lineStream >> request.method >> request.path >> request.version)) {
        return std::nullopt;
    }

    std::string extra;
    if (lineStream >> extra) {
        return std::nullopt;
    }

    if (request.version != "HTTP/1.1" || request.path.front() != '/') {
        return std::nullopt;
    }

    return request;
}

Response routeRequest(const Request& request) {
    if (request.method != "GET") {
        return Response{405, "Method Not Allowed", "Method Not Allowed"};
    }
    if (request.path == "/health") {
        return Response{200, "OK", "OK"};
    }
    if (request.path == "/hello") {
        return Response{200, "OK", "Merhaba HTTP"};
    }
    return Response{404, "Not Found", "Not Found"};
}

std::string buildResponse(const Response& response) {
    std::ostringstream output;
    output << "HTTP/1.1 " << response.status << ' ' << response.reason << "\r\n";
    output << "Content-Type: text/plain; charset=utf-8\r\n";
    output << "Content-Length: " << response.body.size() << "\r\n";
    output << "Connection: close\r\n";
    output << "\r\n";
    output << response.body;
    return output.str();
}

bool sendAll(int clientFd, const std::string& message) {
    std::size_t totalSent = 0;
    while (totalSent < message.size()) {
        const ssize_t sent = send(
            clientFd,
            message.data() + totalSent,
            message.size() - totalSent,
            0);

        if (sent > 0) {
            totalSent += static_cast<std::size_t>(sent);
        } else if (sent == 0) {
            std::cerr << "Socket closed while sending response\n";
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
    const int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        std::perror("socket");
        return 1;
    }

    const int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::perror("setsockopt");
        close(serverFd);
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(8080);

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        std::perror("bind");
        close(serverFd);
        return 1;
    }
    if (listen(serverFd, 5) == -1) {
        std::perror("listen");
        close(serverFd);
        return 1;
    }

    std::cout << "Listening on http://127.0.0.1:8080\n";
    sockaddr_in clientAddress{};
    socklen_t clientAddressSize = sizeof(clientAddress);
    const int clientFd = accept(
        serverFd,
        reinterpret_cast<sockaddr*>(&clientAddress),
        &clientAddressSize);

    if (clientFd == -1) {
        std::perror("accept");
        close(serverFd);
        return 1;
    }

    const auto rawRequest = readHeaders(clientFd);
    if (!rawRequest) {
        close(clientFd);
        close(serverFd);
        return 1;
    }

    const auto request = parseRequest(*rawRequest);
    const Response response = request
        ? routeRequest(*request)
        : Response{400, "Bad Request", "Bad Request"};

    const std::string rawResponse = buildResponse(response);
    const bool sent = sendAll(clientFd, rawResponse);
    close(clientFd);
    close(serverFd);
    return sent ? 0 : 1;
}
