#include "async_stoppable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

#include "socket_global.h"

void QnStoppableAsync::pleaseStopSync(bool checkForLocks)
{
    const nx::network::aio::AIOService* aioService = nullptr;
    if (nx::network::SocketGlobals::isInitialized())
        aioService = &nx::network::SocketGlobals::aioService();
    pleaseStopSync(aioService, checkForLocks);
}

void QnStoppableAsync::pleaseStopSync(
    const nx::network::aio::AIOService* aioService,
    bool checkForLocks)
{
    #ifdef USE_OWN_MUTEX
        if (checkForLocks)
            MutexLockAnalyzer::instance()->expectNoLocks();
    #else
        static_cast<void>(checkForLocks); // unused
    #endif

    if (aioService)
    {
        NX_CRITICAL(!aioService->isInAnyAioThread());
    }

    nx::utils::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop([&]() { promise.set_value(); });
    fut.wait();
}
