#include "event_loop.hpp"
#include "socket_utils.hpp"
#include "connection.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include<unordered_map>
int main() {
    // 1. 创建监听 socket（内部已 bind + listen + 非阻塞）
    constexpr uint16_t kPort = 8080;
    int listen_fd = create_listen_socket(kPort);
    if (listen_fd == -1) {
        std::cerr << "create_listen_socket failed: "
                  << std::strerror(errno) << "\n";
        return 1;
    }
    std::cout << "服务器启动，监听 " << kPort << " 端口...\n";

    // 2. 创建事件循环
    Eventloop loop;
    std::unordered_map<int,Connection> connetions;
    // 3. 注册监听 socket 的回调：有新连接时循环 accept
    loop.add_fd(listen_fd, EPOLLIN,
                [&loop, listen_fd](int, uint32_t) {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd,
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &client_len);

            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // 没有更多连接
                }
                if (errno == EINTR) {
                    continue;  // 被信号打断，重试
                }
                std::cerr << "accept failed: "
                          << std::strerror(errno) << "\n";
                break;
            }

            // 新连接设为非阻塞
            if (set_nonblocking(client_fd) == -1) {
                std::cerr << "set_nonblocking failed: "
                          << std::strerror(errno) << "\n";
                ::close(client_fd);
                continue;
            }

            char ip[INET_ADDRSTRLEN] = {0};
            ::inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            std::cout << "新连接: " << ip << ":"
                      << ntohs(client_addr.sin_port)
                      << " (fd=" << client_fd << ")\n";

            // 为新客户端注册回调
           auto conn = std::make_shared<Connection>(&loop, client_fd);

            conn->setMessageCallback([&connetions](const std::shared_ptr<Connection>& c)
                {

                }
            )

            conn->setCloseCallback([&connetions](const std::share_ptr<Connetcion>& c)
            {
                connetions.erase(c->fd());
            })

            connetions[client_fd] =  conn;
            conn->start();
        }
    });

    // 4. 运行事件循环（阻塞直到 stop() 被调用）
    loop.run();

    ::close(listen_fd);
    return 0;
}