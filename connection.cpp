#include "connection.hpp"
#include "event_loop.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
Connection::Connection(Eventloop* loop, int fd)
:loop_(loop),
    fd_(fd),
    closed_(false)
{
    
}

void Connection::start()
{
    auto self = shared_from_this();
    loop_->add_fd(fd_,EPOLLIN,[self](int,uint32_t){self->on_readable();});
}

void Connection::on_readable()
{
    char temp[4096];
    while ((true))
    {
        size_t n = ::recv(fd_,temp,sizeof(temp),0);

        if(n>0)
        {   
            last_data_.assign(temp, static_cast<size_t>(n));

            if(message_cb_)
            {
                message_cb_(shared_from_this());
            }
            if(closed_) return;
        }
        else if(n ==0 )
        {
            close();
            return;
        }
        else
        {
             if (errno == EAGAIN || errno == EWOULDBLOCK) 
             {              
                break;  // 数据读完了
            }
            if (errno == EINTR) 
            {
                            continue;
            }
            close();
            return;
        }
    }
    
}

void Connection::send(const std::string& data)
{

}

void Connection::close()
{   
    if(closed_) return;
    closed_ = true;
    if(close_cb_)
    {
        close_cb_(shared_from_this());
    }
    loop_ ->del_fd(fd_);
    ::close(fd_);
}