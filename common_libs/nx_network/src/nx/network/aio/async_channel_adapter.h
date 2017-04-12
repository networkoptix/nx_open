#pragma once

#include <memory>

#include <nx/utils/std/cpp14.h>

#include "abstract_async_channel.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Adapts type Adaptee to AbstractAsyncChannel.
 */
template <typename Adaptee>
class AsyncChannelAdapter:
    public AbstractAsyncChannel
{
    using base_type = AbstractAsyncChannel;

public:
    AsyncChannelAdapter(Adaptee* adaptee):
        m_adaptee(adaptee)
    {
        bindToAioThread(m_adaptee->getAioThread());
    }

    virtual ~AsyncChannelAdapter() override
    {
        if (isInSelfAioThread())
            stopWhileInAioThread();
    }

    virtual void bindToAioThread(AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_adaptee->bindToAioThread(aioThread);
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        m_adaptee->readSomeAsync(buffer, std::move(handler));
    }

    virtual void sendAsync(
        const nx::Buffer& buffer,
        std::function<void(SystemError::ErrorCode, size_t)> handler) override
    {
        m_adaptee->sendAsync(buffer, std::move(handler));
    }

    virtual void cancelIOSync(nx::network::aio::EventType eventType) override
    {
        m_adaptee->cancelIOSync(eventType);
    }

private:
    Adaptee* m_adaptee;

    virtual void stopWhileInAioThread() override
    {
        if (m_adaptee)
        {
            m_adaptee->pleaseStopSync();
            m_adaptee = nullptr;
        }
    }
};

template <typename Adaptee>
std::unique_ptr<AbstractAsyncChannel> makeAsyncChannelAdapter(Adaptee* adaptee)
{
    using Adapter = AsyncChannelAdapter<Adaptee>;
    return std::make_unique<Adapter>(adaptee);
}

} // namespace aio
} // namespace network
} // namespace nx
