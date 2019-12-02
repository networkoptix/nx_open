#include "sync_reader.h"

#include <nx/utils/log/log.h>

namespace nx::vms::server::http_audio {

SyncReader::SyncReader(nx::network::aio::AsyncChannelPtr socket):
    m_socket(std::move(socket))
{
    m_buffer.reserve(1024 * 16);
}

void SyncReader::cancel()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_readComplete = true;
    m_condition.notify_one();
}

int SyncReader::read(uint8_t* buffer, int size)
{
    if (!m_socket)
        return 0;

    if (m_bufferSize == 0)
    {
        m_bufferOffset = 0;
        std::unique_lock<std::mutex> lock(m_mutex);
        m_readComplete = false;
        SystemError::ErrorCode error = SystemError::connectionAbort;
        std::size_t bytesRead;
        m_socket->readSomeAsync(&m_buffer,
            [&error, &bytesRead, this](SystemError::ErrorCode errorCode, std::size_t bytesReadSocket)
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                error = errorCode;
                bytesRead = bytesReadSocket;
                m_readComplete = true;
                m_condition.notify_one();
            });
        while(!m_readComplete)
            m_condition.wait(lock);
        if (error != SystemError::noError || bytesRead == 0)
        {
            if (error != SystemError::connectionAbort)
            {
                NX_WARNING(this, "Failed to read from socket. Error code: %1",
                    SystemError::toString(error));
            }
            m_socket.reset();
            return 0;
        }
        m_bufferSize = bytesRead;
    }

    int actualSize = std::min(size, m_bufferSize);
    if (actualSize)
        memcpy(buffer, m_buffer.data() + m_bufferOffset, actualSize);
    m_bufferSize -= actualSize;
    m_bufferOffset += actualSize;
    return actualSize;
}

} // namespace nx::vms::server::http_audio
