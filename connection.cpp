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
    closed_(false){}

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
        ssize_t n = ::recv(fd_,temp,sizeof(temp),0);

        if(n>0)
        {   
          //  last_data_.assign(temp, static_cast<size_t>(n));
            m_input_buffer.append(bytes_const_view{temp,static_cast<size_t>(n)});
            if(message_cb_)
            {
                message_cb_(shared_from_this()); //在此耦合buffer中的数据和http解析逻辑
            }
            if(closed_) return;
            continue;
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
    if (closed_) return;
    m_output_buffer.append(std::string_view{data.data(), data.size()});
    try_flush_output();
}

bool Connection::try_flush_output()
{
    while (!m_output_buffer.empty())
    {
        auto [p, sz] = m_output_buffer.span();
        ssize_t n = ::send(fd_, p, sz, 0);
        if (n > 0) { m_output_buffer.consume(static_cast<size_t>(n)); continue; }
        if (n == 0) { close(); return false; }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            loop_->mod_fd(fd_, EPOLLIN | EPOLLOUT);   // 等可写
            return false;
        }
        close(); return false;
    }
    loop_->mod_fd(fd_, EPOLLIN);                       // 发完，取消 EPOLLOUT
    return true;
}

void Connection::on_writable() { if (!closed_) try_flush_output(); }

void Connection::close()
{   
    if(closed_) return;
    closed_ = true;

    loop_->del_fd(fd_);
    ::close(fd_);
    fd_ = -1;

    if(close_cb_)
    {
        close_cb_(shared_from_this());
    }
}