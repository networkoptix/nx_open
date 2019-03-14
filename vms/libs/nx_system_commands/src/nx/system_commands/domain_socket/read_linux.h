#pragma once

#include <stdint.h>
#include <stdio.h>

namespace nx {
namespace system_commands {
namespace domain_socket {

int readFd(int fd);
bool readInt64(int fd, int64_t* value);

/** Calls reallocCallback() with required buffer size. A pointer to this buffer should be provided
 *  as a reallocCallback() return value.
 */
bool readBuffer(int fd, void* (*reallocCallback)(void*, ssize_t), void* reallocContext);

} // namespace domain_socket
} // namespace system_commands
} // namespace nx
