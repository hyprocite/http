
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "bytes_buffer.hpp"

class Eventloop;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using MessageCallback =
        std::function<void(const std::shared_ptr<Connection>&)>;

    using CloseCallback =
        std::function<void(const std::shared_ptr<Connection>&)>;

    Connection(Eventloop* loop, int fd);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    void setMessageCallback(MessageCallback cb) {
        message_cb_ = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb) {
        close_cb_ = std::move(cb);
    }

    int fd() const noexcept { return fd_; }
    bool closed() const noexcept { return closed_; }

    bytes_buffer& input_buffer() noexcept { return input_buffer_; }
    const bytes_buffer& input_buffer() const noexcept {
        return input_buffer_;
    }

    bool start();

    void on_event(uint32_t events);
    void on_readable();
    void on_writable();

    // close_after_send=true 时，所有待发送数据写完后才关闭连接。
    void send(const std::string& data, bool close_after_send = false);

    void close();

private:
    void update_interest();
    void flush_output();

    Eventloop* loop_;
    int fd_;
    bool closed_ = false;
    bool close_after_write_ = false;
    bool registered_ = false;

    bytes_buffer input_buffer_;
    std::string output_buffer_;
    size_t output_offset_ = 0;

    MessageCallback message_cb_;
    CloseCallback close_cb_;
};
