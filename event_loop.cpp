#include "event_loop.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <iostream>


Eventloop::Eventloop():epoll_fd_(-1), running_(false),__event(64) //构造函数, epoll create
{
    epoll_fd_ = ::epoll_create1(0);   
    if (epoll_fd_ == -1) {
        throw std::runtime_error(
            std::string("epoll_create1 failed: ") + std::strerror(errno));
        }
}

Eventloop::~Eventloop()
{  
    if(epoll_fd_ != -1){
    close(epoll_fd_);
    epoll_fd_ = -1;
}
}

Eventloop:: add_fd(int fd, uint32_t events, EventCallback cb)
{
    //逻辑， 先检查fd是否使用过， 防止回调被覆盖
    if (callbacks_.count(fd)) {
        std::cerr << "EventLoop::add_fd: fd " << fd
                  << " already registered\n";
        return false;
    }

    epoll_event ev{};        //在这里创建ev 而后添加
    ev.events = events;
    ev.data.fd = fd;

    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {   //epoll_ctl
        std::cerr << "epoll_ctl ADD fd=" << fd
                  << " failed: " << std::strerror(errno) << "\n";
        return false;
    }

    callbacks_[fd] = std::move(cb);
    return true;
}

Eventloop :: mod_fd(int fd, uint32_t events , EventCallback cb)
{
    if (!callbacks_.count(fd))  //若没使用过则报错
    {
        std::cerr << "EventLoop::mod_fd: fd " << fd
                  << " not registered\n";
        return false;
    }

        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;

        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1)
         {
        std::cerr << "epoll_ctl MOD fd=" << fd
                  << " failed: " << std::strerror(errno) << "\n";
        return false;
        }


    return true;
    
}

Eventloop :: del_fd(int fd)
{
    if()
}
Eventloop :: run()
{
    running_ = true;
    while(running_)
    {
        int n = epoll_wait(epoll_fd_,events_.data(), static_cast<int>(events_.size()));

        if(n == -1)
        {
            perror()
        }
        else if(n == 0)
        {

        }
        else
        {
            for(int i = 0 ; i < n,i++)   //整体逻辑，有事件，取出，执行
            {
                int fd = events_[i].data.fd;
                uint32_ ev = events_[i].events;
                EventCallback cb;
                auto it = callbacks_.find(fd); //取出键值对cb
                if(it ! = callbacks_.end())
                {
                    cb = it-> second; // 若不是
                }
            }
        }
    }
}



Eventloop :: stop()   //注 若这里的事件循环在epoll_wait 中阻塞 stop则不会唤醒它
{
    running_ = false;
}