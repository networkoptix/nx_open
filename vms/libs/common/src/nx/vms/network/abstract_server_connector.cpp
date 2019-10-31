#include "abstract_server_connector.h"

#include <common/common_module.h>
#include <network/router.h>

namespace nx::vms::network {

cf::future<AbstractServerConnector::Connection> AbstractServerConnector::connect(
    const QnUuid& serverId,
    std::chrono::milliseconds timeout,
    bool sslRequired)
{
    const auto route = commonModule()->router()->routeTo(serverId);
    return connect(route, timeout, sslRequired);
}

} // namespace nx::vms::network
