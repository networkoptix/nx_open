#include "async_stoppable.h"

#include <nx/network/aio/aio_service.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

#include "socket_global.h"

void QnStoppableAsync::pleaseStopSync(bool checkForLocks)
{
    #ifdef USE_OWN_MUTEX
        if (checkForLocks)
            MutexLockAnalyzer::instance()->expectNoLocks();
    #else
        static_cast<void>(checkForLocks); // unused
    #endif

    if (nx::network::SocketGlobals::isInitialized())
    {
        NX_CRITICAL(!nx::network::SocketGlobals::aioService().isInAnyAioThread());
    }

    nx::utils::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop( [&](){ promise.set_value(); } );
    fut.wait();
}
