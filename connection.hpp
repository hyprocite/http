#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include "bytes_buffer.hpp"
class Eventloop;

class Connection : public std::enable_shared_from_this<Connection>
{
 public:
    using MessageCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    using CloseCallback = std::function<void(const std::shared_ptr<Connection>&)>;
    
    Connection(Eventloop* loop, int fd);
    ~Connection() = default;

    void setMessageCallback(MessageCallback cb) {message_cb_ = std::move(cb);}
    void setCloseCallback(CloseCallback cb){close_cb_ = std::move(cb);}


    // connection.hpp（public 区）
   bytes_buffer&       input_buffer()       noexcept { return m_input_buffer; }
   const bytes_buffer& input_buffer() const noexcept { return m_input_buffer; }
   bytes_buffer&       output_buffer()      noexcept { return m_output_buffer; }
   const bytes_buffer& output_buffer() const noexcept { return m_output_buffer; }

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    int fd() const { return fd_; }
    bool closed() const { return closed_; }
    bool try_flush_output();
    
    //实现
    void start();

    // 由 EventLoop 调用的读事件入口
    void on_readable();

    void on_writable(); 
    // 发送数据（简化版：直接 send，后续可加入 output_buffer）
    void send(const std::string& data);

    // 关闭连接
    void close();

 private:
    Eventloop* loop_;
    int fd_;
    bool closed_;
    MessageCallback message_cb_;
    bytes_buffer m_input_buffer{4096};
    bytes_buffer m_output_buffer{4096};
    CloseCallback close_cb_;

};