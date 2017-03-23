#pragma once

#include <list>

#include <nx/utils/move_only_func.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace network {
namespace detail {

class BufferQueue
{
public:
    BufferQueue():
        m_sizeBytes(0)
    {
    }

    bool empty() const
    {
        return m_buffers.empty();
    }

    template<typename BufferRef>
    void push_back(BufferRef&& buffer)
    {
        m_buffers.push_back(std::forward<BufferRef>(buffer));
        m_sizeBytes += m_buffers.back().size();
    }

    std::size_t size() const
    {
        return m_buffers.size();
    }

    std::size_t sizeBytes() const
    {
        return m_sizeBytes;
    }

    const nx::Buffer& front() const
    {
        return m_buffers.front();
    }

    void pop_front()
    {
        m_sizeBytes -= m_buffers.front().size();
        m_buffers.pop_front();
    }

private:
    std::list<nx::Buffer> m_buffers;
    std::size_t m_sizeBytes;
};

template<typename SourcePtr, typename DestinationPtr>
class AsyncChannelUnidirectionalBridge
{
public:
    AsyncChannelUnidirectionalBridge(
        SourcePtr& source,
        DestinationPtr& destination)
        :
        m_source(source),
        m_destination(destination),
        m_readBufferSize(16*1024),
        m_maxSendQueueSizeBytes(128*1024),
        m_isReading(false),
        m_isSourceOpened(true),
        m_sourceClosureReason(SystemError::noError)
    {
        m_readBuffer.reserve((int)m_readBufferSize);
    }

    void setReadBufferSize(std::size_t readBufferSize)
    {
        m_readBufferSize = readBufferSize;
        m_readBuffer.reserve((int)m_readBufferSize);
    }

    void setMaxSendQueueSizeBytes(std::size_t maxSendQueueSizeBytes)
    {
        m_maxSendQueueSizeBytes = maxSendQueueSizeBytes;
    }

    void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> doneHandler)
    {
        m_onDoneHandler = std::move(doneHandler);
        scheduleRead();
    }

    void setOnSomeActivity(nx::utils::MoveOnlyFunc<void()> handler)
    {
        m_onSomeActivityHander = std::move(handler);
    }

private:
    enum class ChannelState
    {
        opened,
        closed,
    };

    SourcePtr& m_source;
    DestinationPtr& m_destination;
    std::size_t m_readBufferSize;
    std::size_t m_maxSendQueueSizeBytes;
    nx::Buffer m_readBuffer;
    BufferQueue m_sendQueue;
    bool m_isReading;
    bool m_isSourceOpened;
    SystemError::ErrorCode m_sourceClosureReason;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;
    nx::utils::MoveOnlyFunc<void()> m_onSomeActivityHander;

    void scheduleRead()
    {
        using namespace std::placeholders;

        m_source->readSomeAsync(
            &m_readBuffer,
            [this](
                SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
            {
                onDataRead(sysErrorCode, bytesRead);
            });
        m_isReading = true;
    }

    void onDataRead(
        SystemError::ErrorCode sysErrorCode,
        std::size_t bytesRead)
    {
        if (m_onSomeActivityHander)
            m_onSomeActivityHander();

        m_isReading = false;

        if (sysErrorCode != SystemError::noError || bytesRead == 0)
        {
            m_isSourceOpened = false;
            m_sourceClosureReason =
                sysErrorCode != SystemError::noError
                ? sysErrorCode
                : SystemError::connectionReset;

            if (m_sendQueue.empty())
                reportFailure(sysErrorCode);
            return;
        }

        processIncomingData();
    }

    void processIncomingData()
    {
        nx::Buffer tmp;
        m_readBuffer.swap(tmp);
        scheduleSend(std::move(tmp));
        m_readBuffer.reserve((int)m_readBufferSize);
        if (!readyToAcceptMoreData())
            return;
        scheduleRead();
    }

    void scheduleSend(nx::Buffer data)
    {
        m_sendQueue.push_back(std::move(data));
        if (m_sendQueue.size() > 1) //< Send already in progress?
            return;
        sendNextDataChunk();
    }

    bool readyToAcceptMoreData()
    {
        return m_sendQueue.sizeBytes() < m_maxSendQueueSizeBytes;
    }

    void sendNextDataChunk()
    {
        using namespace std::placeholders;

        m_destination->sendAsync(
            m_sendQueue.front(),
            [this](
                SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
            {
                onDataSent(sysErrorCode, bytesRead);
            });
    }

    void onDataSent(
        SystemError::ErrorCode sysErrorCode,
        std::size_t /*bytesRead*/)
    {
        if (m_onSomeActivityHander)
            m_onSomeActivityHander();

        if (sysErrorCode != SystemError::noError)
            return reportFailure(sysErrorCode);

        m_sendQueue.pop_front();
        if (!m_isSourceOpened && m_sendQueue.empty())
            return reportFailure(m_sourceClosureReason);

        if (!m_sendQueue.empty())
            sendNextDataChunk();

        if (!readyToAcceptMoreData())
        {
            NX_ASSERT(!m_sendQueue.empty());
            return;
        }

        // Resuming read, if needed.
        if (!m_isReading)
            scheduleRead();
    }

    void reportFailure(SystemError::ErrorCode sysErrorCode)
    {
        m_source->cancelIOSync(aio::EventType::etRead);
        m_destination->cancelIOSync(aio::EventType::etWrite);

        m_onDoneHandler(sysErrorCode);
    }
};

} // namespace detail
} // namespace network
} // namespace nx
