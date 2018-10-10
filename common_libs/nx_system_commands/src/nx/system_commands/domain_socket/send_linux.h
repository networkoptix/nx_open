#pragma once

#include <stdint.h>
#include <stdio.h>

namespace nx {
namespace system_commands {
namespace domain_socket {

int createConnectedSocket(const char* path);
bool sendFd(int transportFd, int fd);
bool sendInt64(int transportFd, int64_t value);
bool sendBuffer(int transportFd, const void* data, ssize_t size);

} // domain_socket
} // namespace system_commands
} // namespace nx
