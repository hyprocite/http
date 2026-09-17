#include <sys/socket.h>
#include <iostream>
#include <vector>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

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

int main() 
{

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);   //监听socket
    if (server_fd == -1) {
        std::cerr << "SOCKET CREATION FAILED\n";
        return 1;
    }

    //设置端口重用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));



    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "BIND FAILED\n";
        close(server_fd);
        return 1;
    }
    std::cout << "finish binding...\n";

    if (listen(server_fd, 3) < 0) {
        std::cerr << "LISTEN FAILED\n";
        close(server_fd);
        return 1;
    }


    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        return 1;
    }
    //创建epoll 实例 ，在socket监听完成之后   注：这里的括号是对象初始化的一种写法
    int epoll_event ev{};
    ev.events = EPOLLIN;   //监听可读事件  EPOLLIN为水平触发， EPOLLET 为边缘触发
    ev.data.fd = server_fd;   
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {   //错误抛出 ,登记进入树
        perror("epoll_ctl: listen_fd");
        close(listen_fd);
        close(epoll_fd);
        return 1;
    }



    std::vector<epoll_event> events(64);  // 事件数组，用于 epoll_wait 返回
    std::vector<char> buffer(1024);      

    while(true)
    {   
        int nfds = epoll_wait(epoll_fd,events.data(),events.size(),-1);
        for(int i = 0 ; i < nfds ; i ++)
        {
            int fd = events[i].data.fd;
            if(fd == listen_fd)
            {
                while (true) 
                {
                    sockaddr_in client_addr{};
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &client_len);
                    if (client_fd == -1) {
                        // 非阻塞模式下，如果没有更多连接，accept 返回 -1 且 errno 为 EAGAIN/EWOULDBLOCK
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;     
                        }
                        perror("accept");
                            continue;
                        }


                          set_nonblocking(client_fd);
                                         ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                        perror("epoll_ctl: client_fd");
                        close(client_fd);
                }
                    
                }

                
                //将新增的client_fd 加入epoll监听
 
            }
            
            //server_fd 只有一个 所以else 以外的fd均是客户端fd
            else
            {   
                
              while (true)   //阻塞非阻塞最大区别， 需要处理读取区域为空时的情况， 非阻塞情况下为空会返回EAGAIN
              {
                ssize_t bytes_read = recv(fd,buffer.data(),buffer.size()-1,0);
                if(bytes_read <= 0)
                {
                    if(bytes_read == 0)
                    {
                        std::cout << "客户端断开连接: fd = "<< fd << "\n";      
                    }
                    else{
                        perror("recv");
                    }
                }
                else  
                {
                    buffer[bytes_read] = '\0';
                    std::cout << "收到(fd=" << fd << "): " << buffer.data() << "\n";

                    // 回显数据
                    ssize_t bytes_sent = send(fd, buffer.data(), bytes_read, 0);
                    if (bytes_sent == -1) {
                        perror("send");}
                }


                }  
            }
        } 
    }





    close(client_fd);
    close(server_fd);
    return 0;
}