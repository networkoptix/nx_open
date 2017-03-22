#pragma once

#include <list>

#include <nx/utils/move_only_func.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace network {
namespace detail {

template<typename SourcePtr, typename DestinationPtr>
class AsyncOneWayChannel
{
public:
    AsyncOneWayChannel(
        SourcePtr& source,
        DestinationPtr& destination,
        std::size_t readBufferSize)
        :
        m_source(source),
        m_destination(destination),
        m_readBufferSize(readBufferSize),
        m_isReading(false),
        m_isSourceOpened(true),
        m_sourceClosureReason(SystemError::noError)
    {
        m_readBuffer.reserve(m_readBufferSize);
    }

    void start()
    {
        scheduleRead();
    }

    void setOnDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
    {
        m_onDoneHandler.swap(handler);
    }

private:
    enum class ChannelState
    {
        opened,
        closed,
    };

    SourcePtr& m_source;
    DestinationPtr& m_destination;
    const std::size_t m_readBufferSize;
    nx::Buffer m_readBuffer;
    std::list<nx::Buffer> m_sendQueue;
    bool m_isReading;
    bool m_isSourceOpened;
    SystemError::ErrorCode m_sourceClosureReason;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;

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
        m_readBuffer.reserve(m_readBufferSize);
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
        // TODO
        return true;
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
        if (sysErrorCode != SystemError::noError)
        {
            reportFailure(sysErrorCode);
            return;
        }

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
