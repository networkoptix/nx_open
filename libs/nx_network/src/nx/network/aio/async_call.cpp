#include "async_call.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace network {
namespace aio {

AsyncCall::AsyncCall():
    m_aioObject(std::make_shared<network::aio::BasicPollable>())
{
}

AsyncCall::~AsyncCall()
{
    cancelPostedAsyncCalls();
    removeAioObjectDelayed();
}

void AsyncCall::cancelPostedAsyncCalls()
{
    m_asyncTransactionDeliveryGuard.reset();
}

void AsyncCall::removeAioObjectDelayed()
{
    decltype(m_aioObject) aioObject;
    aioObject.swap(m_aioObject);
    network::aio::BasicPollable* aioObjectPtr = aioObject.get();

    aioObjectPtr->post(
        [aioObject = std::move(aioObject)]() mutable
        {
            NX_ASSERT(aioObject.use_count() == 1);
            aioObject.reset();
        });
}

} // namespace aio
} // namespace network
} // namespace nx
