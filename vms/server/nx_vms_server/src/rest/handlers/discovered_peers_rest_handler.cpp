#include "discovered_peers_rest_handler.h"

#include <common/common_module.h>
#include <nx/vms/server/discovery/discovery_monitor.h>
#include <network/tcp_connection_priv.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/discovery/manager.h>
#include <rest/server/rest_connection_processor.h>

int QnDiscoveredPeersRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    result.setReply(nx::vms::server::discovery::getServers(owner->commonModule()->moduleDiscoveryManager()));
    return nx::network::http::StatusCode::ok;
}
