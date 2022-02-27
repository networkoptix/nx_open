// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <type_traits>

#include <nx/utils/std/cpp14.h>

#include "abstract_async_channel.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Adapts type Adaptee to AbstractAsyncChannel.
 * @param AdapteePtr can be raw pointer or smart pointer.
 * In latter case AsyncChannelAdapter takes adaptee ownership.
 */
template <typename AdapteePtr>
class AsyncChannelAdapter:
    public AbstractAsyncChannel
{
    using base_type = AbstractAsyncChannel;

public:
    AsyncChannelAdapter(AdapteePtr adaptee):
        m_adaptee(std::move(adaptee))
    {
        bindToAioThread(m_adaptee->getAioThread());
    }

    virtual void bindToAioThread(AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_adaptee->bindToAioThread(aioThread);
    }

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) override
    {
        m_adaptee->readSomeAsync(buffer, std::move(handler));
    }

    virtual void sendAsync(
        const nx::Buffer* buffer,
        IoCompletionHandler handler) override
    {
        m_adaptee->sendAsync(buffer, std::move(handler));
    }

    const AdapteePtr& adaptee() const
    {
        return m_adaptee;
    }

protected:
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) override
    {
        m_adaptee->cancelIOSync(eventType);
    }

private:
    AdapteePtr m_adaptee;

    virtual void stopWhileInAioThread() override
    {
        if (m_adaptee)
        {
            m_adaptee->pleaseStopSync();
            m_adaptee = nullptr;
        }
    }
};

template <typename AdapteePtr>
std::unique_ptr<AbstractAsyncChannel> makeAsyncChannelAdapter(AdapteePtr adaptee)
{
    using Adapter = AsyncChannelAdapter<std::decay_t<AdapteePtr>>;
    return std::make_unique<Adapter>(std::move(adaptee));
}

} // namespace aio
} // namespace network
} // namespace nx
