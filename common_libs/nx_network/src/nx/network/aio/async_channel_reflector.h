#pragma once

#include <deque>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_async_channel.h"
#include "basic_pollable.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Listens supplied channel for incoming data and sends same data in response.
 */
template<typename ChannelToReflectType>
class AsyncChannelReflector:
    public BasicPollable
{
    using base_type = BasicPollable;
    using self_type = AsyncChannelReflector<ChannelToReflectType>;

    // TODO: #ak Get rid of template after inheriting AbstractCommunicatingSocket from AbstractAsyncChannel.
public:
    static constexpr int kDefaultReadBufferSize = 16 * 1024;

    AsyncChannelReflector(std::unique_ptr<ChannelToReflectType> channelToReflect):
        m_channelToReflect(std::move(channelToReflect)),
        m_readBufferSize(kDefaultReadBufferSize)
    {
        bindToAioThread(getAioThread());
    }

    virtual ~AsyncChannelReflector() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_channelToReflect->bindToAioThread(aioThread);
    }

    void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onChannelClosedHandler)
    {
        m_onChannelClosedHandler = std::move(onChannelClosedHandler);
        scheduleRead();
    }

    /**
     * Calling it not from aio thread results in
     * blocking cancellation of I/O on underlying channel.
     */
    std::unique_ptr<ChannelToReflectType> takeChannel()
    {
        m_channelToReflect->cancelIOSync(aio::etNone);

        decltype(m_channelToReflect) result;
        result.swap(m_channelToReflect);
        return result;
    }

private:
    std::unique_ptr<ChannelToReflectType> m_channelToReflect;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onChannelClosedHandler;
    nx::Buffer m_readBuffer;
    std::size_t m_readBufferSize;
    std::deque<nx::Buffer> m_sendQueue;

    virtual void stopWhileInAioThread() override
    {
        m_channelToReflect.reset();
    }

    void scheduleRead()
    {
        using namespace std::placeholders;

        m_readBuffer.reserve(static_cast<int>(m_readBufferSize));
        m_channelToReflect->readSomeAsync(
            &m_readBuffer,
            std::bind(&self_type::onDataRead, this, _1, _2));
    }

    void onDataRead(SystemError::ErrorCode sysErrorCode, std::size_t bytesRead)
    {
        if (sysErrorCode != SystemError::noError &&
            socketCannotRecoverFromError(sysErrorCode))
        {
            return reportDone(sysErrorCode);
        }

        if (sysErrorCode != SystemError::noError)
            return scheduleRead();  //< Recoverable error. Retrying...

        m_readBuffer.resize(static_cast<int>(bytesRead));
        decltype(m_readBuffer) readBuffer;
        readBuffer.swap(m_readBuffer);

        m_sendQueue.push_back(std::move(readBuffer));
        if (m_sendQueue.size() == 1)
            sendNextBuffer();

        // TODO: #ak Put a mimit on send queue size.
        scheduleRead();
    }

    void reportDone(SystemError::ErrorCode sysErrorCode)
    {
        m_channelToReflect->cancelIOSync(aio::etNone);

        decltype(m_onChannelClosedHandler) onChannelClosedHandler;
        onChannelClosedHandler.swap(m_onChannelClosedHandler);
        onChannelClosedHandler(sysErrorCode);
    }

    void sendNextBuffer()
    {
        using namespace std::placeholders;

        m_channelToReflect->sendAsync(
            m_sendQueue.front(),
            std::bind(&self_type::onDataSent, this, _1, _2));
    }

    void onDataSent(SystemError::ErrorCode sysErrorCode, std::size_t bytesSent)
    {
        if (sysErrorCode != SystemError::noError &&
            socketCannotRecoverFromError(sysErrorCode))
        {
            return reportDone(sysErrorCode);
        }

        if (sysErrorCode != SystemError::noError)
        {
            // Recoverable error has occured. Retrying operation.
            sendNextBuffer();
            return;
        }

        NX_ASSERT(bytesSent == (std::size_t)m_sendQueue.front().size());

        m_sendQueue.pop_front();
        if (!m_sendQueue.empty())
            sendNextBuffer();
    }
};

template<typename ChannelToReflectType>
std::unique_ptr<AsyncChannelReflector<ChannelToReflectType>> makeAsyncChannelReflector(
    std::unique_ptr<ChannelToReflectType> channelToReflect)
{
    return std::make_unique<AsyncChannelReflector<ChannelToReflectType>>(
        std::move(channelToReflect));
}

} // namespace aio
} // namespace network
} // namespace nx
