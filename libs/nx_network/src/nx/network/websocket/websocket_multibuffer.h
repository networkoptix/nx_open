// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/utils/buffer.h>

#include "websocket_common_types.h"

namespace nx::network::websocket {

struct Frame
{
    nx::Buffer buffer;
    FrameType type = FrameType::text;
};

/**
 * Multi-frame(message) buffer. Represents a deque (queue) of buffers.
 * Appends are allowed only to the back buffer. After back buffer is full it
 * can be locked. Only locked buffers are allowed for pop.
 */
class NX_NETWORK_API MultiBuffer
{
    struct LockableBuffer
    {
        Frame frame;
        bool locked;

        LockableBuffer(Frame&& frame): frame(std::move(frame)), locked(false) {}
    };

    using BuffersType = std::deque<LockableBuffer>;

public:
    int size() const;

    void append(nx::Buffer&& buf, FrameType type = FrameType::text);

    void lock();
    bool locked() const;

    Frame popBack();
    Frame popFront();

    void clear();
    void clearLast();

private:
    BuffersType m_buffers;
};

} // namespace nx::network::websocket
