#pragma once

#include <memory>

#include <nx/utils/async_operation_guard.h>

#include "basic_pollable.h"

namespace nx {
namespace network {
namespace aio {

/**
 * Invokes functor asynchronously in a specific aio-thread.
 * Aio thread is selected randomly during instantiation.
 */
class NX_NETWORK_API AsyncCall
{
public:
    AsyncCall();
    /**
     * Blocks if there is a async call running in another thread.
     * Posted calls not yet started are silently cancelled.
     */
    ~AsyncCall();

    template<typename Func>
    void post(Func func)
    {
        m_aioObject->post(
            [func = std::move(func),
                guard = m_asyncTransactionDeliveryGuard.sharedGuard()]() mutable
            {
                const auto lock = guard->lock();
                if (!lock)
                    return;

                func();
            });
    }

private:
    std::shared_ptr<network::aio::BasicPollable> m_aioObject;
    nx::utils::AsyncOperationGuard m_asyncTransactionDeliveryGuard;

    void cancelPostedAsyncCalls();
    void removeAioObjectDelayed();
};

} // namespace aio
} // namespace network
} // namespace nx
