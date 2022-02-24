// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <list>

#include <nx/utils/move_only_func.h>
#include <nx/utils/log/assert.h>

namespace nx::network::aio::detail {

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

/**
 * Reads from source and writes to destination.
 * Always seeks to load source and destination as much as possible.
 * NOTE: SourcePtr and DestinationPtr must follow AbstractAsyncChannel API.
 * NOTE: Intermediate buffer that allows concurrent read/writes can be adjusted
 *   with AsyncChannelUnidirectionalBridge::setMaxSendQueueSizeBytes.
 */
class AsyncChannelUnidirectionalBridge:
    public BasicPollable
{
public:
    virtual ~AsyncChannelUnidirectionalBridge() = default;

    virtual void setReadBufferSize(std::size_t readBufferSize) = 0;
    virtual void setMaxSendQueueSizeBytes(std::size_t maxSendQueueSizeBytes) = 0;
    virtual void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> doneHandler) = 0;
    virtual void setOnSomeActivity(nx::utils::MoveOnlyFunc<void()> handler) = 0;
    virtual bool isSendQueueEmpty() const = 0;
};

/**
 * SourcePtr and DestinationPtr are either smart or raw pointers.
 */
template<typename SourcePtr, typename DestinationPtr>
class AsyncChannelUnidirectionalBridgeImpl:
    public AsyncChannelUnidirectionalBridge
{
    using base_type = AsyncChannelUnidirectionalBridge;

public:
    AsyncChannelUnidirectionalBridgeImpl(
        SourcePtr source,
        DestinationPtr destination)
        :
        m_source(std::move(source)),
        m_destination(std::move(destination)),
        m_readBufferSize(16*1024),
        m_maxSendQueueSizeBytes(128*1024),
        m_isReading(false),
        m_isSourceOpened(true),
        m_sourceClosureReason(SystemError::noError)
    {
        m_readBuffer.reserve((int)m_readBufferSize);
    }

    virtual void bindToAioThread(AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);

        if (m_source)
            m_source->bindToAioThread(aioThread);

        if (m_destination)
            m_destination->bindToAioThread(aioThread);
    }

    virtual void setReadBufferSize(std::size_t readBufferSize) override
    {
        m_readBufferSize = readBufferSize;
        m_readBuffer.reserve((int)m_readBufferSize);
    }

    virtual void setMaxSendQueueSizeBytes(std::size_t maxSendQueueSizeBytes) override
    {
        m_maxSendQueueSizeBytes = maxSendQueueSizeBytes;
    }

    virtual void start(nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> doneHandler) override
    {
        NX_ASSERT(m_source->getAioThread() == m_destination->getAioThread());

        m_onDoneHandler = std::move(doneHandler);
        scheduleRead();
    }

    virtual void setOnSomeActivity(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_onSomeActivityHander = std::move(handler);
    }

    virtual bool isSendQueueEmpty() const override
    {
        return m_sendQueue.empty();
    }

protected:
    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();

        m_source = nullptr;
        m_destination = nullptr;
    }

private:
    enum class ChannelState
    {
        opened,
        closed,
    };

    SourcePtr m_source;
    DestinationPtr m_destination;
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
        NX_ASSERT(m_isSourceOpened);

        m_source->readSomeAsync(
            &m_readBuffer,
            [this](auto&&... args) { onDataRead(std::move(args)...); });
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
            m_sourceClosureReason = sysErrorCode;

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
        m_destination->sendAsync(
            &m_sendQueue.front(),
            [this](auto&&... args) { onDataSent(std::move(args)...); });
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
        if (m_isSourceOpened && !m_isReading)
            scheduleRead();
    }

    void reportFailure(SystemError::ErrorCode sysErrorCode)
    {
        m_source->cancelRead();
        m_destination->cancelWrite();

        m_onDoneHandler(sysErrorCode);
    }
};

} // namespace nx::network::aio::detail
