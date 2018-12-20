#include "buffer_socket.h"

#include <nx/utils/random.h>

namespace nx {
namespace network {

BufferSocket::BufferSocket(const std::string& data):
    m_data(data),
    m_isOpened(false),
    m_curPos(0)
{
}

bool BufferSocket::close()
{
    m_isOpened = false;
    return true;
}

bool BufferSocket::shutdown()
{
    m_isOpened = false;
    return true;
}

bool BufferSocket::isClosed() const
{
    return !m_isOpened;
}

bool BufferSocket::connect(
    const SocketAddress& remoteSocketAddress,
    std::chrono::milliseconds timeout)
{
    if (!DummySocket::connect(remoteSocketAddress, timeout))
        return false;

    m_isOpened = true;
    m_curPos = 0;
    return true;
}

int BufferSocket::recv(void* buffer, unsigned int bufferLen, int /*flags*/)
{
    const size_t bytesToCopy = std::min<size_t>(bufferLen, m_data.size() - m_curPos);
    memcpy(buffer, m_data.data() + m_curPos, bytesToCopy);
    m_curPos += bytesToCopy;
    return bytesToCopy;
}

int BufferSocket::send(const void* /*buffer*/, unsigned int bufferLen)
{
    if (!m_isOpened)
        return -1;
    return bufferLen;
}

bool BufferSocket::isConnected() const
{
    return m_isOpened;
}

} // namespace network
} // namespace nx
