#include "socket_utils.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}


int create_listen_socket(uint16_t port , int backlog = 128)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);   //监听socket
    if (listen_fd == -1) {
        std::cerr << "SOCKET CREATION FAILED\n";
        return 1;
    }

    //设置端口重用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

     if (bind(listen_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "BIND FAILED\n";
        close(listen_fd);
        return 1;
    }
    std::cout << "finish binding...\n";

    if (listen(listen_fd, 3) < 0) {
        std::cerr << "LISTEN FAILED\n";
        close(listen_fd);
        return 1;
    }
    
    if (set_nonblocking(listen_fd) == -1) {
        ::close(listen_fd);
        return -1;
    }

    return listen_fd;
}