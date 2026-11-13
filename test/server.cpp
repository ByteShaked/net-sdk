// server.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

const int PORT = 6666;
const int BUFFER_SIZE = 1024;

int main() {
    // 1. 创建Socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return -1;
    }

    // 2. 绑定地址和端口
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return -1;
    }

    // 3. 监听连接
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return -1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    // 4. 接受客户端连接
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        std::cerr << "Accept failed" << std::endl;
        close(server_fd);
        return -1;
    }
    std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) << std::endl;

    // 5. 接收数据并回显
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_read <= 0) {
            std::cout << "Client disconnected" << std::endl;
            break;
        }
        
        std::cout <<"Received: " << buffer << " bytes_read: "<<bytes_read<< std::endl;

        // 发送回显数据
        send(client_fd, buffer, bytes_read, 0);
    }

    // 6. 关闭连接
    close(client_fd);
    close(server_fd);
    return 0;
}