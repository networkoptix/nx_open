// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <optional>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "basic_pollable.h"
#include "timer.h"
#include "detail/async_channel_unidirectional_bridge.h"

namespace nx::network::aio {

/**
 * Bridge between two asynchronous channels (e.g., sockets).
 * Reads data from first channel and writes to the second and vice versa.
 * Seeks for loading both channels simultaneously regardless of channel speed difference to
 * maximize throughput.
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

/**
 * LeftChannelPtr and RightChannelPtr are either smart or raw pointers.
 */
template<typename LeftChannelPtr, typename RightChannelPtr>
class AsyncChannelBridgeImpl:
    public AsyncChannelBridge
{
    using base_type = AsyncChannelBridge;

    using LeftChannel = std::decay_t<decltype(*LeftChannelPtr())>;
    using RightChannel = std::decay_t<decltype(*RightChannelPtr())>;

public:
    AsyncChannelBridgeImpl(LeftChannelPtr leftChannel, RightChannelPtr rightChannel):
        m_closedChannelCount(0)
    {
        m_leftChannel = std::move(leftChannel);
        m_rightChannel = std::move(rightChannel);

        initializeOneWayChannel(&m_leftToRight, *m_leftChannel, *m_rightChannel);
        initializeOneWayChannel(&m_rightToLeft, *m_rightChannel, *m_leftChannel);

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
        m_leftChannel->bindToAioThread(aioThread);
        m_rightChannel->bindToAioThread(aioThread);
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
                    std::bind(&AsyncChannelBridgeImpl::handleChannelClosure, this,
                        _1, m_rightToLeft.get()));
                m_rightToLeft->start(
                    std::bind(&AsyncChannelBridgeImpl::handleChannelClosure, this,
                        _1, m_leftToRight.get()));
                startInactivityTimerIfNeeded();
            });
    }

private:
    std::unique_ptr<
        detail::AsyncChannelUnidirectionalBridgeImpl<LeftChannel*, RightChannel*>
    > m_leftToRight;
    std::unique_ptr<
        detail::AsyncChannelUnidirectionalBridgeImpl<RightChannel*, LeftChannel*>
    > m_rightToLeft;

    LeftChannelPtr m_leftChannel;
    RightChannelPtr m_rightChannel;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;
    int m_closedChannelCount;
    std::optional<std::chrono::milliseconds> m_inactivityTimeout;
    aio::Timer m_timer;
    std::chrono::steady_clock::time_point m_prevActivityTime;

    virtual void stopWhileInAioThread() override
    {
        m_timer.pleaseStopSync();
        m_leftToRight.reset();
        m_rightToLeft.reset();

        if (m_leftChannel)
        {
            m_leftChannel->pleaseStopSync();
            m_leftChannel = nullptr;
        }
        if (m_rightChannel)
        {
            m_rightChannel->pleaseStopSync();
            m_rightChannel = nullptr;
        }
    }

    template<typename Source, typename Destination>
    // requires AsyncChannel<Source> && AsyncChannel<Destination>
    void initializeOneWayChannel(
        std::unique_ptr<detail::AsyncChannelUnidirectionalBridgeImpl<Source*, Destination*>>* channel,
        Source& source,
        Destination& destination)
    {
        *channel =
            std::make_unique<detail::AsyncChannelUnidirectionalBridgeImpl<Source*, Destination*>>(
                &source,
                &destination);
        (*channel)->setReadBufferSize(AsyncChannelBridge::kDefaultReadBufferSize);
        (*channel)->setMaxSendQueueSizeBytes(AsyncChannelBridge::kDefaultMaxSendQueueSizeBytes);
        (*channel)->setOnSomeActivity([this]() { onSomeActivity(); });
    }

    void handleChannelClosure(
        SystemError::ErrorCode reason,
        const detail::AsyncChannelUnidirectionalBridge* theOtherUnidirectionalBridge)
    {
        ++m_closedChannelCount;
        if (m_closedChannelCount < 2 &&
            !theOtherUnidirectionalBridge->isSendQueueEmpty())
        {
            return;
        }
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
        m_leftChannel->cancelIOSync(aio::EventType::etNone);
        m_rightChannel->cancelIOSync(aio::EventType::etNone);

        if (m_onDoneHandler)
            m_onDoneHandler(sysErrorCode);
    }
};

/**
 * LeftChannel and RightChannel types must meet the requirements of MoveConstructible
 * and support AbstractAsyncChannel API.
 * NOTE: LeftChannelPtr and RightChannelPtr are either smart or raw pointers.
 */
template<typename LeftChannelPtr, typename RightChannelPtr>
// requires MoveConstructible<LeftChannelPtr> && MoveConstructible<RightChannelPtr>
std::unique_ptr<AsyncChannelBridgeImpl<LeftChannelPtr, RightChannelPtr>>
    makeAsyncChannelBridge(LeftChannelPtr leftChannel, RightChannelPtr rightChannel)
{
    return std::make_unique<AsyncChannelBridgeImpl<LeftChannelPtr, RightChannelPtr>>(
        std::move(leftChannel),
        std::move(rightChannel));
}

/**
 * Builds bridge that reads from source and writes to the destination.
 * AsyncChannelBridge behavior applies.
 * SourcePtr and DestinationPtr type requirements:
 * - meet the requirements of MoveConstructible
 * - implement aio::BasicPollable
 * - source must have AbstractAsyncChannel::readSomeAsync and AbstractAsyncChannel::cancelRead
 * - destination must have AbstractAsyncChannel::sendAsync and AbstractAsyncChannel::cancelWrite
 * NOTE: SourcePtr and DestinationPtr are either smart or raw pointers.
 */
template<typename SourcePtr, typename DestinationPtr>
// requires MoveConstructible<SourcePtr> && MoveConstructible<DestinationPtr>
std::unique_ptr<detail::AsyncChannelUnidirectionalBridgeImpl<SourcePtr, DestinationPtr>>
    makeAsyncChannelUnidirectionalBridge(
        SourcePtr source,
        DestinationPtr destination)
{
    return std::make_unique<detail::AsyncChannelUnidirectionalBridgeImpl<SourcePtr, DestinationPtr>>(
        std::move(source),
        std::move(destination));
}

} // namespace nx::network::aio
