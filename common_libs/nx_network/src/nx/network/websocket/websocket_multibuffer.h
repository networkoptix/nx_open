#pragma once

#include <deque>
#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace websocket {

/**
 * Multi-frame(message) buffer. Represents a deque (queue) of buffers.
 * Appends are allowed only to the back buffer. After back buffer is full it
 * can be locked. Only locked buffers are allowed for pop.
 */
class NX_NETWORK_API MultiBuffer
{
    struct LockableBuffer
    {
        nx::Buffer buffer;
        bool locked;

        LockableBuffer() : locked(false) {}
    };

    using BuffersType = std::deque<LockableBuffer>;

public:
    int readySize() const;

    void append(const nx::Buffer& buf);
    void append(const char* data, int size);

    void lock();
    bool locked() const;

    nx::Buffer popBack();
    nx::Buffer popFront();

    void clear();
    void clearLast();

private:
    BuffersType m_buffers;
};

} // namespace websocket
} // namespace network
} // namespace nx
