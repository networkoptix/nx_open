#include "client_time_sync_manager.h"

#include <common/common_module.h>
#include <network/router.h>

namespace nx {
namespace time_sync {

void ClientTimeSyncManager::updateTime()
{
    auto route = commonModule()->router()->routeTo(commonModule()->remoteGUID());
    if (route.isValid())
        loadTimeFromServer(route);
}

} // namespace time_sync
} // namespace nx
