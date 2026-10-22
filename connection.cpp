
#include "connection.hpp"
#include "event_loop.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(Eventloop* loop, int fd)
    : loop_(loop), fd_(fd) {
    if (!loop_ || fd_ < 0) {
        throw std::invalid_argument("invalid Connection arguments");
    }
}

Connection::~Connection() {
    // 正常路径应通过 close() 清理。
    // 析构函数提供最后一道 fd 资源保护。
    if (fd_ >= 0) {
        if (registered_ && loop_) {
            loop_->del_fd(fd_);
        }
        ::close(fd_);
        fd_ = -1;
    }
}

bool Connection::start() {
    if (closed_ || fd_ < 0 || registered_) return false;

    auto self = shared_from_this();

    if (!loop_->add_fd(fd_, EPOLLIN | EPOLLRDHUP,
        [self](int, uint32_t events) {
            self->on_event(events);
        })) {
        return false;
    }

    registered_ = true;
    return true;
}

void Connection::on_event(uint32_t events) {
    if (closed_) return;

    if (events & EPOLLERR) {
        close();
        return;
    }

    if (events & EPOLLIN) {
        on_readable();
    }

    if (closed_) return;

    if (events & EPOLLOUT) {
        on_writable();
    }

    if (closed_) return;

    // 如果有可读数据，优先让 on_readable() 读到 EOF。
    // 否则 HUP/RDHUP 说明对端已关闭或半关闭连接。
    if ((events & (EPOLLHUP | EPOLLRDHUP)) &&
        !(events & EPOLLIN)) {
        close();
    }
}

void Connection::on_readable() {
    if (closed_) return;

    char temp[4096];

    while (true) {
        const ssize_t n = ::recv(fd_, temp, sizeof(temp), 0);

        if (n > 0) {
            input_buffer_.append(
                std::string_view(temp, static_cast<size_t>(n))
            );

            if (message_cb_) {
                message_cb_(shared_from_this());
            }

            if (closed_) return;
            continue;
        }

        if (n == 0) {
            close();
            return;
        }

        if (errno == EINTR) continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        std::cerr << "recv failed: " << std::strerror(errno) << '\n';
        close();
        return;
    }
}

void Connection::send(const std::string& data, bool close_after_send) {
    if (closed_) return;

    output_buffer_.append(data);

    if (close_after_send) {
        close_after_write_ = true;
    }

    flush_output();
}

void Connection::on_writable() {
    if (!closed_) flush_output();
}

void Connection::flush_output() {
    while (!closed_ && output_offset_ < output_buffer_.size()) {
        const char* data = output_buffer_.data() + output_offset_;
        const size_t remaining = output_buffer_.size() - output_offset_;

        const ssize_t n = ::send(fd_, data, remaining, MSG_NOSIGNAL);

        if (n > 0) {
            output_offset_ += static_cast<size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR) continue;

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            update_interest();
            return;
        }

        std::cerr << "send failed: " << std::strerror(errno) << '\n';
        close();
        return;
    }

    if (closed_) return;

    output_buffer_.clear();
    output_offset_ = 0;

    if (close_after_write_) {
        close();
        return;
    }

    update_interest();
}

void Connection::update_interest() {
    if (closed_ || !registered_) return;

    uint32_t events = EPOLLIN | EPOLLRDHUP;

    if (output_offset_ < output_buffer_.size()) {
        events |= EPOLLOUT;
    }

    if (!loop_->mod_fd(fd_, events)) {
        std::cerr << "failed to update epoll interest for fd="
                  << fd_ << '\n';
        close();
    }
}

void Connection::close() {
    if (closed_) return;

    closed_ = true;

    // 先从 epoll 注销，防止继续收到该 fd 的事件。
    if (registered_ && loop_) {
        loop_->del_fd(fd_);
        registered_ = false;
    }

    const int old_fd = fd_;
    fd_ = -1;

    if (old_fd >= 0) {
        ::close(old_fd);
    }

    input_buffer_.clear();
    output_buffer_.clear();
    output_offset_ = 0;

    if (close_cb_) {
        auto self = shared_from_this();
        close_cb_(self);
    }
}
