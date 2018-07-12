#pragma once

#include <stdint.h>
#include <stdio.h>

namespace nx {
namespace system_commands {
namespace domain_socket {

int readFd(int socketPostfix);
bool readInt64(int socketPostfix, int64_t* value);

/** Calls reallocCallback() with required buffer size. A pointer to this buffer should be provided
 *  as a reallocCallback() return value.
 */
bool readBuffer(int socketPostfix, void* (*reallocCallback)(void*, ssize_t), void* reallocContext);

} // namespace domain_socket
} // namespace system_commands
} // namespace nx
