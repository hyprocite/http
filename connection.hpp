#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class Eventloop;

class Connection : public std::enable_shared_from_this<Connection>
{
 public:
    using MessageCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    using CloseCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    
    Connection(Eventloop* loop, int fd);
    ~Connection();

    void setMessageCallback(MessageCallback cb) {message_cb_ = std::move(cb);}
    void setCloseCallback(CloseCallback cb){close_cb_ = std::move(cb);}


    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int fd() const { return fd_; }
    bool closed() const { return closed_; }

    
    //实现
    void start();

    // 由 EventLoop 调用的读事件入口
    void on_readable();

    // 发送数据（简化版：直接 send，后续可加入 output_buffer）
    void send(const std::string& data);

    // 关闭连接
    void close();

 private:
    Eventloop* loop_;
    int fd_;
    bool closed_;
    std::string last_data_;
    MessageCallback message_cb_;
    
    CloseCallback close_cb_;

};