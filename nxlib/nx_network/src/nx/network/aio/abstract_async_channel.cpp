#include "abstract_async_channel.h"

#include <future>

namespace nx {
namespace network {
namespace aio {

void AbstractAsyncChannel::cancelIOAsync(
    nx::network::aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    this->post(
        [this, eventType, handler = std::move(handler)]()
        {
            cancelIoInAioThread(eventType);
            handler();
        });
}

void AbstractAsyncChannel::cancelIOSync(nx::network::aio::EventType eventType)
{
    if (isInSelfAioThread())
    {
        cancelIoInAioThread(eventType);
    }
    else
    {
        std::promise<void> promise;
        post(
            [this, eventType, &promise]()
            {
                cancelIoInAioThread(eventType);
                promise.set_value();
            });
        promise.get_future().wait();
    }
}

} // namespace aio
} // namespace network
} // namespace nx
