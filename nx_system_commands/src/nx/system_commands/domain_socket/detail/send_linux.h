#pragma once

#include <stdint.h>
#include <stdio.h>

namespace nx {
namespace system_commands {
namespace domain_socket {
namespace detail {

bool sendFd(int socketPostfix, int fd);
bool sendInt64(int socketPostfix, int64_t value);
bool sendBuffer(int socketPostfix, const void* data, ssize_t size);

} // namespace detail
} // domain_socket
} // namespace system_commands
} // namespace nx
