#include "abstract_async_channel.h"

#include <future>
#include <utility>
#include <system_error>

#include <nx/utils/general_macros.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {
namespace aio {

namespace {

template <typename Func, typename Buffer>
cf::future<std::size_t> futurizeIO(Func func, AbstractAsyncChannel* channel, const Buffer& buffer)
{
    cf::promise<std::pair<SystemError::ErrorCode, std::size_t>> promise;
    auto future = promise.get_future();

    func(*channel, buffer,
        [promise = std::move(promise)](auto errorCode, auto transferredSize) mutable
        {
            promise.set_value(std::make_pair(errorCode, transferredSize));
        });

    return future
        .then(cf::translate_broken_promise_to_operation_canceled)
        .then_unwrap(
            [](auto result) mutable
            {
                auto [errorCode, transferredSize] = result;

                if (errorCode)
                    throw std::system_error(errorCode, std::system_category());

                return transferredSize;
            });
}

} // namespace

cf::future<std::size_t> AbstractAsyncChannel::readSome(Buffer* buffer)
{
    return futurizeIO(NX_WRAP_MEM_FUNC_TO_LAMBDA(readSomeAsync), this, buffer);
}

cf::future<std::size_t> AbstractAsyncChannel::send(const Buffer& buffer)
{
    return futurizeIO(NX_WRAP_MEM_FUNC_TO_LAMBDA(sendAsync), this, buffer);
}

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

cf::future<cf::unit> AbstractAsyncChannel::cancelIO(EventType eventType)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();
    cancelIOAsync(eventType,
        [promise = std::move(promise)]() mutable { promise.set_value(cf::unit()); });
    return future
        .then(cf::translate_broken_promise_to_operation_canceled);
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
