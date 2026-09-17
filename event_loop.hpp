#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include <sys/epoll.h>

class Eventloop
{
    public
    using EventCallback = std::function<void(int fd, uint32_t events)>;
    EventLoop();
    ~EventLoop();
    bool add_fd(int fd, uint32_t events, EventCallback cb);
    bool mod_fd(int fd, uint32_t events);
    bool del_fd(int fd);
    void run();
    void stop();
private:
    int epoll_fd_;         
    bool running_;    //运行或终止的标志位
    std::unordered_map<int, EventCallback> callbacks_;  //回调fd
    std::vector<epoll_event> events_;   
}