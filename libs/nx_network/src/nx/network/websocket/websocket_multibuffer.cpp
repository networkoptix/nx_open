// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "websocket_multibuffer.h"

namespace nx::network::websocket {

int MultiBuffer::size() const
{
    if (m_buffers.empty())
        return 0;

    return m_buffers.back().locked ? (int)m_buffers.size() : (int)m_buffers.size() - 1;
}

void MultiBuffer::append(nx::Buffer&& buf, FrameType type)
{
    if (m_buffers.empty() || m_buffers.back().locked)
        m_buffers.emplace_back(Frame{std::move(buf), type});
    else
        m_buffers.back().frame.buffer.append(buf.data(), buf.size()); //< No check for type change.
}

void MultiBuffer::lock()
{
    if (m_buffers.empty())
        return;

    m_buffers.back().locked = true;
}

Frame MultiBuffer::popFront()
{
    if (m_buffers.empty() || !m_buffers.front().locked)
        return Frame();

    auto result = std::move(m_buffers.front().frame);
    m_buffers.pop_front();

    return result;
}

Frame MultiBuffer::popBack()
{
    if (m_buffers.empty() || !m_buffers.back().locked)
        return Frame();

    auto result = std::move(m_buffers.back().frame);
    m_buffers.pop_back();

    return result;
}

bool MultiBuffer::locked() const
{
    return !m_buffers.empty() && m_buffers.back().locked;
}

void MultiBuffer::clear()
{
    m_buffers.clear();
}

void MultiBuffer::clearLast()
{
    if (m_buffers.empty())
        return;

    m_buffers.back().frame.buffer.clear();
    m_buffers.back().locked = false;
}

} // namespace nx::network::websocket
