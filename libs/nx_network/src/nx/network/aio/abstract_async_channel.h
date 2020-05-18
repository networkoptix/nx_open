#pragma once

#include <cstddef>
#include <memory>

#include <nx/utils/thread/cf/cfuture.h>

#include "basic_pollable.h"
#include "event_type.h"
#include "../buffer.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Interface for any entity that support asynchronous read/write operations.
 */
class NX_NETWORK_API AbstractAsyncChannel:
    public BasicPollable
{
public:
    virtual ~AbstractAsyncChannel() override = default;

    virtual void readSomeAsync(
        nx::Buffer* const buffer,
        IoCompletionHandler handler) = 0;

    virtual cf::future<std::size_t> readSome(nx::Buffer* buffer);

    virtual void sendAsync(
        const nx::Buffer& buffer,
        IoCompletionHandler handler) = 0;

    virtual cf::future<std::size_t> send(const nx::Buffer& buffer);

    /**
     * Cancel async socket operation. cancellationDoneHandler is invoked when cancelled.
     * @param eventType event to cancel.
     */
    virtual void cancelIOAsync(
        nx::network::aio::EventType eventType,
        nx::utils::MoveOnlyFunc<void()> handler) final;

    cf::future<cf::unit> cancelIO(nx::network::aio::EventType eventType);

    /**
     * Does not block if called within object's AIO thread.
     * If called from any other thread then returns after asynchronous handler completion.
     */
    virtual void cancelIOSync(nx::network::aio::EventType eventType) final;
    virtual void cancelIoInAioThread(nx::network::aio::EventType eventType) = 0;
};

using AsyncChannelPtr = std::unique_ptr<nx::network::aio::AbstractAsyncChannel>;

} // namespace aio
} // namespace network
} // namespace nx
