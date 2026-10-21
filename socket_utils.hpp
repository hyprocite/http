#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

// 将 fd 设为非阻塞。成功返回 0，失败返回 -1 并设置 errno。
int set_nonblocking(int fd);

// 创建监听 socket：socket + setsockopt(SO_REUSEADDR) + bind + listen，
// 并自动设为非阻塞。成功返回 listen_fd，失败返回 -1 并设置 errno。
int create_listen_socket(uint16_t port, int backlog = 128);

// 循环发送 len 字节，处理部分发送。
// 返回实际发送的字节数；出错返回 -1。
//ssize_t send_all(int fd, const char* data, size_t len);