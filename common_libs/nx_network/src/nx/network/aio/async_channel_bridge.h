#pragma once

#include <chrono>

#include <boost/optional.hpp>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "basic_pollable.h"
#include "timer.h"
#include "detail/async_channel_unidirectional_bridge.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Bridge between two asynchronous channels (e.g., sockets).
 * Reads data from both channels and writes to another one.
 * Seeks for loading both channels simultaneously regardless 
 *   of channel speed difference to maximize throughput.
 * Supports:
 * - Inactivity timeout.
 * - Limit on size of data waiting to be written.
 *
 * Example of usage:
 * @code {*.cpp}
 *   auto bridge = nx::network::makeAsyncChannelBridge(socket1, socket2);
 *   bridge->start(functor_executed_on_bridge_closure);
 * @endcode
 */
class AsyncChannelBridge:
    public aio::BasicPollable
{
public:
    static constexpr std::size_t kDefaultReadBufferSize = 16 * 1024;
    static constexpr std::size_t kDefaultMaxSendQueueSizeBytes = 128 * 1024;

    virtual ~AsyncChannelBridge() override = default;

    virtual void setReadBufferSize(std::size_t readBufferSize) = 0;
    virtual void setMaxSendQueueSizeBytes(std::size_t maxSendQueueSizeBytes) = 0;
    virtual void setInactivityTimeout(std::chrono::milliseconds) = 0;
    virtual void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> doneHandler) = 0;
};

template<typename LeftFile, typename RightFile>
class AsyncChannelBridgeImpl:
    public AsyncChannelBridge
{
    using base_type = AsyncChannelBridge;

public:
    AsyncChannelBridgeImpl(LeftFile leftFile, RightFile rightFile):
        m_closedChannelCount(0)
    {
        m_leftFile = std::move(leftFile);
        m_rightFile = std::move(rightFile);

        initializeOneWayChannel(&m_leftToRight, &m_leftFile, &m_rightFile);
        initializeOneWayChannel(&m_rightToLeft, &m_rightFile, &m_leftFile);

        bindToAioThread(getAioThread());
    }

    virtual ~AsyncChannelBridgeImpl() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_leftFile->bindToAioThread(aioThread);
        m_rightFile->bindToAioThread(aioThread);
        m_timer.bindToAioThread(aioThread);
    }

    virtual void setReadBufferSize(std::size_t readBufferSize) override
    {
        m_leftToRight->setReadBufferSize(readBufferSize);
        m_rightToLeft->setReadBufferSize(readBufferSize);
    }

    virtual void setMaxSendQueueSizeBytes(std::size_t maxSendQueueSizeBytes) override
    {
        m_leftToRight->setMaxSendQueueSizeBytes(maxSendQueueSizeBytes);
        m_rightToLeft->setMaxSendQueueSizeBytes(maxSendQueueSizeBytes);
    }

    virtual void setInactivityTimeout(std::chrono::milliseconds timeout) override
    {
        m_inactivityTimeout = timeout;
    }

    virtual void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> doneHandler) override
    {
        using namespace std::placeholders;

        m_onDoneHandler = std::move(doneHandler);
        post(
            [this]()
            {
                m_leftToRight->start(
                    std::bind(&AsyncChannelBridgeImpl::handleChannelClosure, this, _1));
                m_rightToLeft->start(
                    std::bind(&AsyncChannelBridgeImpl::handleChannelClosure, this, _1));
                startInactivityTimerIfNeeded();
            });
    }

private:
    std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<LeftFile, RightFile>> m_leftToRight;
    std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<RightFile, LeftFile>> m_rightToLeft;

    LeftFile m_leftFile;
    RightFile m_rightFile;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;
    int m_closedChannelCount;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    aio::Timer m_timer;
    std::chrono::steady_clock::time_point m_prevActivityTime;

    virtual void stopWhileInAioThread() override
    {
        m_timer.pleaseStopSync();
        m_leftToRight.reset();
        m_rightToLeft.reset();

        if (m_leftFile)
        {
            m_leftFile->pleaseStopSync();
            m_leftFile = nullptr;
        }
        if (m_rightFile)
        {
            m_rightFile->pleaseStopSync();
            m_rightFile = nullptr;
        }
    }

    template<typename Source, typename Destination>
    void initializeOneWayChannel(
        std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<Source, Destination>>* channel,
        Source* source,
        Destination* destination)
    {
        *channel =
            std::make_unique<detail::AsyncChannelUnidirectionalBridge<Source, Destination>>(
                *source,
                *destination);
        (*channel)->setReadBufferSize(AsyncChannelBridge::kDefaultReadBufferSize);
        (*channel)->setMaxSendQueueSizeBytes(AsyncChannelBridge::kDefaultMaxSendQueueSizeBytes);
        (*channel)->setOnSomeActivity([this]() { onSomeActivity(); });
    }

    void handleChannelClosure(SystemError::ErrorCode reason)
    {
        ++m_closedChannelCount;
        if (m_closedChannelCount < 2)
            return;
        reportFailure(reason);
    }

    void startInactivityTimerIfNeeded()
    {
        if (!m_inactivityTimeout)
            return;

        m_prevActivityTime = std::chrono::steady_clock::now();
        m_timer.start(
            *m_inactivityTimeout,
            std::bind(&AsyncChannelBridgeImpl::onInactivityTimer, this));
    }

    void onInactivityTimer()
    {
        using namespace std::chrono;

        const auto timeSincePrevActivity = steady_clock::now() - m_prevActivityTime;
        if (timeSincePrevActivity >= *m_inactivityTimeout)
            return reportFailure(SystemError::timedOut);

        m_timer.start(
            duration_cast<milliseconds>(*m_inactivityTimeout - timeSincePrevActivity),
            std::bind(&AsyncChannelBridgeImpl::onInactivityTimer, this));
    }

    void onSomeActivity()
    {
        m_prevActivityTime = std::chrono::steady_clock::now();
    }
    
    void reportFailure(SystemError::ErrorCode sysErrorCode)
    {
        m_leftFile->cancelIOSync(aio::EventType::etNone);
        m_rightFile->cancelIOSync(aio::EventType::etNone);

        if (m_onDoneHandler)
            m_onDoneHandler(sysErrorCode);
    }
};

/**
 * LeftFile and RightFile types must be at least movable.
 * That allows them to be std::unique_ptr<Something>.
 */
template<typename LeftFile, typename RightFile>
std::unique_ptr<AsyncChannelBridgeImpl<LeftFile, RightFile>> makeAsyncChannelBridge(
    LeftFile leftFile, RightFile rightFile)
{
    return std::make_unique<AsyncChannelBridgeImpl<LeftFile, RightFile>>(
        std::move(leftFile),
        std::move(rightFile));
}

} // namespace aio
} // namespace network
} // namespace nx
