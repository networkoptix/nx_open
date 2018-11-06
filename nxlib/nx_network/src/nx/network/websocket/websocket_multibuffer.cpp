#include "websocket_multibuffer.h"

namespace nx {
namespace network {
namespace websocket {

int MultiBuffer::readySize() const
{
    if (m_buffers.empty())
        return 0;

    return m_buffers.back().locked ? (int)m_buffers.size() : (int)m_buffers.size() - 1;
}

void MultiBuffer::append(const nx::Buffer& buf)
{
    append(buf.constData(), buf.size());
}

void MultiBuffer::append(const char* data, int size)
{
    if (m_buffers.empty() || m_buffers.back().locked)
        m_buffers.emplace_back();

    m_buffers.back().buffer.append(data, size);
}

void MultiBuffer::lock()
{
    if (m_buffers.empty())
        return;

    m_buffers.back().locked = true;
}

nx::Buffer MultiBuffer::popFront()
{
    if (m_buffers.empty() || !m_buffers.front().locked)
        return nx::Buffer();

    nx::Buffer result = std::move(m_buffers.front().buffer);
    m_buffers.pop_front();

    return result;
}

nx::Buffer MultiBuffer::popBack()
{
    if (m_buffers.empty() || !m_buffers.back().locked)
        return nx::Buffer();

    nx::Buffer result = std::move(m_buffers.back().buffer);
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

    m_buffers.back().buffer.clear();
    m_buffers.back().locked = false;
}

} // namespace websocket
} // namespace network
} // namespace nx
