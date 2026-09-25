#include "event_loop.hpp"
#include "socket_utils.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

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
    EventLoop loop;

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
            loop.add_fd(client_fd, EPOLLIN,
                        [&loop](int fd, uint32_t) {
                std::vector<char> buffer(1024);
                bool should_close = false;

                // 循环读，直到 EAGAIN
                while (true) {
                    ssize_t n = ::recv(fd, buffer.data(),
                                       buffer.size() - 1, 0);
                    if (n > 0) {
                        buffer[n] = '\0';
                        std::cout << "收到(fd=" << fd << "): "
                                  << buffer.data() << "\n";
                        // 回显
                        ssize_t sent = send_all(fd, buffer.data(),
                                                static_cast<size_t>(n));
                        if (sent == -1) {
                            std::cerr << "send failed: "
                                      << std::strerror(errno) << "\n";
                            should_close = true;
                            break;
                        }
                    } else if (n == 0) {
                        std::cout << "客户端断开: fd=" << fd << "\n";
                        should_close = true;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // 数据读完了
                        }
                        if (errno == EINTR) {
                            continue;
                        }
                        std::cerr << "recv failed: "
                                  << std::strerror(errno) << "\n";
                        should_close = true;
                        break;
                    }
                }

                if (should_close) {
                    loop.del_fd(fd);   // 先摘除回调，防止后续事件
                    ::close(fd);
                }
            });
        }
    });

    // 4. 运行事件循环（阻塞直到 stop() 被调用）
    loop.run();

    ::close(listen_fd);
    return 0;
}