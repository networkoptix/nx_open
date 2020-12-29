#include "async_stoppable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

#include "socket_global.h"

namespace nx {
namespace network {

void QnStoppableAsync::pleaseStopSync()
{
    const nx::network::aio::AIOService* aioService = nullptr;
    if (nx::network::SocketGlobals::isInitialized())
        aioService = &nx::network::SocketGlobals::aioService();
    pleaseStopSync(aioService);
}

void QnStoppableAsync::pleaseStopSync(const nx::network::aio::AIOService* aioService)
{
    if (aioService)
    {
        NX_CRITICAL(!aioService->isInAnyAioThread());
    }

    nx::utils::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop([&]() { promise.set_value(); });
    fut.wait();
}

} // namespace network
} // namespace nx
