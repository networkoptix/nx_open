#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include "aio/basic_pollable.h"
#include "detail/async_channel_unidirectional_bridge.h"

namespace nx {
namespace network {

class AsyncChannelBridge:
    public aio::BasicPollable
{
public:
    static constexpr std::size_t kDefaultReadBufferSize = 16 * 1024;
    static constexpr std::size_t kDefaultMaxSendQueueSizeBytes = 128 * 1024;

    virtual ~AsyncChannelBridge() override = default;

    virtual void start() = 0;
    virtual void setOnDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) = 0;
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

    template<typename Source, typename Destination>
    void initializeOneWayChannel(
        std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<Source, Destination>>* channel,
        Source* source,
        Destination* destination)
    {
        *channel =
            std::make_unique<detail::AsyncChannelUnidirectionalBridge<Source, Destination>>(
                *source,
                *destination,
                AsyncChannelBridge::kDefaultReadBufferSize,
                AsyncChannelBridge::kDefaultMaxSendQueueSizeBytes);
        (*channel)->setOnDone(
            [this](SystemError::ErrorCode sysErrorCode)
            {
                handleChannelClosure(sysErrorCode);
            });
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
    }

    virtual void start() override
    {
        m_leftToRight->start();
        m_rightToLeft->start();
    }

    virtual void setOnDone(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override
    {
        m_onDoneHandler = std::move(handler);
    }

private:
    std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<LeftFile, RightFile>> m_leftToRight;
    std::unique_ptr<detail::AsyncChannelUnidirectionalBridge<RightFile, LeftFile>> m_rightToLeft;

    LeftFile m_leftFile;
    RightFile m_rightFile;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_onDoneHandler;
    int m_closedChannelCount;

    virtual void stopWhileInAioThread() override
    {
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

    void handleChannelClosure(SystemError::ErrorCode reason)
    {
        ++m_closedChannelCount;
        if (m_closedChannelCount < 2)
            return;
        reportFailure(reason);
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

} // namespace network
} // namespace nx
