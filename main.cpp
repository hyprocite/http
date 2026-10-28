#include "event_loop.hpp"
#include "socket_utils.hpp"
#include "connection.hpp"
#include "chat_server.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

int main() {
    constexpr uint16_t kPort = 8081;
    int listen_fd = create_listen_socket(kPort);
    if (listen_fd == -1) {
        std::cerr << "create_listen_socket failed: "
                  << std::strerror(errno) << "\n";
        return 1;
    }
    std::cout << "服务器启动，监听 " << kPort << " 端口...\n";

    Eventloop loop;
    ChatServer chat(&loop);

    // 连接表：fd → shared_ptr<Connection>，保活用
    std::unordered_map<int, std::shared_ptr<Connection>> connections;

    chat.set_close_hook([&connections](const std::shared_ptr<Connection>& c) {
        for(auto conn : connections)
        {
            if(conn.second == c)
            {
                std::cout << "连接关闭 fd=" << conn.first << "\n";
                connections.erase(conn.first);
                break;
            } 
        }
        });
    loop.add_fd(listen_fd, EPOLLIN, [&](int, uint32_t) {
        while (true) {
            sockaddr_in client_addr{};
            socklen_t   client_len = sizeof(client_addr);
            int client_fd = ::accept(listen_fd,
                                     reinterpret_cast<sockaddr*>(&client_addr),
                                     &client_len);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                std::cerr << "accept failed: " << std::strerror(errno) << "\n";
                break;
            }
            if (set_nonblocking(client_fd) == -1) {
                std::cerr << "set_nonblocking failed: " << std::strerror(errno) << "\n";
                ::close(client_fd);
                continue;
            }

            char ip[INET_ADDRSTRLEN] = {0};
            ::inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
            std::cout << "新连接: " << ip << ":"
                      << ntohs(client_addr.sin_port)
                      << " (fd=" << client_fd << ")\n";

            auto conn = std::make_shared<Connection>(&loop, client_fd);


             connections[client_fd] = conn;

            // 关闭时从连接表移除。注意：close() 里 fd_ 会置 -1，
            // 所以捕获原 fd 值，别用 c->fd()
            int fd_copy = client_fd;


           
            // ChatServer 内部会 setMessageCallback / setCloseCallback / start
            chat.on_new_connection(conn);
        }
    });

    loop.run();

    ::close(listen_fd);
    return 0;
}
