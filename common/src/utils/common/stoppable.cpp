#include "stoppable.h"

#include <nx/network/socket_global.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/mutex_lock_analyzer.h>

void QnStoppableAsync::pleaseStopSync(bool checkForLocks)
{
    #ifdef USE_OWN_MUTEX
        if (checkForLocks)
            MutexLockAnalyzer::instance()->expectNoLocks();
    #else
        static_cast<void>(checkForLocks); // unused
    #endif

    NX_ASSERT(!nx::network::SocketGlobals::aioService().isInAnyAioThread());

    nx::utils::promise<void> promise;
    auto fut = promise.get_future();
    pleaseStop( [&](){ promise.set_value(); } );
    fut.wait();
}
